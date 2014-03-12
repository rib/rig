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

typedef struct _RigImageSource RigImageSource;

#include "rig-engine.h"
#include "rig-asset.h"

extern RutType rig_image_source_type;

void _rig_image_source_init_type (void);

RigImageSource *
rig_image_source_new (RigEngine *engine,
                      RigAsset *asset);

CoglTexture *
rig_image_source_get_texture (RigImageSource *source);

#ifdef USE_GSTREAMER
CoglGstVideoSink *
rig_image_source_get_sink (RigImageSource *source);
#endif

bool
rig_image_source_get_is_video (RigImageSource *source);

typedef void (*RigImageSourceReadyCallback) (RigImageSource *source,
                                             void *user_data);

typedef void (*RigImageSourceChangedCallback) (RigImageSource *source,
                                               void *user_data);

RutClosure *
rig_image_source_add_ready_callback (RigImageSource *source,
                                     RigImageSourceReadyCallback callback,
                                     void *user_data,
                                     RutClosureDestroyCallback destroy_cb);

RutClosure *
rig_image_source_add_on_changed_callback (RigImageSource *source,
                                          RigImageSourceChangedCallback callback,
                                          void *user_data,
                                          RutClosureDestroyCallback destroy_cb);

void
rig_image_source_set_first_layer (RigImageSource *source,
                                  int first_layer);

void
rig_image_source_set_default_sample (RigImageSource *source,
                                     bool default_sample);

void
rig_image_source_setup_pipeline (RigImageSource *source,
                                 CoglPipeline *pipeline);

void
rig_image_source_attach_frame (RigImageSource *source,
                               CoglPipeline *pipeline);

void
_rig_init_image_source_wrappers_cache (RigEngine *engine);

void
_rig_destroy_image_source_wrappers (RigEngine *engine);

#endif
