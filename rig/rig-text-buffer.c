/* rig-text-buffer.c
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#include "rig.h"
#include "rig-text-buffer.h"

#include <string.h>

/* Initial size of buffer, in bytes */
#define MIN_SIZE 16

enum {
  PROP_TEXT,
  PROP_LENGTH,
  PROP_MAX_LENGTH,
  N_PROPS
};

struct _RigTextBuffer
{
  RigObjectProps _parent;

  int ref_count;

  RigContext *ctx;

  int  max_length;

  /* Only valid if this class is not derived */
  char *simple_text;
  int simple_text_size;
  int simple_text_bytes;
  int simple_text_chars;

  RigTextBufferInsertCallback insert_text_cb;
  void *insert_text_cb_data;

  RigTextBufferDeleteCallback delete_text_cb;
  void *delete_text_cb_data;

  RigSimpleIntrospectableProps introspectable;
  RigProperty properties[N_PROPS];
};

static RigPropertySpec _rig_text_buffer_prop_specs[] = {
  {
    .name = "text",
    .type = RIG_PROPERTY_TYPE_TEXT,
    .data_offset = offsetof (RigTextBuffer, simple_text),
    .getter = rig_text_buffer_get_text,
    .setter = rig_text_buffer_set_text
  },
  {
    .name = "length",
    .type = RIG_PROPERTY_TYPE_INTEGER,
    .data_offset = offsetof (RigTextBuffer, simple_text_chars),
  },
  {
    .name = "max-length",
    .type = RIG_PROPERTY_TYPE_INTEGER,
    .data_offset = offsetof (RigTextBuffer, max_length),
    .setter = rig_text_buffer_set_max_length
  },

  { 0 }
};

/* --------------------------------------------------------------------------------
 *
 */

static void
_rig_text_buffer_notify_inserted_text (RigTextBuffer *buffer,
                                       int position,
                                       const char *chars,
                                       int n_chars)
{
  if (buffer->insert_text_cb)
    buffer->insert_text_cb (buffer,
                            position, chars, n_chars,
                            buffer->insert_text_cb_data);

  rig_property_dirty (&buffer->ctx->property_ctx,
                      &buffer->properties[PROP_TEXT]);
  rig_property_dirty (&buffer->ctx->property_ctx,
                      &buffer->properties[PROP_LENGTH]);
}

static void
_rig_text_buffer_notify_deleted_text (RigTextBuffer *buffer,
                                      int position,
                                      int n_chars)
{
  if (buffer->delete_text_cb)
    buffer->delete_text_cb (buffer,
                            position, n_chars,
                            buffer->delete_text_cb_data);

  rig_property_dirty (&buffer->ctx->property_ctx,
                      &buffer->properties[PROP_TEXT]);
  rig_property_dirty (&buffer->ctx->property_ctx,
                      &buffer->properties[PROP_LENGTH]);
}


/* --------------------------------------------------------------------------------
 * DEFAULT IMPLEMENTATIONS OF TEXT BUFFER
 *
 * These may be overridden by a derived class, behavior may be changed etc...
 * The simple_text and simple_text_xxxx fields may not be valid when
 * this class is derived from.
 */

/* Overwrite a memory that might contain sensitive information. */
static void
trash_area (char *area,
            int len)
{
  volatile char *varea = (volatile char *)area;
  while (len-- > 0)
    *varea++ = 0;
}

static const char*
_rig_simple_text_buffer_get_text (RigTextBuffer *buffer,
                                  int *n_bytes)
{
  if (n_bytes)
    *n_bytes = buffer->simple_text_bytes;
  if (!buffer->simple_text)
      return "";
  return buffer->simple_text;
}

static int
_rig_simple_text_buffer_get_length (RigTextBuffer *buffer)
{
  return buffer->simple_text_chars;
}

static int
_rig_simple_text_buffer_insert_text (RigTextBuffer *buffer,
                                     int position,
                                     const char *chars,
                                     int n_chars)
{
  int prev_size;
  int n_bytes;
  int at;

  n_bytes = g_utf8_offset_to_pointer (chars, n_chars) - chars;

  /* Need more memory */
  if (n_bytes + buffer->simple_text_bytes + 1 > buffer->simple_text_size)
    {
      char *et_new;

      prev_size = buffer->simple_text_size;

      /* Calculate our new buffer size */
      while (n_bytes + buffer->simple_text_bytes + 1 > buffer->simple_text_size)
        {
          if (buffer->simple_text_size == 0)
            buffer->simple_text_size = MIN_SIZE;
          else
            {
              if (2 * buffer->simple_text_size < RIG_TEXT_BUFFER_MAX_SIZE)
                buffer->simple_text_size *= 2;
              else
                {
                  buffer->simple_text_size = RIG_TEXT_BUFFER_MAX_SIZE;
                  if (n_bytes > buffer->simple_text_size - buffer->simple_text_bytes - 1)
                    {
                      n_bytes = buffer->simple_text_size - buffer->simple_text_bytes - 1;
                      n_bytes = g_utf8_find_prev_char (chars, chars + n_bytes + 1) - chars;
                      n_chars = g_utf8_strlen (chars, n_bytes);
                    }
                  break;
                }
            }
        }

      /* Could be a password, so can't leave stuff in memory. */
      et_new = g_malloc (buffer->simple_text_size);
      memcpy (et_new, buffer->simple_text, MIN (prev_size, buffer->simple_text_size));
      trash_area (buffer->simple_text, prev_size);
      g_free (buffer->simple_text);
      buffer->simple_text = et_new;
    }

  /* Actual text insertion */
  at = g_utf8_offset_to_pointer (buffer->simple_text, position) - buffer->simple_text;
  g_memmove (buffer->simple_text + at + n_bytes, buffer->simple_text + at, buffer->simple_text_bytes - at);
  memcpy (buffer->simple_text + at, chars, n_bytes);

  /* Book keeping */
  buffer->simple_text_bytes += n_bytes;
  buffer->simple_text_chars += n_chars;
  buffer->simple_text[buffer->simple_text_bytes] = '\0';

  _rig_text_buffer_notify_inserted_text (buffer, position, chars, n_chars);

  return n_chars;
}

static int
_rig_simple_text_buffer_delete_text (RigTextBuffer *buffer,
                                        int position,
                                        int n_chars)
{
  int start, end;

  if (position > buffer->simple_text_chars)
    position = buffer->simple_text_chars;
  if (position + n_chars > buffer->simple_text_chars)
    n_chars = buffer->simple_text_chars - position;

  if (n_chars > 0)
    {
      start = g_utf8_offset_to_pointer (buffer->simple_text, position) - buffer->simple_text;
      end = g_utf8_offset_to_pointer (buffer->simple_text, position + n_chars) - buffer->simple_text;

      g_memmove (buffer->simple_text + start, buffer->simple_text + end, buffer->simple_text_bytes + 1 - end);
      buffer->simple_text_chars -= n_chars;
      buffer->simple_text_bytes -= (end - start);

      /*
       * Could be a password, make sure we don't leave anything sensitive after
       * the terminating zero.  Note, that the terminating zero already trashed
       * one byte.
       */
      trash_area (buffer->simple_text + buffer->simple_text_bytes + 1, end - start - 1);

      _rig_text_buffer_notify_deleted_text (buffer, position, n_chars);
    }

  return n_chars;
}

/* --------------------------------------------------------------------------------
 *
 */

static void
_rig_text_buffer_free (void *object)
{
  RigTextBuffer *buffer = object;

  if (buffer->simple_text)
    {
      trash_area (buffer->simple_text, buffer->simple_text_size);
      g_free (buffer->simple_text);
    }

  rig_simple_introspectable_destroy (buffer);

  rig_ref_countable_simple_unref (buffer->ctx);

  g_slice_free (RigTextBuffer, buffer);
}

static RigRefCountableVTable _rig_text_buffer_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_text_buffer_free
};

static RigIntrospectableVTable _rig_text_buffer_introspectable_vtable = {
  rig_simple_introspectable_lookup_property,
  rig_simple_introspectable_foreach_property
};

RigType rig_text_buffer_type;

void
_rig_text_buffer_init_type (void)
{
  rig_type_init (&rig_text_buffer_type);
  rig_type_add_interface (&rig_text_buffer_type,
                          RIG_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigTextBuffer, ref_count),
                          &_rig_text_buffer_ref_countable_vtable);
  rig_type_add_interface (&rig_text_buffer_type,
                          RIG_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_rig_text_buffer_introspectable_vtable);
  rig_type_add_interface (&rig_text_buffer_type,
                          RIG_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RigTextBuffer, introspectable),
                          NULL); /* no implied vtable */
}

RigTextBuffer*
rig_text_buffer_new (RigContext *ctx)
{
  RigTextBuffer *buffer;

  buffer = g_slice_new0 (RigTextBuffer);

  rig_object_init (&buffer->_parent, &rig_text_buffer_type);

  buffer->ctx = rig_ref_countable_ref (ctx);

  buffer->ref_count = 1;

  buffer->simple_text = NULL;
  buffer->simple_text_chars = 0;
  buffer->simple_text_bytes = 0;
  buffer->simple_text_size = 0;

  rig_simple_introspectable_init (buffer,
                                  _rig_text_buffer_prop_specs,
                                  buffer->properties);

  return buffer;
}

RigTextBuffer*
rig_text_buffer_new_with_text (RigContext *ctx,
                               const char *text,
                               int text_len)
{
  RigTextBuffer *buffer = rig_text_buffer_new (ctx);
  rig_text_buffer_set_text (buffer, text, text_len);
  return buffer;
}

int
rig_text_buffer_get_length (RigTextBuffer *buffer)
{
  return _rig_simple_text_buffer_get_length (buffer);
}

int
rig_text_buffer_get_bytes (RigTextBuffer *buffer)
{
  int bytes = 0;

  _rig_simple_text_buffer_get_text (buffer, &bytes);
  return bytes;
}

const char*
rig_text_buffer_get_text (RigTextBuffer *buffer)
{
  return _rig_simple_text_buffer_get_text (buffer, NULL);
}

void
rig_text_buffer_set_text (RigTextBuffer *buffer,
                          const char *chars,
                          int n_chars)
{
  g_return_if_fail (chars != NULL);

  rig_text_buffer_delete_text (buffer, 0, -1);
  rig_text_buffer_insert_text (buffer, 0, chars, n_chars);
}

void
rig_text_buffer_set_max_length (RigTextBuffer *buffer,
                                int max_length)
{
  max_length = CLAMP (max_length, 0, RIG_TEXT_BUFFER_MAX_SIZE);

  if (max_length > 0 && rig_text_buffer_get_length (buffer) > max_length)
    rig_text_buffer_delete_text (buffer, max_length, -1);

  buffer->max_length = max_length;
  rig_property_dirty (&buffer->ctx->property_ctx,
                      &buffer->properties[PROP_MAX_LENGTH]);
}

int
rig_text_buffer_get_max_length (RigTextBuffer *buffer)
{
  return buffer->max_length;
}

int
rig_text_buffer_insert_text (RigTextBuffer *buffer,
                             int position,
                             const char *chars,
                             int n_chars)
{
  int length = rig_text_buffer_get_length (buffer);

  if (n_chars < 0)
    n_chars = g_utf8_strlen (chars, -1);

  /* Bring position into bounds */
  if (position == -1 || position > length)
    position = length;

  /* Make sure not entering too much data */
  if (buffer->max_length > 0)
    {
      if (length >= buffer->max_length)
        n_chars = 0;
      else if (length + n_chars > buffer->max_length)
        n_chars -= (length + n_chars) - buffer->max_length;
    }

  return _rig_simple_text_buffer_insert_text (buffer,
                                              position, chars, n_chars);
}

int
rig_text_buffer_delete_text (RigTextBuffer *buffer,
                             int position,
                             int n_chars)
{
  int length;

  length = rig_text_buffer_get_length (buffer);
  if (n_chars < 0)
    n_chars = length;
  if (position == -1 || position > length)
    position = length;
  if (position + n_chars > length)
    n_chars = length - position;

  return _rig_simple_text_buffer_delete_text (buffer, position, n_chars);
}

void
rig_text_buffer_set_insert_text_callback (RigTextBuffer *buffer,
                                          RigTextBufferInsertCallback callback,
                                          void *user_data)
{
  g_return_if_fail (buffer->insert_text_cb == NULL || callback == NULL);
  buffer->insert_text_cb = callback;
  buffer->insert_text_cb_data = user_data;
}

void
rig_text_buffer_set_delete_text_callback (RigTextBuffer *buffer,
                                          RigTextBufferDeleteCallback callback,
                                          void *user_data)
{
  g_return_if_fail (buffer->delete_text_cb == NULL || callback == NULL);
  buffer->delete_text_cb = callback;
  buffer->delete_text_cb_data = user_data;
}

