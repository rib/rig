/*
 * Rut
 *
 * Copyright (C) 2012  Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef __RIG_IMAGE_SOURCE_H__
#define __RIG_IMAGE_SOURCE_H__

#include <cogl-gst/cogl-gst.h>

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

CoglGstVideoSink *
rig_image_source_get_sink (RigImageSource *source);

CoglBool
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
