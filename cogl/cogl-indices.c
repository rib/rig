/*
 * Cogl
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
 *   Robert Bragg <robert@linux.intel.com>
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-util.h"
#include "cogl-object-private.h"
#include "cogl-context-private.h"
#include "cogl-indices.h"
#include "cogl-indices-private.h"
#include "cogl-index-buffer.h"

#include <stdarg.h>

static void _cg_indices_free(cg_indices_t *indices);

CG_OBJECT_DEFINE(Indices, indices);

static size_t
sizeof_indices_type(cg_indices_type_t type)
{
    switch (type) {
    case CG_INDICES_TYPE_UNSIGNED_BYTE:
        return 1;
    case CG_INDICES_TYPE_UNSIGNED_SHORT:
        return 2;
    case CG_INDICES_TYPE_UNSIGNED_INT:
        return 4;
    }
    c_return_val_if_reached(0);
}

cg_indices_t *
cg_indices_new_for_buffer(cg_indices_type_t type,
                          cg_index_buffer_t *buffer,
                          size_t offset)
{
    cg_indices_t *indices = c_slice_new(cg_indices_t);

    indices->buffer = cg_object_ref(buffer);
    indices->offset = offset;

    indices->type = type;

    indices->immutable_ref = 0;

    return _cg_indices_object_new(indices);
}

cg_indices_t *
cg_indices_new(cg_context_t *context,
               cg_indices_type_t type,
               const void *indices_data,
               int n_indices)
{
    size_t buffer_bytes = sizeof_indices_type(type) * n_indices;
    cg_index_buffer_t *index_buffer =
        cg_index_buffer_new(context, buffer_bytes);
    cg_buffer_t *buffer = CG_BUFFER(index_buffer);
    cg_indices_t *indices;
    cg_error_t *ignore_error = NULL;

    cg_buffer_set_data(buffer, 0, indices_data, buffer_bytes, &ignore_error);
    if (ignore_error) {
        cg_error_free(ignore_error);
        cg_object_unref(index_buffer);
        return NULL;
    }

    indices = cg_indices_new_for_buffer(type, index_buffer, 0);
    cg_object_unref(index_buffer);

    return indices;
}

cg_index_buffer_t *
cg_indices_get_buffer(cg_indices_t *indices)
{
    return indices->buffer;
}

cg_indices_type_t
cg_indices_get_type(cg_indices_t *indices)
{
    _CG_RETURN_VAL_IF_FAIL(cg_is_indices(indices),
                           CG_INDICES_TYPE_UNSIGNED_BYTE);
    return indices->type;
}

size_t
cg_indices_get_offset(cg_indices_t *indices)
{
    _CG_RETURN_VAL_IF_FAIL(cg_is_indices(indices), 0);

    return indices->offset;
}

static void
warn_about_midscene_changes(void)
{
    static bool seen = false;
    if (!seen) {
        c_warning("Mid-scene modification of indices has "
                  "undefined results\n");
        seen = true;
    }
}

void
cg_indices_set_offset(cg_indices_t *indices, size_t offset)
{
    _CG_RETURN_IF_FAIL(cg_is_indices(indices));

    if (C_UNLIKELY(indices->immutable_ref))
        warn_about_midscene_changes();

    indices->offset = offset;
}

static void
_cg_indices_free(cg_indices_t *indices)
{
    cg_object_unref(indices->buffer);
    c_slice_free(cg_indices_t, indices);
}

cg_indices_t *
_cg_indices_immutable_ref(cg_indices_t *indices)
{
    _CG_RETURN_VAL_IF_FAIL(cg_is_indices(indices), NULL);

    indices->immutable_ref++;
    _cg_buffer_immutable_ref(CG_BUFFER(indices->buffer));
    return indices;
}

void
_cg_indices_immutable_unref(cg_indices_t *indices)
{
    _CG_RETURN_IF_FAIL(cg_is_indices(indices));
    _CG_RETURN_IF_FAIL(indices->immutable_ref > 0);

    indices->immutable_ref--;
    _cg_buffer_immutable_unref(CG_BUFFER(indices->buffer));
}

cg_indices_t *
cg_get_rectangle_indices(cg_context_t *ctx, int n_rectangles)
{
    int n_indices = n_rectangles * 6;

    /* Check if the largest index required will fit in a byte array... */
    if (n_indices <= 256 / 4 * 6) {
        /* Generate the byte array if we haven't already */
        if (ctx->rectangle_byte_indices == NULL) {
            uint8_t *byte_array = c_malloc(256 / 4 * 6 * sizeof(uint8_t));
            uint8_t *p = byte_array;
            int i, vert_num = 0;

            for (i = 0; i < 256 / 4; i++) {
                *(p++) = vert_num + 0;
                *(p++) = vert_num + 1;
                *(p++) = vert_num + 2;
                *(p++) = vert_num + 0;
                *(p++) = vert_num + 2;
                *(p++) = vert_num + 3;
                vert_num += 4;
            }

            ctx->rectangle_byte_indices = cg_indices_new(
                ctx, CG_INDICES_TYPE_UNSIGNED_BYTE, byte_array, 256 / 4 * 6);

            c_free(byte_array);
        }

        return ctx->rectangle_byte_indices;
    } else {
        if (ctx->rectangle_short_indices_len < n_indices) {
            uint16_t *short_array;
            uint16_t *p;
            int i, vert_num = 0;

            if (ctx->rectangle_short_indices != NULL)
                cg_object_unref(ctx->rectangle_short_indices);
            /* Pick a power of two >= MAX (512, n_indices) */
            if (ctx->rectangle_short_indices_len == 0)
                ctx->rectangle_short_indices_len = 512;
            while (ctx->rectangle_short_indices_len < n_indices)
                ctx->rectangle_short_indices_len *= 2;

            /* Over-allocate to generate a whole number of quads */
            p = short_array = c_malloc((ctx->rectangle_short_indices_len + 5) /
                                       6 * 6 * sizeof(uint16_t));

            /* Fill in the complete quads */
            for (i = 0; i < ctx->rectangle_short_indices_len; i += 6) {
                *(p++) = vert_num + 0;
                *(p++) = vert_num + 1;
                *(p++) = vert_num + 2;
                *(p++) = vert_num + 0;
                *(p++) = vert_num + 2;
                *(p++) = vert_num + 3;
                vert_num += 4;
            }

            ctx->rectangle_short_indices =
                cg_indices_new(ctx,
                               CG_INDICES_TYPE_UNSIGNED_SHORT,
                               short_array,
                               ctx->rectangle_short_indices_len);

            c_free(short_array);
        }

        return ctx->rectangle_short_indices;
    }
}
