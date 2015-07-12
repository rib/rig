/*
 * Rut
 *
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

#include <config.h>

#include "rut-shell.h"
#include "rut-property.h"
#include "rut-introspectable.h"
#include "rut-text-buffer.h"

#include <string.h>

/* Initial size of buffer, in bytes */
#define MIN_SIZE 16

enum {
    PROP_TEXT,
    PROP_LENGTH,
    PROP_MAX_LENGTH,
    N_PROPS
};

struct _rut_text_buffer_t {
    rut_object_base_t _base;

    rut_shell_t *shell;

    int max_length;

    /* Only valid if this class is not derived */
    char *simple_text;
    int simple_text_size;
    int simple_text_bytes;
    int simple_text_chars;

    c_list_t insert_text_cb_list;
    c_list_t delete_text_cb_list;

    rut_introspectable_props_t introspectable;
    rut_property_t properties[N_PROPS];
};

static rut_property_spec_t _rut_text_buffer_prop_specs[] = {
    { .name = "text",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_TEXT,
      .data_offset = offsetof(rut_text_buffer_t, simple_text),
      .getter.text_type = rut_text_buffer_get_text,
      .setter.text_type = rut_text_buffer_set_text },
    { .name = "length",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .data_offset = offsetof(rut_text_buffer_t, simple_text_chars), },
    { .name = "max-length",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .data_offset = offsetof(rut_text_buffer_t, max_length),
      .setter.integer_type = rut_text_buffer_set_max_length },
    { 0 }
};

/* --------------------------------------------------------------------------------
 *
 */

static void
_rut_text_buffer_notify_inserted_text(rut_text_buffer_t *buffer,
                                      int position,
                                      const char *chars,
                                      int n_chars)
{
    rut_closure_list_invoke(&buffer->insert_text_cb_list,
                            rut_text_buffer_insert_callback_t,
                            buffer,
                            position,
                            chars,
                            n_chars);

    rut_property_dirty(&buffer->shell->property_ctx,
                       &buffer->properties[PROP_TEXT]);
    rut_property_dirty(&buffer->shell->property_ctx,
                       &buffer->properties[PROP_LENGTH]);
}

static void
_rut_text_buffer_notify_deleted_text(rut_text_buffer_t *buffer,
                                     int position,
                                     int n_chars)
{
    rut_closure_list_invoke(&buffer->delete_text_cb_list,
                            rut_text_buffer_delete_callback_t,
                            buffer,
                            position,
                            n_chars);

    rut_property_dirty(&buffer->shell->property_ctx,
                       &buffer->properties[PROP_TEXT]);
    rut_property_dirty(&buffer->shell->property_ctx,
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
trash_area(char *area, int len)
{
    volatile char *varea = (volatile char *)area;
    while (len-- > 0)
        *varea++ = 0;
}

static const char *
_rut_simple_text_buffer_get_text(rut_text_buffer_t *buffer,
                                 int *n_bytes)
{
    if (n_bytes)
        *n_bytes = buffer->simple_text_bytes;
    if (!buffer->simple_text)
        return "";
    return buffer->simple_text;
}

static int
_rut_simple_text_buffer_get_length(rut_text_buffer_t *buffer)
{
    return buffer->simple_text_chars;
}

static int
_rut_simple_text_buffer_insert_text(rut_text_buffer_t *buffer,
                                    int position,
                                    const char *chars,
                                    int n_chars)
{
    int prev_size;
    int n_bytes;
    int at;

    n_bytes = g_utf8_offset_to_pointer(chars, n_chars) - chars;

    /* Need more memory */
    if (n_bytes + buffer->simple_text_bytes + 1 > buffer->simple_text_size) {
        char *et_new;

        prev_size = buffer->simple_text_size;

        /* Calculate our new buffer size */
        while (n_bytes + buffer->simple_text_bytes + 1 >
               buffer->simple_text_size) {
            if (buffer->simple_text_size == 0)
                buffer->simple_text_size = MIN_SIZE;
            else {
                if (2 * buffer->simple_text_size < RUT_TEXT_BUFFER_MAX_SIZE)
                    buffer->simple_text_size *= 2;
                else {
                    buffer->simple_text_size = RUT_TEXT_BUFFER_MAX_SIZE;
                    if (n_bytes > buffer->simple_text_size -
                        buffer->simple_text_bytes - 1) {
                        n_bytes = buffer->simple_text_size -
                                  buffer->simple_text_bytes - 1;
                        n_bytes =
                            g_utf8_find_prev_char(chars, chars + n_bytes + 1) -
                            chars;
                        n_chars = g_utf8_strlen(chars, n_bytes);
                    }
                    break;
                }
            }
        }

        /* Could be a password, so can't leave stuff in memory. */
        et_new = c_malloc(buffer->simple_text_size);
        memcpy(et_new,
               buffer->simple_text,
               MIN(prev_size, buffer->simple_text_size));
        trash_area(buffer->simple_text, prev_size);
        c_free(buffer->simple_text);
        buffer->simple_text = et_new;
    }

    /* Actual text insertion */
    at = g_utf8_offset_to_pointer(buffer->simple_text, position) -
         buffer->simple_text;
    g_memmove(buffer->simple_text + at + n_bytes,
              buffer->simple_text + at,
              buffer->simple_text_bytes - at);
    memcpy(buffer->simple_text + at, chars, n_bytes);

    /* Book keeping */
    buffer->simple_text_bytes += n_bytes;
    buffer->simple_text_chars += n_chars;
    buffer->simple_text[buffer->simple_text_bytes] = '\0';

    _rut_text_buffer_notify_inserted_text(buffer, position, chars, n_chars);

    return n_chars;
}

static int
_rut_simple_text_buffer_delete_text(rut_text_buffer_t *buffer,
                                    int position,
                                    int n_chars)
{
    int start, end;

    if (position > buffer->simple_text_chars)
        position = buffer->simple_text_chars;
    if (position + n_chars > buffer->simple_text_chars)
        n_chars = buffer->simple_text_chars - position;

    if (n_chars > 0) {
        start = g_utf8_offset_to_pointer(buffer->simple_text, position) -
                buffer->simple_text;
        end =
            g_utf8_offset_to_pointer(buffer->simple_text, position + n_chars) -
            buffer->simple_text;

        g_memmove(buffer->simple_text + start,
                  buffer->simple_text + end,
                  buffer->simple_text_bytes + 1 - end);
        buffer->simple_text_chars -= n_chars;
        buffer->simple_text_bytes -= (end - start);

        /*
         * Could be a password, make sure we don't leave anything sensitive
         * after
         * the terminating zero.  Note, that the terminating zero already
         * trashed
         * one byte.
         */
        trash_area(buffer->simple_text + buffer->simple_text_bytes + 1,
                   end - start - 1);

        _rut_text_buffer_notify_deleted_text(buffer, position, n_chars);
    }

    return n_chars;
}

/* --------------------------------------------------------------------------------
 *
 */

static void
_rut_text_buffer_free(void *object)
{
    rut_text_buffer_t *buffer = object;

    rut_closure_list_disconnect_all_FIXME(&buffer->insert_text_cb_list);
    rut_closure_list_disconnect_all_FIXME(&buffer->delete_text_cb_list);

    if (buffer->simple_text) {
        trash_area(buffer->simple_text, buffer->simple_text_size);
        c_free(buffer->simple_text);
    }

    rut_introspectable_destroy(buffer);

    rut_object_unref(buffer->shell);

    rut_object_free(rut_text_buffer_t, buffer);
}

rut_type_t rut_text_buffer_type;

void
_rut_text_buffer_init_type(void)
{

    rut_type_t *type = &rut_text_buffer_type;
#define TYPE rut_text_buffer_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rut_text_buffer_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INTROSPECTABLE,
                       offsetof(TYPE, introspectable),
                       NULL); /* no implied vtable */

#undef TYPE
}

rut_text_buffer_t *
rut_text_buffer_new(rut_shell_t *shell)
{
    rut_text_buffer_t *buffer;

    buffer = rut_object_alloc0(
        rut_text_buffer_t, &rut_text_buffer_type, _rut_text_buffer_init_type);

    c_list_init(&buffer->insert_text_cb_list);
    c_list_init(&buffer->delete_text_cb_list);

    buffer->shell = rut_object_ref(shell);

    buffer->simple_text = NULL;
    buffer->simple_text_chars = 0;
    buffer->simple_text_bytes = 0;
    buffer->simple_text_size = 0;

    rut_introspectable_init(
        buffer, _rut_text_buffer_prop_specs, buffer->properties);

    return buffer;
}

rut_text_buffer_t *
rut_text_buffer_new_with_text(rut_shell_t *shell,
                              const char *text,
                              int text_len)
{
    rut_text_buffer_t *buffer = rut_text_buffer_new(shell);
    rut_text_buffer_set_text_with_length(buffer, text, text_len);
    return buffer;
}

int
rut_text_buffer_get_length(rut_text_buffer_t *buffer)
{
    return _rut_simple_text_buffer_get_length(buffer);
}

int
rut_text_buffer_get_bytes(rut_text_buffer_t *buffer)
{
    int bytes = 0;

    _rut_simple_text_buffer_get_text(buffer, &bytes);
    return bytes;
}

const char *
rut_text_buffer_get_text(rut_object_t *obj)
{
    rut_text_buffer_t *buffer = obj;

    return _rut_simple_text_buffer_get_text(buffer, NULL);
}

void
rut_text_buffer_set_text_with_length(rut_text_buffer_t *buffer,
                                     const char *chars,
                                     int n_chars)
{
    c_return_if_fail(chars != NULL);

    rut_text_buffer_delete_text(buffer, 0, -1);
    rut_text_buffer_insert_text(buffer, 0, chars, n_chars);
}

void
rut_text_buffer_set_text(rut_object_t *obj, const char *chars)
{
    rut_text_buffer_t *buffer = obj;

    rut_text_buffer_set_text_with_length(buffer, chars, -1);
}

void
rut_text_buffer_set_max_length(rut_object_t *obj, int max_length)
{
    rut_text_buffer_t *buffer = obj;

    max_length = CLAMP(max_length, 0, RUT_TEXT_BUFFER_MAX_SIZE);

    if (max_length > 0 && rut_text_buffer_get_length(buffer) > max_length)
        rut_text_buffer_delete_text(buffer, max_length, -1);

    buffer->max_length = max_length;
    rut_property_dirty(&buffer->shell->property_ctx,
                       &buffer->properties[PROP_MAX_LENGTH]);
}

int
rut_text_buffer_get_max_length(rut_text_buffer_t *buffer)
{
    return buffer->max_length;
}

int
rut_text_buffer_insert_text(rut_text_buffer_t *buffer,
                            int position,
                            const char *chars,
                            int n_chars)
{
    int length = rut_text_buffer_get_length(buffer);

    if (n_chars < 0)
        n_chars = g_utf8_strlen(chars, -1);

    /* Bring position into bounds */
    if (position == -1 || position > length)
        position = length;

    /* Make sure not entering too much data */
    if (buffer->max_length > 0) {
        if (length >= buffer->max_length)
            n_chars = 0;
        else if (length + n_chars > buffer->max_length)
            n_chars -= (length + n_chars) - buffer->max_length;
    }

    return _rut_simple_text_buffer_insert_text(
        buffer, position, chars, n_chars);
}

int
rut_text_buffer_delete_text(rut_text_buffer_t *buffer,
                            int position,
                            int n_chars)
{
    int length;

    length = rut_text_buffer_get_length(buffer);
    if (n_chars < 0)
        n_chars = length;
    if (position == -1 || position > length)
        position = length;
    if (position + n_chars > length)
        n_chars = length - position;

    return _rut_simple_text_buffer_delete_text(buffer, position, n_chars);
}

rut_closure_t *
rut_text_buffer_add_insert_text_callback(
    rut_text_buffer_t *buffer,
    rut_text_buffer_insert_callback_t callback,
    void *user_data,
    rut_closure_destroy_callback_t destroy_cb)
{
    c_return_val_if_fail(callback != NULL, NULL);

    return rut_closure_list_add_FIXME(
        &buffer->insert_text_cb_list, callback, user_data, destroy_cb);
}

rut_closure_t *
rut_text_buffer_add_delete_text_callback(
    rut_text_buffer_t *buffer,
    rut_text_buffer_delete_callback_t callback,
    void *user_data,
    rut_closure_destroy_callback_t destroy_cb)
{
    c_return_val_if_fail(callback != NULL, NULL);

    return rut_closure_list_add_FIXME(
        &buffer->delete_text_cb_list, callback, user_data, destroy_cb);
}
