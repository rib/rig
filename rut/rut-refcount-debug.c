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

#include <rut-config.h>

#ifdef RUT_ENABLE_REFCOUNT_DEBUG

#include <clib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(HAVE_BACKTRACE)
#define RUT_ENABLE_BACKTRACE
#endif

#ifdef RUT_ENABLE_BACKTRACE
#include <execinfo.h>
#include <unistd.h>
#endif /* RUT_ENABLE_BACKTRACE */

#include "rut-refcount-debug.h"
#include "rut-object.h"
#include "rut-util.h"

typedef struct {
    bool enabled;
    c_hash_table_t *hash;
    c_llist_t *owners;
} rut_refcount_debug_state_t;

typedef struct {
    const char *name;
    void *object;
    int data_ref;
    int object_ref_count;
    int n_claims;
    c_list_t actions;
} rut_refcount_debug_object_t;

typedef enum {
    RUT_REFCOUNT_DEBUG_ACTION_TYPE_CREATE,
    RUT_REFCOUNT_DEBUG_ACTION_TYPE_FREE,
    RUT_REFCOUNT_DEBUG_ACTION_TYPE_REF,
    RUT_REFCOUNT_DEBUG_ACTION_TYPE_UNREF,
    RUT_REFCOUNT_DEBUG_ACTION_TYPE_CLAIM,
    RUT_REFCOUNT_DEBUG_ACTION_TYPE_RELEASE
} rut_refcount_debug_action_type_t;

typedef struct {
    c_list_t link;
    rut_refcount_debug_action_type_t type;

    /* for _CLAIM/_RELEASE actions... */
    rut_refcount_debug_object_t *owner;

    c_backtrace_t *backtrace;
} rut_refcount_debug_action_t;

static void destroy_tls_state_cb(void *tls_data);

static void object_data_unref(rut_refcount_debug_object_t *object_data);

static rut_refcount_debug_state_t *get_state(void);

static void
free_action(rut_refcount_debug_action_t *action)
{
    if (action->owner)
        object_data_unref(action->owner);

#ifdef RUT_ENABLE_BACKTRACE
    if (action->backtrace)
        c_backtrace_free(action->backtrace);
#endif

    c_free(action);
}

static void
free_action_log(rut_refcount_debug_object_t *object_data)
{
    rut_refcount_debug_action_t *action, *tmp;

    c_list_for_each_safe(action, tmp, &object_data->actions, link)
    free_action(action);
}

static rut_refcount_debug_object_t *
object_data_ref(rut_refcount_debug_object_t *object_data)
{
    object_data->data_ref++;
    return object_data;
}

static void
object_data_unref(rut_refcount_debug_object_t *object_data)
{
    if (--object_data->data_ref <= 0) {
        free_action_log(object_data);

        c_slice_free(rut_refcount_debug_object_t, object_data);
    }
}

static void
log_action(rut_refcount_debug_object_t *object_data,
           rut_refcount_debug_action_type_t action_type,
           rut_refcount_debug_object_t *owner)
{
    rut_refcount_debug_state_t *state = get_state();
    rut_refcount_debug_action_t *action = c_malloc(sizeof(*action));

    action->type = action_type;

    if (owner)
        action->owner = object_data_ref(owner);

#ifdef RUT_ENABLE_BACKTRACE
    action->backtrace = c_backtrace_new();
#endif

    c_list_insert(object_data->actions.prev, &action->link);
}

static void
object_data_destroy_cb(void *data)
{
    object_data_unref(data);
}

static c_tls_t tls;

void
rut_refcount_debug_init(void)
{
    c_tls_init(&tls, destroy_tls_state_cb);
}

static rut_refcount_debug_state_t *
get_state(void)
{
    rut_refcount_debug_state_t *state = c_tls_get(&tls);

    if (state == NULL) {
        state = c_new0(rut_refcount_debug_state_t, 1);

        state->hash = c_hash_table_new_full(c_direct_hash,
                                            c_direct_equal,
                                            NULL, /* key destroy */
                                            object_data_destroy_cb);

        state->enabled =
            !rut_util_is_boolean_env_set("RUT_DISABLE_REFCOUNT_DEBUG");

        c_tls_set(&tls, state);
    }

    return state;
}

typedef struct {
    rut_refcount_debug_state_t *state;
    FILE *out_file;
} dump_object_callback_data_t;

static void
dump_object_cb(rut_refcount_debug_object_t *object_data,
               void *user_data)

{
    dump_object_callback_data_t *data = user_data;

    fprintf(data->out_file,
            "Object: ptr=%p, id=%p, type=%s, ref_count=%i",
            object_data->object,
            object_data,
            object_data->name,
            object_data->object_ref_count);

    fputc('\n', data->out_file);

#ifdef RUT_ENABLE_BACKTRACE
    {
        rut_refcount_debug_action_t *action;
        int ref_count = 0;

        c_list_for_each(action, &object_data->actions, link)
        {
            int n_frames = c_backtrace_get_n_frames(action->backtrace);
            char *symbols[n_frames];
            int i;

            fputc(' ', data->out_file);
            switch (action->type) {
            case RUT_REFCOUNT_DEBUG_ACTION_TYPE_CREATE:
                fprintf(data->out_file, "CREATE: ref_count = %i", ++ref_count);
                break;
            case RUT_REFCOUNT_DEBUG_ACTION_TYPE_FREE:
                fprintf(data->out_file, "FREE: ref_count = %i", ++ref_count);
                break;
            case RUT_REFCOUNT_DEBUG_ACTION_TYPE_REF:
                fprintf(data->out_file, "REF: ref_count = %i", ++ref_count);
                break;
            case RUT_REFCOUNT_DEBUG_ACTION_TYPE_UNREF:
                fprintf(data->out_file, "UNREF: ref_count = %i", --ref_count);
                break;
            case RUT_REFCOUNT_DEBUG_ACTION_TYPE_CLAIM:
                fprintf(data->out_file,
                        "CLAIM: ref_count = %i, "
                        "Owner: ptr=%p,id=%p,type=%s",
                        ++ref_count,
                        action->owner->object,
                        action->owner,
                        action->owner->name);
                break;
            case RUT_REFCOUNT_DEBUG_ACTION_TYPE_RELEASE:
                fprintf(data->out_file,
                        "RELEASE: ref_count = %i, "
                        "Owner: ptr=%p,id=%p,type=%s",
                        --ref_count,
                        action->owner->object,
                        action->owner,
                        action->owner->name);
                break;
            }
            fputc('\n', data->out_file);

            c_backtrace_get_frame_symbols(action->backtrace,
                                          symbols,
                                          n_frames);
            for (i = 0; i < n_frames; i++)
                fprintf(data->out_file, "  %s\n", symbols[i]);
        }
    }
#endif /* RUT_ENABLE_BACKTRACE */
}

static void
dump_hash_object_cb(void *key, void *value, void *user_data)
{
    dump_object_cb(value, user_data);
}

static void
destroy_tls_state_cb(void *tls_data)
{
    rut_refcount_debug_state_t *state = tls_data;
    int size = c_hash_table_size(state->hash);

    if (size > 0) {
        // char *thread_name = SDL_GetThreadName ();
#ifdef USE_SDL
        char *thread_name = c_strdup_printf("thread-%lu", SDL_ThreadID());
#elif defined(__linux__)
        char *thread_name =
            c_strdup_printf("thread-%lu", (unsigned long)pthread_self());
#else
#error "Missing platform support for querying thread id"
#endif
        char *filename =
            c_strconcat("rut-object-log-", thread_name, ".txt", NULL);
        char *out_name = c_build_filename(c_get_tmp_dir(), filename, NULL);
        dump_object_callback_data_t data;

        if (size == 1)
            c_warning("%s: One object was leaked", thread_name);
        else
            c_warning("%s: %i objects were leaked", thread_name, size);

        data.state = state;
        data.out_file = fopen(out_name, "w");

        if (data.out_file == NULL) {
            c_warning("Error saving refcount log: %s", strerror(errno));
        } else {
            c_hash_table_foreach(state->hash, dump_hash_object_cb, &data);
            c_llist_foreach(state->owners, (c_iter_func_t)dump_object_cb, &data);

            if (ferror(data.out_file))
                c_warning("Error saving refcount log: %s", strerror(errno));
            else {
                c_warning("Refcount log saved to %s", out_name);
            }

            fclose(data.out_file);
        }

        c_free(out_name);
    }

    c_llist_foreach(state->owners, (c_iter_func_t)object_data_unref, NULL);
    c_llist_free(state->owners);

    c_hash_table_destroy(state->hash);
    c_free(state);
}

void
_rut_refcount_debug_object_created(void *object)
{
    rut_refcount_debug_state_t *state = get_state();

    if (!state->enabled)
        return;

    if (c_hash_table_contains(state->hash, object))
        c_warning("Address of existing object reused for newly created object");
    else {
        rut_refcount_debug_object_t *object_data =
            c_slice_new(rut_refcount_debug_object_t);

        /* The object data might outlive the lifetime of the
         * object itself so lets find out the object type now
         * while we can... */
        object_data->name = rut_object_get_type_name(object);

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
        c_list_init(&object_data->actions);
        log_action(object_data, RUT_REFCOUNT_DEBUG_ACTION_TYPE_CREATE, NULL);

        c_hash_table_insert(state->hash, object, object_data);
        object_data_ref(object_data);
    }
}

void
_rut_refcount_debug_claim(void *object, void *owner)
{
    rut_refcount_debug_state_t *state = get_state();
    rut_refcount_debug_object_t *object_data;

    if (!state->enabled)
        return;

    object_data = c_hash_table_lookup(state->hash, object);

    if (object_data == NULL) {
        c_warning("Reference taken on object that does not exist");
        return;
    }

    if (owner) {
        rut_refcount_debug_object_t *owner_data =
            c_hash_table_lookup(state->hash, owner);

        if (owner_data == NULL)
            c_warning("Reference claimed by object that does not exist");

        if (owner_data->n_claims == 0) {
            state->owners = c_llist_prepend(state->owners, owner_data);
            object_data_ref(owner_data);
            owner_data->n_claims++;
        }

        log_action(
            object_data, RUT_REFCOUNT_DEBUG_ACTION_TYPE_CLAIM, owner_data);
    } else
        log_action(object_data, RUT_REFCOUNT_DEBUG_ACTION_TYPE_REF, NULL);

    object_data->object_ref_count++;
}

void
_rut_refcount_debug_ref(void *object)
{
    _rut_refcount_debug_claim(object, NULL /* owner */);
}

void
_rut_refcount_debug_release(void *object, void *owner)
{
    rut_refcount_debug_state_t *state = get_state();
    rut_refcount_debug_object_t *object_data;

    if (!state->enabled)
        return;

    object_data = c_hash_table_lookup(state->hash, object);

    if (object_data == NULL) {
        c_warning("Reference removed on object that does not exist");
        return;
    }

    if (--object_data->object_ref_count <= 0) {
        if (object_data->object_ref_count != 0) {
            c_warning("Reference less than zero but object still exists: "
                      "corrupt ref_count for object %p",
                      object);
        }
        log_action(object_data, RUT_REFCOUNT_DEBUG_ACTION_TYPE_FREE, NULL);
        object_data->object = NULL;
        c_hash_table_remove(state->hash, object);
    } else {
        if (owner) {
            rut_refcount_debug_object_t *owner_data =
                c_hash_table_lookup(state->hash, object);

            if (owner_data) {
                owner_data->n_claims--;
                if (owner_data->n_claims == 0) {
                    state->owners = c_llist_remove(state->owners, owner_data);
                    object_data_unref(owner_data);
                }
            } else
                c_warning("Reference released by unknown owner");

            log_action(object_data,
                       RUT_REFCOUNT_DEBUG_ACTION_TYPE_RELEASE,
                       owner_data);
        } else
            log_action(object_data, RUT_REFCOUNT_DEBUG_ACTION_TYPE_UNREF, NULL);
    }
}

void
_rut_refcount_debug_unref(void *object)
{
    _rut_refcount_debug_release(object, NULL /* owner */);
}

void
rut_object_dump_refs(void *object)
{
    rut_refcount_debug_state_t *state = get_state();
    rut_refcount_debug_object_t *object_data;
    dump_object_callback_data_t dump_data = {
        .state = state, .out_file = stdout, NULL /* address table */
    };

    if (!state->enabled)
        return;

    object_data = c_hash_table_lookup(state->hash, object);
    if (object_data == NULL) {
        c_debug("No reference information tracked for object %p\n", object);
        return;
    }

    dump_object_cb(object_data, &dump_data);
}
#endif /* RUT_ENABLE_REFCOUNT_DEBUG */
