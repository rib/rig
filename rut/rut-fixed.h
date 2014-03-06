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

#ifndef __RUT_FIXED_H__
#define __RUT_FIXED_H__

#include <cogl/cogl.h>

#include <rut-context.h>

typedef struct _RutFixed RutFixed;
extern RutType rut_fixed_type;

RutFixed *
rut_fixed_new (RutContext *ctx,
               float width,
               float height);

void
rut_fixed_set_width (RutFixed *fixed, float width);

void
rut_fixed_set_height (RutFixed *fixed, float height);

void
rut_fixed_set_size (RutObject *self,
                    float width,
                    float height);

void
rut_fixed_get_size (RutObject *self,
                    float *width,
                    float *height);

void
rut_fixed_add_child (RutFixed *fixed, RutObject *child);

void
rut_fixed_remove_child (RutFixed *fixed, RutObject *child);

#endif /* __RUT_FIXED_H__ */
