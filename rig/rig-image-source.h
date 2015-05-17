/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012  Intel Corporation
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
 */

#ifndef __RIG_IMAGE_SOURCE_H__
#define __RIG_IMAGE_SOURCE_H__

#ifdef USE_GSTREAMER
#include <cogl-gst/cogl-gst.h>
#endif

#include <rut.h>

typedef struct _rig_image_source_t rig_image_source_t;

#include "rig-frontend.h"

extern rut_type_t rig_image_source_type;

void _rig_image_source_init_type(void);

rig_image_source_t *rig_image_source_new(rig_frontend_t *frontend,
                                         const char *mime,
                                         const char *path,
                                         const uint8_t *data,
                                         int len,
                                         int natural_width,
                                         int natural_height);

cg_texture_t *rig_image_source_get_texture(rig_image_source_t *source);

#ifdef USE_GSTREAMER
CgGstVideoSink *rig_image_source_get_sink(rig_image_source_t *source);
#endif

bool rig_image_source_get_is_video(rig_image_source_t *source);

typedef void (*rig_image_source_ready_callback_t)(rig_image_source_t *source,
                                                  void *user_data);

typedef void (*rig_image_source_changed_callback_t)(rig_image_source_t *source,
                                                    void *user_data);

rut_closure_t *
rig_image_source_add_ready_callback(rig_image_source_t *source,
                                    rig_image_source_ready_callback_t callback,
                                    void *user_data,
                                    rut_closure_destroy_callback_t destroy_cb);

rut_closure_t *rig_image_source_add_on_changed_callback(
    rig_image_source_t *source,
    rig_image_source_changed_callback_t callback,
    void *user_data,
    rut_closure_destroy_callback_t destroy_cb);

void rig_image_source_set_first_layer(rig_image_source_t *source,
                                      int first_layer);

void rig_image_source_set_default_sample(rig_image_source_t *source,
                                         bool default_sample);

void rig_image_source_setup_pipeline(rig_image_source_t *source,
                                     cg_pipeline_t *pipeline);

void rig_image_source_attach_frame(rig_image_source_t *source,
                                   cg_pipeline_t *pipeline);

void _rig_init_image_source_wrappers_cache(rig_frontend_t *frontend);

void _rig_destroy_image_source_wrappers(rig_frontend_t *frontend);

void rig_image_source_get_natural_size(rig_image_source_t *source,
                                       float *width,
                                       float *height);

#endif
