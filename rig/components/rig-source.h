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

#pragma once

#ifdef USE_GSTREAMER
#include <cogl-gst/cogl-gst.h>
#endif

#include <rut.h>

typedef struct _rig_source_t rig_source_t;

#include "rig-frontend.h"

extern rut_type_t rig_source_type;

rig_source_t *
rig_source_new(rig_engine_t *engine,
               const char *mime,
               const char *url,
               const uint8_t *data,
               int len,
               int natural_width,
               int natural_height);

cg_texture_t *rig_source_get_texture(rig_source_t *source);

typedef void (*rig_source_ready_callback_t)(rig_source_t *source,
                                            void *user_data);

typedef void (*rig_source_changed_callback_t)(rig_source_t *source,
                                              void *user_data);

typedef void (*rig_source_error_callback_t)(rig_source_t *source,
                                            const char *message,
                                            void *user_data);

void
rig_source_add_ready_callback(rig_source_t *source,
                              rut_closure_t *closure);
void
rig_source_add_on_changed_callback(rig_source_t *source,
                                   rut_closure_t *closure);
void
rig_source_add_on_error_callback(rig_source_t *source,
                                 rut_closure_t *closure);

void rig_source_set_first_layer(rig_source_t *source,
                                int first_layer);

void rig_source_set_default_sample(rig_source_t *source,
                                   bool default_sample);

void rig_source_setup_pipeline(rig_source_t *source,
                               cg_pipeline_t *pipeline);

void rig_source_attach_frame(rig_source_t *source,
                             cg_pipeline_t *pipeline);

void _rig_init_source_wrappers_cache(rig_frontend_t *frontend);

void _rig_destroy_source_wrappers(rig_frontend_t *frontend);

void rig_source_get_natural_size(rig_source_t *source,
                                 float *width,
                                 float *height);

void
rig_source_set_url(rut_object_t *obj, const char *url);

const char *
rig_source_get_url(rut_object_t *obj);
