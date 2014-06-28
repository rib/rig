/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <config.h>

#ifdef RUT_ENABLE_REFCOUNT_DEBUG

#include <clib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef RUT_ENABLE_BACKTRACE
#include <execinfo.h>
#include <unistd.h>
#endif /* RUT_ENABLE_BACKTRACE */

#include "rut-refcount-debug.h"
#include "rut-list.h"
#include "rut-object.h"
#include "rut-util.h"

typedef struct
{
  bool enabled;
  GHashTable *hash;
  c_list_t *owners;
  int backtrace_level;
} RutRefcountDebugState;

typedef struct
{
  const char *name;
  void *object;
  int data_ref;
  int object_ref_count;
  int n_claims;
  RutList actions;
} RutRefcountDebugObject;

typedef enum
{
  RUT_REFCOUNT_DEBUG_ACTION_TYPE_CREATE,
  RUT_REFCOUNT_DEBUG_ACTION_TYPE_FREE,
  RUT_REFCOUNT_DEBUG_ACTION_TYPE_REF,
  RUT_REFCOUNT_DEBUG_ACTION_TYPE_UNREF,
  RUT_REFCOUNT_DEBUG_ACTION_TYPE_CLAIM,
  RUT_REFCOUNT_DEBUG_ACTION_TYPE_RELEASE
} RutRefcountDebugActionType;

typedef struct
{
  RutList link;
  RutRefcountDebugActionType type;

  /* for _CLAIM/_RELEASE actions... */
  RutRefcountDebugObject *owner;

  int n_backtrace_addresses;

  /* This array is over-allocated to incorporate enough space for
   * state->backtrace_level */
  void *backtrace_addresses[1];
} RutRefcountDebugAction;

static void
destroy_tls_state_cb (void *tls_data);

static void
object_data_unref (RutRefcountDebugObject *object_data);

static RutRefcountDebugState *
get_state (void);

static size_t
get_sizeof_action (RutRefcountDebugState *state)
{
  return (offsetof (RutRefcountDebugAction, backtrace_addresses) +
          sizeof (void *) * state->backtrace_level);
}

static void
free_action (RutRefcountDebugAction *action)
{
  if (action->owner)
    object_data_unref (action->owner);
  c_slice_free1 (get_sizeof_action (get_state ()), action);
}

static void
free_action_log (RutRefcountDebugObject *object_data)
{
  RutRefcountDebugAction *action, *tmp;

  rut_list_for_each_safe (action, tmp, &object_data->actions, link)
    free_action (action);
}

static RutRefcountDebugObject *
object_data_ref (RutRefcountDebugObject *object_data)
{
  object_data->data_ref++;
  return object_data;
}

static void
object_data_unref (RutRefcountDebugObject *object_data)
{
  if (--object_data->data_ref <= 0)
    {
      free_action_log (object_data);

      c_slice_free (RutRefcountDebugObject, object_data);
    }
}

static void
log_action (RutRefcountDebugObject *object_data,
            RutRefcountDebugActionType action_type,
            RutRefcountDebugObject *owner)
{
  RutRefcountDebugState *state = get_state ();
  RutRefcountDebugAction *action = c_slice_alloc (get_sizeof_action (state));

  action->type = action_type;

  if (owner)
    action->owner = object_data_ref (owner);

#ifdef RUT_ENABLE_BACKTRACE
  {
    if (state->backtrace_level == 0)
      action->n_backtrace_addresses = 0;
    else
      action->n_backtrace_addresses =
        backtrace (action->backtrace_addresses, state->backtrace_level);
  }
#else /* RUT_ENABLE_BACKTRACE */
  {
    action->n_backtrace_addresses = 0;
  }
#endif /* RUT_ENABLE_BACKTRACE */

  rut_list_insert (object_data->actions.prev, &action->link);
}

static void
object_data_destroy_cb (void *data)
{
  object_data_unref (data);
}

static SDL_TLSID tls;

void
rut_refcount_debug_init (void)
{
  tls = SDL_TLSCreate ();
}

static RutRefcountDebugState *
get_state (void)
{
  RutRefcountDebugState *state = SDL_TLSGet (tls);

  if (state == NULL)
    {
      state = c_new0 (RutRefcountDebugState, 1);

      state->hash = g_hash_table_new_full (g_direct_hash,
                                           g_direct_equal,
                                           NULL, /* key destroy */
                                           object_data_destroy_cb);

      state->enabled =
        !rut_util_is_boolean_env_set ("RUT_DISABLE_REFCOUNT_DEBUG");

#ifdef RUT_ENABLE_BACKTRACE
      {
        const char *backtrace_level = g_getenv ("RUT_BACKTRACE_LEVEL");

        if (backtrace_level)
          {
            state->backtrace_level = atoi (backtrace_level);
            state->backtrace_level = CLAMP (state->backtrace_level, 0, 1024);
          }
        else
          state->backtrace_level = 0;
      }
#else /* RUT_ENABLE_BACKTRACE */
      {
        state->backtrace_level = 0;
      }
#endif /* RUT_ENABLE_BACKTRACE */

      SDL_TLSSet (tls, state, destroy_tls_state_cb);
    }

  return state;
}

#if RUT_ENABLE_BACKTRACE

static char *
readlink_alloc (const char *linkname)
{
  int buf_size = 32;

  while (true)
    {
      char *buf = c_malloc (buf_size);
      int got = readlink (linkname, buf, buf_size - 1);

      if (got < 0)
        {
          c_free (buf);
          return NULL;
        }
      else if (got < buf_size - 1)
        {
          buf[got] = '\0';
          return buf;
        }
      else
        {
          c_free (buf);
          buf_size *= 2;
        }
    }
}

static bool
resolve_addresses_addr2line (GHashTable *hash_table,
                             int n_addresses,
                             void * const *addresses)
{
  const char *base_args[] =
    { "addr2line", "-f", "-e" };
  char *addr_out;
  char **argv;
  int exit_status;
  int extra_args = G_N_ELEMENTS (base_args);
  int address_args = extra_args + 1;
  bool ret = true;
  int i;

  argv = g_alloca (sizeof (char *) *
                   (n_addresses + address_args + 1));
  memcpy (argv, base_args, sizeof (base_args));

  argv[extra_args] = readlink_alloc ("/proc/self/exe");
  if (argv[extra_args] == NULL)
    ret = false;
  else
    {
      for (i = 0; i < n_addresses; i++)
        argv[i + address_args] = c_strdup_printf ("%p", addresses[i]);
      argv[address_args + n_addresses] = NULL;

      if (g_spawn_sync (NULL, /* working_directory */
                        argv,
                        NULL, /* envp */
                        G_SPAWN_STDERR_TO_DEV_NULL |
                        G_SPAWN_SEARCH_PATH,
                        NULL, /* child_setup */
                        NULL, /* user_data for child_setup */
                        &addr_out,
                        NULL, /* standard_error */
                        &exit_status,
                        NULL /* error */) &&
          exit_status == 0)
        {
          int addr_num;
          char **lines = g_strsplit (addr_out, "\n", 0);
          char **line;

          for (line = lines, addr_num = 0;
               line[0] && line[1];
               line += 2, addr_num++)
            {
              char *result = c_strdup_printf ("%s (%s)", line[1], line[0]);
              g_hash_table_insert (hash_table, addresses[addr_num], result);
            }

          g_strfreev (lines);
        }
      else
        ret = false;

      for (i = 0; i < n_addresses; i++)
        c_free (argv[i + address_args]);
      c_free (argv[extra_args]);
    }

  return ret;
}

static bool
resolve_addresses_backtrace (GHashTable *hash_table,
                             int n_addresses,
                             void * const *addresses)
{
  char **symbols;
  int i;

  symbols = backtrace_symbols (addresses,
                               n_addresses);

  for (i = 0; i < n_addresses; i++)
    g_hash_table_insert (hash_table,
                         addresses[i],
                         c_strdup (symbols[i]));

  free (symbols);

  return true;
}

static void
add_addresses_cb (void *key,
                  void *value,
                  void *user_data)
{
  GHashTable *hash_table = user_data;
  RutRefcountDebugObject *object_data = value;
  RutRefcountDebugAction *action;

  rut_list_for_each (action, &object_data->actions, link)
    {
      int i;

      for (i = 0; i < action->n_backtrace_addresses; i++)
        g_hash_table_insert (hash_table, action->backtrace_addresses[i], NULL);
    }
}

static void
get_addresses_cb (void *key,
                  void *value,
                  void *user_data)
{
  void ***addr_p = user_data;

  *((*addr_p)++) = key;
}

static GHashTable *
resolve_addresses (RutRefcountDebugState *state)
{
  GHashTable *hash_table;
  int n_addresses;

  hash_table = g_hash_table_new_full (g_direct_hash,
                                      g_direct_equal,
                                      NULL, /* key destroy */
                                      c_free /* value destroy */);

  g_hash_table_foreach (state->hash, add_addresses_cb, hash_table);

  n_addresses = g_hash_table_size (hash_table);

  if (n_addresses >= 1)
    {
      void **addresses = c_malloc (sizeof (void *) * n_addresses);
      void **addr_p = addresses;
      bool resolve_ret;

      g_hash_table_foreach (hash_table, get_addresses_cb, &addr_p);

      resolve_ret = (resolve_addresses_addr2line (hash_table,
                                                  n_addresses,
                                                  addresses) ||
                     resolve_addresses_backtrace (hash_table,
                                                  n_addresses,
                                                  addresses));

      c_free (addresses);

      if (!resolve_ret)
        {
          g_hash_table_destroy (hash_table);
          return NULL;
        }
    }

  return hash_table;
}

#endif /* RUT_ENABLE_BACKTRACE */

typedef struct
{
  RutRefcountDebugState *state;
  FILE *out_file;
  GHashTable *address_table;
} DumpObjectCallbackData;


static void
dump_object_cb (RutRefcountDebugObject *object_data,
                void *user_data)

{
  DumpObjectCallbackData *data = user_data;

  fprintf (data->out_file,
           "Object: ptr=%p, id=%p, type=%s, ref_count=%i",
           object_data->object,
           object_data,
           object_data->name,
           object_data->object_ref_count);

  fputc ('\n', data->out_file);

#if RUT_ENABLE_BACKTRACE
  if (data->address_table)
    {
      RutRefcountDebugAction *action;
      int ref_count = 0;

      rut_list_for_each (action, &object_data->actions, link)
        {
          int i;

          fputc (' ', data->out_file);
          switch (action->type)
            {
            case RUT_REFCOUNT_DEBUG_ACTION_TYPE_CREATE:
              fprintf (data->out_file, "CREATE: ref_count = %i", ++ref_count);
              break;
            case RUT_REFCOUNT_DEBUG_ACTION_TYPE_FREE:
              fprintf (data->out_file, "FREE: ref_count = %i", ++ref_count);
              break;
            case RUT_REFCOUNT_DEBUG_ACTION_TYPE_REF:
              fprintf (data->out_file, "REF: ref_count = %i", ++ref_count);
              break;
            case RUT_REFCOUNT_DEBUG_ACTION_TYPE_UNREF:
              fprintf (data->out_file, "UNREF: ref_count = %i", --ref_count);
              break;
            case RUT_REFCOUNT_DEBUG_ACTION_TYPE_CLAIM:
              fprintf (data->out_file, "CLAIM: ref_count = %i, "
                       "Owner: ptr=%p,id=%p,type=%s",
                       ++ref_count,
                       action->owner->object,
                       action->owner,
                       action->owner->name);
              break;
            case RUT_REFCOUNT_DEBUG_ACTION_TYPE_RELEASE:
              fprintf (data->out_file, "RELEASE: ref_count = %i, "
                       "Owner: ptr=%p,id=%p,type=%s",
                       --ref_count,
                       action->owner->object,
                       action->owner,
                       action->owner->name);
              break;
            }
          fputc ('\n', data->out_file);

          for (i = 0; i < action->n_backtrace_addresses; i++)
            {
              char *addr = g_hash_table_lookup (data->address_table,
                                                action->backtrace_addresses[i]);
              fprintf (data->out_file, "  %s\n", addr);
            }
        }
    }
#endif /* RUT_ENABLE_BACKTRACE */
}

static void
dump_hash_object_cb (void *key,
                     void *value,
                     void *user_data)
{
  dump_object_cb (value, user_data);
}

static void
destroy_tls_state_cb (void *tls_data)
{
  RutRefcountDebugState *state = tls_data;
  int size = g_hash_table_size (state->hash);

  if (size > 0)
    {
      //char *thread_name = SDL_GetThreadName ();
      char *thread_name = c_strdup_printf ("thread-%lu", SDL_ThreadID ());
      char *filename = g_strconcat ("rut-object-log-", thread_name, ".txt", NULL);
      char *out_name =
        g_build_filename (g_get_tmp_dir (), filename, NULL);
      DumpObjectCallbackData data;

      if (size == 1)
        c_warning ("%s: One object was leaked", thread_name);
      else
        c_warning ("%s: %i objects were leaked", thread_name, size);

      data.state = state;
      data.out_file = fopen (out_name, "w");

      if (data.out_file == NULL)
        {
          c_warning ("Error saving refcount log: %s", strerror (errno));
        }
      else
        {
#ifdef RUT_ENABLE_BACKTRACE
          if (state->backtrace_level > 0)
            data.address_table = resolve_addresses (state);
          else
#endif
            data.address_table = NULL;

          g_hash_table_foreach (state->hash, dump_hash_object_cb, &data);
          c_list_foreach (state->owners, (GFunc)dump_object_cb, &data);

          if (data.address_table)
            g_hash_table_destroy (data.address_table);

          if (ferror (data.out_file))
            c_warning ("Error saving refcount log: %s", strerror (errno));
          else
            {
              c_warning ("Refcount log saved to %s", out_name);

              if (state->backtrace_level <= 0)
                c_warning ("Set RUT_BACKTRACE_LEVEL to a non-zero value "
                           "to include bactraces in the log");
            }

          fclose (data.out_file);
        }

      c_free (out_name);
    }

  c_list_foreach (state->owners, (GFunc)object_data_unref, NULL);
  c_list_free (state->owners);

  g_hash_table_destroy (state->hash);
  c_free (state);
}

void
_rut_refcount_debug_object_created (void *object)
{
  RutRefcountDebugState *state = get_state ();

  if (!state->enabled)
    return;

  if (g_hash_table_contains (state->hash, object))
    c_warning ("Address of existing object reused for newly created object");
  else
    {
      RutRefcountDebugObject *object_data =
        c_slice_new (RutRefcountDebugObject);

      /* The object data might outlive the lifetime of the
       * object itself so lets find out the object type now
       * while we can... */
      object_data->name = rut_object_get_type_name (object);

      object_data->object = object;

      /* NB This debug object data may out live the object itself
       * since we track relationships between objects and so if there
       * are other objects that are owned by this object we will keep
       * the object data for the owner. If the owner forgets to
       * release the references it claimed then we want to maintain
       * the graph of ownership for debugging. */

      object_data->data_ref = 1; /* for the object_data itself */
      object_data->object_ref_count = 1;
      object_data->n_claims = 0;
      rut_list_init (&object_data->actions);
      log_action (object_data, RUT_REFCOUNT_DEBUG_ACTION_TYPE_CREATE, NULL);

      g_hash_table_insert (state->hash, object, object_data);
      object_data_ref (object_data);
    }
}

void
_rut_refcount_debug_claim (void *object, void *owner)
{
  RutRefcountDebugState *state = get_state ();
  RutRefcountDebugObject *object_data;

  if (!state->enabled)
    return;

  object_data = g_hash_table_lookup (state->hash, object);

  if (object_data == NULL)
    {
      c_warning ("Reference taken on object that does not exist");
      return;
    }

  if (owner)
    {
      RutRefcountDebugObject *owner_data =
        g_hash_table_lookup (state->hash, owner);

      if (owner_data == NULL)
        c_warning ("Reference claimed by object that does not exist");

      if (owner_data->n_claims == 0)
        {
          state->owners = c_list_prepend (state->owners, owner_data);
          object_data_ref (owner_data);
          owner_data->n_claims++;
        }

      log_action (object_data, RUT_REFCOUNT_DEBUG_ACTION_TYPE_CLAIM,
                  owner_data);
    }
  else
    log_action (object_data, RUT_REFCOUNT_DEBUG_ACTION_TYPE_REF, NULL);

  object_data->object_ref_count++;
}

void
_rut_refcount_debug_ref (void *object)
{
  _rut_refcount_debug_claim (object, NULL /* owner */);
}

void
_rut_refcount_debug_release (void *object, void *owner)
{
  RutRefcountDebugState *state = get_state ();
  RutRefcountDebugObject *object_data;

  if (!state->enabled)
    return;

  object_data = g_hash_table_lookup (state->hash, object);

  if (object_data == NULL)
    {
      c_warning ("Reference removed on object that does not exist");
      return;
    }

  if (--object_data->object_ref_count <= 0)
    {
      if (object_data->object_ref_count != 0)
        {
          c_warning ("Reference less than zero but object still exists: "
                     "corrupt ref_count for object %p", object);
        }
      log_action (object_data, RUT_REFCOUNT_DEBUG_ACTION_TYPE_FREE, NULL);
      object_data->object = NULL;
      g_hash_table_remove (state->hash, object);
    }
  else
    {
      if (owner)
        {
          RutRefcountDebugObject *owner_data =
            g_hash_table_lookup (state->hash, object);

          if (owner_data)
            {
              owner_data->n_claims--;
              if (owner_data->n_claims == 0)
                {
                  state->owners = c_list_remove (state->owners, owner_data);
                  object_data_unref (owner_data);
                }
            }
          else
            c_warning ("Reference released by unknown owner");

          log_action (object_data, RUT_REFCOUNT_DEBUG_ACTION_TYPE_RELEASE,
                      owner_data);
        }
      else
        log_action (object_data, RUT_REFCOUNT_DEBUG_ACTION_TYPE_UNREF, NULL);
    }
}

void
_rut_refcount_debug_unref (void *object)
{
  _rut_refcount_debug_release (object, NULL /* owner */);
}

void
rut_object_dump_refs (void *object)
{
  RutRefcountDebugState *state = get_state ();
  RutRefcountDebugObject *object_data;
  DumpObjectCallbackData dump_data = {
    .state = state,
    .out_file = stdout,
    NULL /* address table */
  };

  if (!state->enabled)
    return;

  object_data = g_hash_table_lookup (state->hash, object);
  if (object_data == NULL)
    {
      c_print ("No reference information tracked for object %p\n", object);
      return;
    }

#ifdef RUT_ENABLE_BACKTRACE
  if (state->backtrace_level > 0)
    dump_data.address_table = resolve_addresses (state);
#endif

  dump_object_cb (object_data, &dump_data);
}
#endif /* RUT_ENABLE_REFCOUNT_DEBUG */

