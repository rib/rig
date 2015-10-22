/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
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
 */

#include <cglib-config.h>

#include <string.h>

#include "cg-boxed-value.h"
#include "cg-device-private.h"
#include "cg-util-gl-private.h"

bool
_cg_boxed_value_equal(const cg_boxed_value_t *bva,
                      const cg_boxed_value_t *bvb)
{
    const void *pa, *pb;

    if (bva->type != bvb->type)
        return false;

    switch (bva->type) {
    case CG_BOXED_NONE:
        return true;

    case CG_BOXED_INT:
        if (bva->size != bvb->size || bva->count != bvb->count)
            return false;

        if (bva->count == 1) {
            pa = bva->v.int_value;
            pb = bvb->v.int_value;
        } else {
            pa = bva->v.int_array;
            pb = bvb->v.int_array;
        }

        return !memcmp(pa, pb, sizeof(int) * bva->size * bva->count);

    case CG_BOXED_FLOAT:
        if (bva->size != bvb->size || bva->count != bvb->count)
            return false;

        if (bva->count == 1) {
            pa = bva->v.float_value;
            pb = bvb->v.float_value;
        } else {
            pa = bva->v.float_array;
            pb = bvb->v.float_array;
        }

        return !memcmp(pa, pb, sizeof(float) * bva->size * bva->count);

    case CG_BOXED_MATRIX:
        if (bva->size != bvb->size || bva->count != bvb->count)
            return false;

        if (bva->count == 1) {
            pa = bva->v.matrix;
            pb = bvb->v.matrix;
        } else {
            pa = bva->v.array;
            pb = bvb->v.array;
        }

        return !memcmp(
            pa, pb, sizeof(float) * bva->size * bva->size * bva->count);
    }

    c_warn_if_reached();

    return false;
}

static void
_cg_boxed_value_tranpose(float *dst, int size, const float *src)
{
    int y, x;

    /* If the value is transposed we'll just transpose it now as it
     * is copied into the boxed value instead of passing true to
     * glUniformMatrix because that is not supported on GLES and it
     * doesn't seem like the GL driver would be able to do anything
     * much smarter than this anyway */

    for (y = 0; y < size; y++)
        for (x = 0; x < size; x++)
            *(dst++) = src[y + x * size];
}

static void
_cg_boxed_value_set_x(cg_boxed_value_t *bv,
                      int size,
                      int count,
                      cg_boxed_type_t type,
                      size_t value_size,
                      const void *value,
                      bool transpose)
{
    if (count == 1) {
        if (bv->count > 1)
            c_free(bv->v.array);

        if (transpose)
            _cg_boxed_value_tranpose(bv->v.float_value, size, value);
        else
            memcpy(bv->v.float_value, value, value_size);
    } else {
        if (bv->count > 1) {
            if (bv->count != count || bv->size != size || bv->type != type) {
                c_free(bv->v.array);
                bv->v.array = c_malloc(count * value_size);
            }
        } else
            bv->v.array = c_malloc(count * value_size);

        if (transpose) {
            int value_num;

            for (value_num = 0; value_num < count; value_num++)
                _cg_boxed_value_tranpose(
                    bv->v.float_array + value_num * size * size,
                    size,
                    (const float *)value + value_num * size * size);
        } else
            memcpy(bv->v.array, value, count * value_size);
    }

    bv->type = type;
    bv->size = size;
    bv->count = count;
}

void
_cg_boxed_value_set_1f(cg_boxed_value_t *bv, float value)
{
    _cg_boxed_value_set_x(
        bv, 1, 1, CG_BOXED_FLOAT, sizeof(float), &value, false);
}

void
_cg_boxed_value_set_1i(cg_boxed_value_t *bv, int value)
{
    _cg_boxed_value_set_x(bv, 1, 1, CG_BOXED_INT, sizeof(int), &value, false);
}

void
_cg_boxed_value_set_float(cg_boxed_value_t *bv,
                          int n_components,
                          int count,
                          const float *value)
{
    _cg_boxed_value_set_x(bv,
                          n_components,
                          count,
                          CG_BOXED_FLOAT,
                          sizeof(float) * n_components,
                          value,
                          false);
}

void
_cg_boxed_value_set_int(cg_boxed_value_t *bv,
                        int n_components,
                        int count,
                        const int *value)
{
    _cg_boxed_value_set_x(bv,
                          n_components,
                          count,
                          CG_BOXED_INT,
                          sizeof(int) * n_components,
                          value,
                          false);
}

void
_cg_boxed_value_set_matrix(cg_boxed_value_t *bv,
                           int dimensions,
                           int count,
                           bool transpose,
                           const float *value)
{
    _cg_boxed_value_set_x(bv,
                          dimensions,
                          count,
                          CG_BOXED_MATRIX,
                          sizeof(float) * dimensions * dimensions,
                          value,
                          transpose);
}

void
_cg_boxed_value_copy(cg_boxed_value_t *dst, const cg_boxed_value_t *src)
{
    *dst = *src;

    if (src->count > 1) {
        switch (src->type) {
        case CG_BOXED_NONE:
            break;

        case CG_BOXED_INT:
            dst->v.int_array = c_memdup(src->v.int_array,
                                        src->size * src->count * sizeof(int));
            break;

        case CG_BOXED_FLOAT:
            dst->v.float_array = c_memdup(
                src->v.float_array, src->size * src->count * sizeof(float));
            break;

        case CG_BOXED_MATRIX:
            dst->v.float_array =
                c_memdup(src->v.float_array,
                         src->size * src->size * src->count * sizeof(float));
            break;
        }
    }
}

void
_cg_boxed_value_destroy(cg_boxed_value_t *bv)
{
    if (bv->count > 1)
        c_free(bv->v.array);
}

void
_cg_boxed_value_set_uniform(cg_device_t *dev,
                            GLint location,
                            const cg_boxed_value_t *value)
{
    switch (value->type) {
    case CG_BOXED_NONE:
        break;

    case CG_BOXED_INT: {
        const int *ptr;

        if (value->count == 1)
            ptr = value->v.int_value;
        else
            ptr = value->v.int_array;

        switch (value->size) {
        case 1:
            GE(dev, glUniform1iv(location, value->count, ptr));
            break;
        case 2:
            GE(dev, glUniform2iv(location, value->count, ptr));
            break;
        case 3:
            GE(dev, glUniform3iv(location, value->count, ptr));
            break;
        case 4:
            GE(dev, glUniform4iv(location, value->count, ptr));
            break;
        }
    } break;

    case CG_BOXED_FLOAT: {
        const float *ptr;

        if (value->count == 1)
            ptr = value->v.float_value;
        else
            ptr = value->v.float_array;

        switch (value->size) {
        case 1:
            GE(dev, glUniform1fv(location, value->count, ptr));
            break;
        case 2:
            GE(dev, glUniform2fv(location, value->count, ptr));
            break;
        case 3:
            GE(dev, glUniform3fv(location, value->count, ptr));
            break;
        case 4:
            GE(dev, glUniform4fv(location, value->count, ptr));
            break;
        }
    } break;

    case CG_BOXED_MATRIX: {
        const float *ptr;

        if (value->count == 1)
            ptr = value->v.matrix;
        else
            ptr = value->v.float_array;

        switch (value->size) {
        case 2:
            GE(dev, glUniformMatrix2fv(location, value->count, false, ptr));
            break;
        case 3:
            GE(dev, glUniformMatrix3fv(location, value->count, false, ptr));
            break;
        case 4:
            GE(dev, glUniformMatrix4fv(location, value->count, false, ptr));
            break;
        }
    } break;
    }
}
