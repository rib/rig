/*
 * Rut.
 *
 * Copyright (C) 2013  Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
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
rut_transform_init_identity (RutTransform *transform);

const CoglMatrix *
rut_transform_get_matrix (RutObject *self);


#endif /* __RUT_TRANSFORM_H__ */
