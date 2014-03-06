/*
 * Rig
 *
 * UI Engine & Editor
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
