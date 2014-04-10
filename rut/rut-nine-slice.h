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

#ifndef __RUT_NINE_SLICE_H__
#define __RUT_NINE_SLICE_H__

#include <cogl/cogl.h>

#include <rut-context.h>
#include <rut-mesh.h>

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
