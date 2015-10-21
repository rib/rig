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

#include <cglib/cglib.h>

#include <rut-shell.h>
#include <rut-mesh.h>

typedef struct _rut_nine_slice_t rut_nine_slice_t;
extern rut_type_t rut_nine_slice_type;

rut_nine_slice_t *rut_nine_slice_new(rut_shell_t *shell,
                                     cg_texture_t *texture,
                                     float top,
                                     float right,
                                     float bottom,
                                     float left,
                                     float width,
                                     float height);

void rut_nine_slice_set_size(rut_object_t *self, float width, float height);

void rut_nine_slice_get_size(rut_object_t *self, float *width, float *height);

void rut_nine_slice_set_image_size(rut_object_t *self, int width, int height);

cg_texture_t *rut_nine_slice_get_texture(rut_nine_slice_t *nine_slice);

void rut_nine_slice_set_texture(rut_nine_slice_t *nine_slice,
                                cg_texture_t *texture);

cg_pipeline_t *rut_nine_slice_get_pipeline(rut_nine_slice_t *nine_slice);

cg_primitive_t *rut_nine_slice_get_primitive(rut_object_t *object);

rut_mesh_t *rut_nine_slice_get_pick_mesh(rut_object_t *object);

typedef void (*rut_nine_slice_update_callback_t)(rut_nine_slice_t *nine_slice,
                                                 void *user_data);

rut_closure_t *
rut_nine_slice_add_update_callback(rut_nine_slice_t *nine_slice,
                                   rut_nine_slice_update_callback_t callback,
                                   void *user_data,
                                   rut_closure_destroy_callback_t destroy_cb);

void rut_nine_slice_set_width(rut_object_t *nine_slice, float width);

void rut_nine_slice_set_height(rut_object_t *nine_slice, float height);

void rut_nine_slice_set_left(rut_object_t *nine_slice, float left);

void rut_nine_slice_set_right(rut_object_t *nine_slice, float right);

void rut_nine_slice_set_top(rut_object_t *nine_slice, float top);

void rut_nine_slice_set_bottom(rut_object_t *nine_slice, float bottom);

#endif /* __RUT_NINE_SLICE_H__ */
