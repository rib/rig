/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013  Intel Corporation.
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

#ifndef __RUT_TRANSFORM_H__
#define __RUT_TRANSFORM_H__

#include <cglib/cglib.h>

#include <rut-shell.h>

typedef struct _rut_transform_t rut_transform_t;
extern rut_type_t rut_transform_type;

rut_transform_t *rut_transform_new(rut_shell_t *shell);

void
rut_transform_translate(rut_transform_t *transform, float x, float y, float z);

void rut_transform_rotate(
    rut_transform_t *transform, float angle, float x, float y, float z);

void rut_transform_quaternion_rotate(rut_transform_t *transform,
                                     const c_quaternion_t *quaternion);

void rut_transform_scale(rut_transform_t *transform, float x, float y, float z);

void rut_transform_transform(rut_transform_t *transform,
                             const c_matrix_t *matrix);

void rut_transform_init_identity(rut_transform_t *transform);

const c_matrix_t *rut_transform_get_matrix(rut_object_t *self);

#endif /* __RUT_TRANSFORM_H__ */
