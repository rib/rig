/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010 Intel Corporation.
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
 *
 *
 * Authors:
 *   Damien Lespiau <damien.lespiau@intel.com>
 *   Robert Bragg <robert@linux.intel.com>
 */

/* For an overview of the functionality implemented here, please see
 * cg-buffer.h, which contains the gtk-doc section overview for the
 * Pixel Buffers API.
 */

#include <cglib-config.h>

#include <stdio.h>
#include <string.h>
#include <clib.h>

#include "cg-util.h"
#include "cg-device-private.h"
#include "cg-object-private.h"
#include "cg-pixel-buffer-private.h"

/* XXX:
 * The cg_object_t macros don't support any form of inheritance, so for
 * now we implement the cg_object_t support for the cg_buffer_t
 * abstract class manually.
 */

static c_sllist_t *_cg_buffer_types;

void
_cg_buffer_register_buffer_type(const cg_object_class_t *klass)
{
    _cg_buffer_types = c_sllist_prepend(_cg_buffer_types, (void *)klass);
}

bool
cg_is_buffer(void *object)
{
    const cg_object_t *obj = object;
    c_sllist_t *l;

    if (object == NULL)
        return false;

    for (l = _cg_buffer_types; l; l = l->next)
        if (l->data == obj->klass)
            return true;

    return false;
}

/*
 * Fallback path, buffer->data points to a malloc'ed buffer.
 */

static void *
malloc_map_range(cg_buffer_t *buffer,
                 size_t offset,
                 size_t size,
                 cg_buffer_access_t access,
                 cg_buffer_map_hint_t hints,
                 cg_error_t **error)
{
    buffer->flags |= CG_BUFFER_FLAG_MAPPED;
    return buffer->data + offset;
}

static void
malloc_unmap(cg_buffer_t *buffer)
{
    buffer->flags &= ~CG_BUFFER_FLAG_MAPPED;
}

static bool
malloc_set_data(cg_buffer_t *buffer,
                unsigned int offset,
                const void *data,
                unsigned int size,
                cg_error_t **error)
{
    memcpy(buffer->data + offset, data, size);
    return true;
}

void
_cg_buffer_initialize(cg_buffer_t *buffer,
                      cg_device_t *dev,
                      size_t size,
                      cg_buffer_bind_target_t default_target,
                      cg_buffer_usage_hint_t usage_hint,
                      cg_buffer_update_hint_t update_hint)
{
    bool use_malloc = false;

    buffer->dev = dev;
    buffer->flags = CG_BUFFER_FLAG_NONE;
    buffer->store_created = false;
    buffer->size = size;
    buffer->last_target = default_target;
    buffer->usage_hint = usage_hint;
    buffer->update_hint = update_hint;
    buffer->data = NULL;
    buffer->immutable_ref = 0;

    if (default_target == CG_BUFFER_BIND_TARGET_PIXEL_PACK ||
        default_target == CG_BUFFER_BIND_TARGET_PIXEL_UNPACK) {
        if (!_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_PBOS))
            use_malloc = true;
    } else if (default_target == CG_BUFFER_BIND_TARGET_ATTRIBUTE_BUFFER ||
               default_target == CG_BUFFER_BIND_TARGET_INDEX_BUFFER) {
        if (!_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_VBOS))
            use_malloc = true;
    }

    if (use_malloc) {
        buffer->vtable.map_range = malloc_map_range;
        buffer->vtable.unmap = malloc_unmap;
        buffer->vtable.set_data = malloc_set_data;

        buffer->data = c_malloc(size);
    } else {
        buffer->vtable.map_range = dev->driver_vtable->buffer_map_range;
        buffer->vtable.unmap = dev->driver_vtable->buffer_unmap;
        buffer->vtable.set_data = dev->driver_vtable->buffer_set_data;

        dev->driver_vtable->buffer_create(buffer);

        buffer->flags |= CG_BUFFER_FLAG_BUFFER_OBJECT;
    }
}

void
_cg_buffer_fini(cg_buffer_t *buffer)
{
    c_return_if_fail(!(buffer->flags & CG_BUFFER_FLAG_MAPPED));
    c_return_if_fail(buffer->immutable_ref == 0);

    if (buffer->flags & CG_BUFFER_FLAG_BUFFER_OBJECT)
        buffer->dev->driver_vtable->buffer_destroy(buffer);
    else
        c_free(buffer->data);
}

unsigned int
cg_buffer_get_size(cg_buffer_t *buffer)
{
    if (!cg_is_buffer(buffer))
        return 0;

    return CG_BUFFER(buffer)->size;
}

void
cg_buffer_set_update_hint(cg_buffer_t *buffer,
                          cg_buffer_update_hint_t hint)
{
    if (!cg_is_buffer(buffer))
        return;

    if (C_UNLIKELY(hint > CG_BUFFER_UPDATE_HINT_STREAM))
        hint = CG_BUFFER_UPDATE_HINT_STATIC;

    buffer->update_hint = hint;
}

cg_buffer_update_hint_t
cg_buffer_get_update_hint(cg_buffer_t *buffer)
{
    if (!cg_is_buffer(buffer))
        return false;

    return buffer->update_hint;
}

static void
warn_about_midscene_changes(void)
{
    static bool seen = false;
    if (!seen) {
        c_warning("Mid-scene modification of buffers has "
                  "undefined results\n");
        seen = true;
    }
}

void *
cg_buffer_map(cg_buffer_t *buffer,
              cg_buffer_access_t access,
              cg_buffer_map_hint_t hints,
              cg_error_t **error)
{
    c_return_val_if_fail(cg_is_buffer(buffer), NULL);

    return cg_buffer_map_range(buffer, 0, buffer->size, access, hints, error);
}

void *
cg_buffer_map_range(cg_buffer_t *buffer,
                    size_t offset,
                    size_t size,
                    cg_buffer_access_t access,
                    cg_buffer_map_hint_t hints,
                    cg_error_t **error)
{
    c_return_val_if_fail(cg_is_buffer(buffer), NULL);
    c_return_val_if_fail(!(buffer->flags & CG_BUFFER_FLAG_MAPPED), NULL);

    if (C_UNLIKELY(buffer->immutable_ref))
        warn_about_midscene_changes();

    buffer->data =
        buffer->vtable.map_range(buffer, offset, size, access, hints, error);

    return buffer->data;
}

void
cg_buffer_unmap(cg_buffer_t *buffer)
{
    if (!cg_is_buffer(buffer))
        return;

    if (!(buffer->flags & CG_BUFFER_FLAG_MAPPED))
        return;

    buffer->vtable.unmap(buffer);
}

void *
_cg_buffer_map_for_fill_or_fallback(cg_buffer_t *buffer)
{
    return _cg_buffer_map_range_for_fill_or_fallback(buffer, 0, buffer->size);
}

void *
_cg_buffer_map_range_for_fill_or_fallback(cg_buffer_t *buffer,
                                          size_t offset,
                                          size_t size)
{
    cg_device_t *dev = buffer->dev;
    void *ret;
    cg_error_t *ignore_error = NULL;

    c_return_val_if_fail(!dev->buffer_map_fallback_in_use, NULL);

    dev->buffer_map_fallback_in_use = true;

    ret = cg_buffer_map_range(buffer,
                              offset,
                              size,
                              CG_BUFFER_ACCESS_WRITE,
                              CG_BUFFER_MAP_HINT_DISCARD,
                              &ignore_error);

    if (ret)
        return ret;

    cg_error_free(ignore_error);

    /* If the map fails then we'll use a temporary buffer to fill
       the data and then upload it using cg_buffer_set_data when
       the buffer is unmapped. The temporary buffer is shared to
       avoid reallocating it every time */
    c_byte_array_set_size(dev->buffer_map_fallback_array, size);
    dev->buffer_map_fallback_offset = offset;

    buffer->flags |= CG_BUFFER_FLAG_MAPPED_FALLBACK;

    return dev->buffer_map_fallback_array->data;
}

void
_cg_buffer_unmap_for_fill_or_fallback(cg_buffer_t *buffer)
{
    cg_device_t *dev = buffer->dev;

    c_return_if_fail(dev->buffer_map_fallback_in_use);

    dev->buffer_map_fallback_in_use = false;

    if ((buffer->flags & CG_BUFFER_FLAG_MAPPED_FALLBACK)) {
        /* Note: don't try to catch OOM errors here since the use
         * cases we currently have for this api (path stroke
         * tesselator) don't have anything particularly sensible they
         * can do in response to a failure anyway so it seems better
         * to simply abort instead.
         *
         * If we find this is a problem for real world applications
         * then in the path tesselation case we could potentially add an
         * explicit cg_path_tesselate_stroke() api that can throw an
         * error for the app to cache.
         */
        cg_buffer_set_data(buffer,
                           dev->buffer_map_fallback_offset,
                           dev->buffer_map_fallback_array->data,
                           dev->buffer_map_fallback_array->len,
                           NULL);
        buffer->flags &= ~CG_BUFFER_FLAG_MAPPED_FALLBACK;
    } else
        cg_buffer_unmap(buffer);
}

bool
cg_buffer_set_data(cg_buffer_t *buffer,
                   size_t offset,
                   const void *data,
                   size_t size,
                   cg_error_t **error)
{
    c_return_val_if_fail(cg_is_buffer(buffer), false);
    c_return_val_if_fail((offset + size) <= buffer->size, false);

    if (C_UNLIKELY(buffer->immutable_ref))
        warn_about_midscene_changes();

    return buffer->vtable.set_data(buffer, offset, data, size, error);
}

cg_buffer_t *
_cg_buffer_immutable_ref(cg_buffer_t *buffer)
{
    c_return_val_if_fail(cg_is_buffer(buffer), NULL);

    buffer->immutable_ref++;
    return buffer;
}

void
_cg_buffer_immutable_unref(cg_buffer_t *buffer)
{
    c_return_if_fail(cg_is_buffer(buffer));
    c_return_if_fail(buffer->immutable_ref > 0);

    buffer->immutable_ref--;
}
