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

#ifndef __CG_BOXED_VALUE_H
#define __CG_BOXED_VALUE_H

#include <clib.h>

#include "cg-device.h"

typedef enum {
    CG_BOXED_NONE,
    CG_BOXED_INT,
    CG_BOXED_FLOAT,
    CG_BOXED_MATRIX
} cg_boxed_type_t;

typedef struct _cg_boxed_value_t {
    cg_boxed_type_t type;
    int size, count;

    union {
        float float_value[4];
        int int_value[4];
        float matrix[16];
        float *float_array;
        int *int_array;
        void *array;
    } v;
} cg_boxed_value_t;

#define _cg_boxed_value_init(bv)                                               \
    C_STMT_START                                                               \
    {                                                                          \
        cg_boxed_value_t *_bv = (bv);                                          \
        _bv->type = CG_BOXED_NONE;                                             \
        _bv->count = 1;                                                        \
    }                                                                          \
    C_STMT_END

bool _cg_boxed_value_equal(const cg_boxed_value_t *bva,
                           const cg_boxed_value_t *bvb);

void _cg_boxed_value_set_1f(cg_boxed_value_t *bv, float value);

void _cg_boxed_value_set_1i(cg_boxed_value_t *bv, int value);

void _cg_boxed_value_set_float(cg_boxed_value_t *bv,
                               int n_components,
                               int count,
                               const float *value);

void _cg_boxed_value_set_int(cg_boxed_value_t *bv,
                             int n_components,
                             int count,
                             const int *value);

void _cg_boxed_value_set_matrix(cg_boxed_value_t *bv,
                                int dimensions,
                                int count,
                                bool transpose,
                                const float *value);

/*
 * _cg_boxed_value_copy:
 * @dst: The destination boxed value
 * @src: The source boxed value
 *
 * This copies @src to @dst. It is assumed that @dst is initialised.
 */
void _cg_boxed_value_copy(cg_boxed_value_t *dst, const cg_boxed_value_t *src);

void _cg_boxed_value_destroy(cg_boxed_value_t *bv);

void _cg_boxed_value_set_uniform(cg_device_t *dev,
                                 int location,
                                 const cg_boxed_value_t *value);

#endif /* __CG_BOXED_VALUE_H */
