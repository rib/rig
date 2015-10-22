/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2009,2010,2012 Intel Corporation.
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
 *   Havoc Pennington <hp@pobox.com> for litl
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef _CG_MATRIX_STACK_PRIVATE_H_
#define _CG_MATRIX_STACK_PRIVATE_H_

#include "cg-object-private.h"
#include "cg-matrix-stack.h"
#include "cg-device.h"
#include "cg-framebuffer.h"

typedef enum _cg_matrix_op_t {
    CG_MATRIX_OP_LOAD_IDENTITY,
    CG_MATRIX_OP_TRANSLATE,
    CG_MATRIX_OP_ROTATE,
    CG_MATRIX_OP_ROTATE_QUATERNION,
    CG_MATRIX_OP_ROTATE_EULER,
    CG_MATRIX_OP_SCALE,
    CG_MATRIX_OP_MULTIPLY,
    CG_MATRIX_OP_LOAD,
    CG_MATRIX_OP_SAVE,
} cg_matrix_op_t;

struct _cg_matrix_entry_t {
    cg_matrix_entry_t *parent;
    cg_matrix_op_t op;
    unsigned int ref_count;

#ifdef CG_DEBUG_ENABLED
    /* used for performance tracing */
    int composite_gets;
#endif
};

typedef struct _cg_matrix_entry_translate_t {
    cg_matrix_entry_t _parent_data;

    float x;
    float y;
    float z;

} cg_matrix_entry_translate_t;

typedef struct _cg_matrix_entry_rotate_t {
    cg_matrix_entry_t _parent_data;

    float angle;
    float x;
    float y;
    float z;

} cg_matrix_entry_rotate_t;

typedef struct _cg_matrix_entry_rotate_euler_t {
    cg_matrix_entry_t _parent_data;

    /* This doesn't store an actual c_euler_t in order to avoid the
     * padding */
    float heading;
    float pitch;
    float roll;
} cg_matrix_entry_rotate_euler_t;

typedef struct _cg_matrix_entry_rotate_quaternion_t {
    cg_matrix_entry_t _parent_data;

    /* This doesn't store an actual c_quaternion_t in order to avoid the
     * padding */
    float values[4];
} cg_matrix_entry_rotate_quaternion_t;

typedef struct _cg_matrix_entry_scale_t {
    cg_matrix_entry_t _parent_data;

    float x;
    float y;
    float z;

} cg_matrix_entry_scale_t;

typedef struct _cg_matrix_entry_multiply_t {
    cg_matrix_entry_t _parent_data;

    c_matrix_t *matrix;

} cg_matrix_entry_multiply_t;

typedef struct _cg_matrix_entry_load_t {
    cg_matrix_entry_t _parent_data;

    c_matrix_t *matrix;

} cg_matrix_entry_load_t;

typedef struct _cg_matrix_entry_save_t {
    cg_matrix_entry_t _parent_data;

    c_matrix_t *cache;
    bool cache_valid;

} cg_matrix_entry_save_t;

typedef union _cg_matrix_entry_full_t {
    cg_matrix_entry_t any;
    cg_matrix_entry_translate_t translate;
    cg_matrix_entry_rotate_t rotate;
    cg_matrix_entry_rotate_euler_t rotate_euler;
    cg_matrix_entry_rotate_quaternion_t rotate_quaternion;
    cg_matrix_entry_scale_t scale;
    cg_matrix_entry_multiply_t multiply;
    cg_matrix_entry_load_t load;
    cg_matrix_entry_save_t save;
} cg_matrix_entry_full_t;

struct _cg_matrix_stack_t {
    cg_object_t _parent;

    cg_device_t *dev;

    cg_matrix_entry_t *last_entry;
};

typedef struct _cg_matrix_entry_cache_t {
    cg_matrix_entry_t *entry;
    bool flushed_identity;
} cg_matrix_entry_cache_t;

void _cg_matrix_entry_identity_init(cg_matrix_entry_t *entry);

void _cg_matrix_entry_cache_init(cg_matrix_entry_cache_t *cache);

bool _cg_matrix_entry_cache_maybe_update(cg_matrix_entry_cache_t *cache,
                                         cg_matrix_entry_t *entry);

void _cg_matrix_entry_cache_destroy(cg_matrix_entry_cache_t *cache);

#endif /* _CG_MATRIX_STACK_PRIVATE_H_ */
