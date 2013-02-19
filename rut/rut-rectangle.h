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

#ifndef __RUT_RECTANGLE_H__
#define __RUT_RECTANGLE_H__

#include <cogl/cogl.h>

#include <rut-context.h>

typedef struct _RutRectangle RutRectangle;
extern RutType rut_rectangle_type;

RutRectangle *
rut_rectangle_new4f (RutContext *ctx,
                     float width,
                     float height,
                     float red,
                     float green,
                     float blue,
                     float alpha);

void
rut_rectangle_set_width (RutRectangle *rectangle, float width);

void
rut_rectangle_set_height (RutRectangle *rectangle, float height);

void
rut_rectangle_set_size (RutObject *self,
                        float width,
                        float height);

void
rut_rectangle_get_size (RutObject *self,
                        float *width,
                        float *height);

#endif /* __RUT_RECTANGLE_H__ */
