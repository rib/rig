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

#ifndef __RIG_NINE_SLICE_H__
#define __RIG_NINE_SLICE_H__

#include <cogl/cogl.h>

#include <rut.h>

typedef struct _RigNineSlice RigNineSlice;
extern RutType rig_nine_slice_type;

RigNineSlice *
rig_nine_slice_new (RutContext *ctx,
                    CoglTexture *texture,
                    float top,
                    float right,
                    float bottom,
                    float left,
                    float width,
                    float height);

void
rig_nine_slice_set_size (RutObject *self,
                         float width,
                         float height);

void
rig_nine_slice_get_size (RutObject *self,
                         float *width,
                         float *height);

void
rig_nine_slice_set_image_size (RutObject *self,
                               int width,
                               int height);

CoglTexture *
rig_nine_slice_get_texture (RigNineSlice *nine_slice);

void
rig_nine_slice_set_texture (RigNineSlice *nine_slice,
                            CoglTexture *texture);

CoglPipeline *
rig_nine_slice_get_pipeline (RigNineSlice *nine_slice);

CoglPrimitive *
rig_nine_slice_get_primitive (RutObject *object);

RutMesh *
rig_nine_slice_get_pick_mesh (RutObject *object);

typedef void (* RigNineSliceUpdateCallback) (RigNineSlice *nine_slice,
                                             void *user_data);

RutClosure *
rig_nine_slice_add_update_callback (RigNineSlice *nine_slice,
                                    RigNineSliceUpdateCallback callback,
                                    void *user_data,
                                    RutClosureDestroyCallback destroy_cb);

void
rig_nine_slice_set_width (RutObject *nine_slice, float width);

void
rig_nine_slice_set_height (RutObject *nine_slice, float height);

void
rig_nine_slice_set_left (RutObject *nine_slice, float left);

void
rig_nine_slice_set_right (RutObject *nine_slice, float right);

void
rig_nine_slice_set_top (RutObject *nine_slice, float top);

void
rig_nine_slice_set_bottom (RutObject *nine_slice, float bottom);

#endif /* __RIG_NINE_SLICE_H__ */
