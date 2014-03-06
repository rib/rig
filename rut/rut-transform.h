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

#include <cogl/cogl.h>

#include <rut-context.h>

typedef struct _RutTransform RutTransform;
extern RutType rut_transform_type;

RutTransform *
rut_transform_new (RutContext *ctx);

void
rut_transform_translate (RutTransform *transform,
                         float x,
                         float y,
                         float z);

void
rut_transform_rotate (RutTransform *transform,
                      float angle,
                      float x,
                      float y,
                      float z);

void
rut_transform_quaternion_rotate (RutTransform *transform,
                                 const CoglQuaternion *quaternion);

void
rut_transform_scale (RutTransform *transform,
                     float x,
                     float y,
                     float z);

void
rut_transform_transform (RutTransform *transform,
                         const CoglMatrix *matrix);

void
rut_transform_init_identity (RutTransform *transform);

const CoglMatrix *
rut_transform_get_matrix (RutObject *self);


#endif /* __RUT_TRANSFORM_H__ */
