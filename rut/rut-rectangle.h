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

#ifndef __RUT_RECTANGLE_H__
#define __RUT_RECTANGLE_H__

#include <cglib/cglib.h>

#include <rut-shell.h>

typedef struct _rut_rectangle_t rut_rectangle_t;
extern rut_type_t rut_rectangle_type;

rut_rectangle_t *rut_rectangle_new4f(rut_shell_t *shell,
                                     float width,
                                     float height,
                                     float red,
                                     float green,
                                     float blue,
                                     float alpha);

void rut_rectangle_set_width(rut_rectangle_t *rectangle, float width);

void rut_rectangle_set_height(rut_rectangle_t *rectangle, float height);

void rut_rectangle_set_size(rut_object_t *self, float width, float height);

void rut_rectangle_get_size(rut_object_t *self, float *width, float *height);

#endif /* __RUT_RECTANGLE_H__ */
