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

void
rut_nine_slice_set_image_size (RutObject *self,
                               int width,
                               int height);

CoglTexture *
rut_nine_slice_get_texture (RutNineSlice *nine_slice);

void
rut_nine_slice_set_texture (RutNineSlice *nine_slice,
                            CoglTexture *texture);

CoglPipeline *
rut_nine_slice_get_pipeline (RutNineSlice *nine_slice);

CoglPrimitive *
rut_nine_slice_get_primitive (RutObject *object);

RutMesh *
rut_nine_slice_get_pick_mesh (RutObject *object);

typedef void (* RutNineSliceUpdateCallback) (RutNineSlice *nine_slice,
                                             void *user_data);

RutClosure *
rut_nine_slice_add_update_callback (RutNineSlice *nine_slice,
                                    RutNineSliceUpdateCallback callback,
                                    void *user_data,
                                    RutClosureDestroyCallback destroy_cb);

void
rut_nine_slice_set_width (RutObject *nine_slice, float width);

void
rut_nine_slice_set_height (RutObject *nine_slice, float height);

void
rut_nine_slice_set_left (RutObject *nine_slice, float left);

void
rut_nine_slice_set_right (RutObject *nine_slice, float right);

void
rut_nine_slice_set_top (RutObject *nine_slice, float top);

void
rut_nine_slice_set_bottom (RutObject *nine_slice, float bottom);

#endif /* __RUT_NINE_SLICE_H__ */
