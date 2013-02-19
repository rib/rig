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

#ifndef __RUT_NINE_SLICE_H__
#define __RUT_NINE_SLICE_H__

#include <cogl/cogl.h>

#include <rut-context.h>

typedef struct _RutNineSlice RutNineSlice;
extern RutType rut_nine_slice_type;

RutNineSlice *
rut_nine_slice_new (RutContext *ctx,
                    CoglTexture *texture,
                    float top,
                    float right,
                    float bottom,
                    float left,
                    float width,
                    float height);

void
rut_nine_slice_set_size (RutObject *self,
                         float width,
                         float height);

void
rut_nine_slice_get_size (RutObject *self,
                         float *width,
                         float *height);

CoglTexture *
rut_nine_slice_get_texture (RutNineSlice *nine_slice);

CoglPipeline *
rut_nine_slice_get_pipeline (RutNineSlice *nine_slice);

#endif /* __RUT_NINE_SLICE_H__ */
