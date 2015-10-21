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

#ifndef __RUT_SHIM_H__
#define __RUT_SHIM_H__

#include <cglib/cglib.h>

#include <rut-shell.h>

typedef struct _rut_shim_t rut_shim_t;
extern rut_type_t rut_shim_type;

rut_shim_t *rut_shim_new(rut_shell_t *shell, float width, float height);

void rut_shim_set_width(rut_shim_t *shim, float width);

void rut_shim_set_height(rut_shim_t *shim, float height);

void rut_shim_set_size(rut_object_t *self, float width, float height);

void rut_shim_get_size(rut_object_t *self, float *width, float *height);

void rut_shim_set_child(rut_shim_t *shim, rut_object_t *child);

typedef enum _rut_shim_axis_t {
    RUT_SHIM_AXIS_XY,
    RUT_SHIM_AXIS_X,
    RUT_SHIM_AXIS_Y
} rut_shim_axis_t;

void rut_shim_set_shim_axis(rut_shim_t *shim, rut_shim_axis_t axis);

#endif /* __RUT_SHIM_H__ */
