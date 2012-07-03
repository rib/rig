/*
 * Rig
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __RIG_CAMERA_H__
#define __RIG_CAMERA_H__

#include <cogl/cogl.h>

#include "rig-entity.h"
#include "rig-shell.h"
#include "rig-context.h"

typedef void (*RigCameraPaintCallback) (RigCamera *camera, void *user_data);

void
_rig_camera_init_type (void);

RigCamera *
rig_camera_new (RigContext *ctx, CoglFramebuffer *framebuffer);

void
rig_camera_set_background_color4f (RigCamera *camera,
                                   float red,
                                   float green,
                                   float blue,
                                   float alpha);

void
rig_camera_set_clear (RigCamera *camera,
                      CoglBool clear);

CoglFramebuffer *
rig_camera_get_framebuffer (RigCamera *camera);

void
rig_camera_set_framebuffer (RigCamera *camera,
                            CoglFramebuffer *framebuffer);

void
rig_camera_set_viewport (RigCamera *camera,
                         float x,
                         float y,
                         float width,
                         float height);

const float *
rig_camera_get_viewport (RigCamera *camera);

const CoglMatrix *
rig_camera_get_projection (RigCamera *camera);

void
rig_camera_set_near_plane (RigCamera *camera,
                           float near);

float
rig_camera_get_near_plane (RigCamera *camera);

void
rig_camera_set_far_plane (RigCamera *camera,
                          float far);

float
rig_camera_get_far_plane (RigCamera *camera);

RigProjection
rig_camera_get_projection_mode (RigCamera *camera);

void
rig_camera_set_projection_mode (RigCamera *camera,
                                RigProjection projection);

void
rig_camera_set_field_of_view (RigCamera *camera,
                              float fov);

float
rig_camera_get_field_of_view (RigCamera *camera);

void
rig_camera_set_orthographic_coordinates (RigCamera *camera,
                                         float x1,
                                         float y1,
                                         float x2,
                                         float y2);

const CoglMatrix *
rig_camera_get_inverse_projection (RigCamera *camera);

void
rig_camera_set_view_transform (RigCamera *camera,
                               const CoglMatrix *view);

const CoglMatrix *
rig_camera_get_view_transform (RigCamera *camera);

const CoglMatrix *
rig_camera_get_inverse_view_transform (RigCamera *camera);

void
rig_camera_set_input_transform (RigCamera *camera,
                                const CoglMatrix *input_transform);

void
rig_camera_flush (RigCamera *camera);

void
rig_camera_end_frame (RigCamera *camera);

void
rig_camera_add_input_region (RigCamera *camera,
                             RigInputRegion *region);

void
rig_camera_remove_input_region (RigCamera *camera,
                                RigInputRegion *region);

CoglBool
rig_camera_pick_input_region (RigCamera *camera,
                              RigInputRegion *region,
                              float x,
                              float y);

CoglBool
rig_camera_transform_window_coordinate (RigCamera *camera,
                                        float *x,
                                        float *y);

void
rig_camera_unproject_coord (RigCamera *camera,
                            const CoglMatrix *modelview,
                            const CoglMatrix *inverse_modelview,
                            float object_coord_z,
                            float *x,
                            float *y);

#if 0
/* PRIVATE */
RigInputEventStatus
_rig_camera_input_callback_wrapper (RigCameraInputCallbackState *state,
                                    RigInputEvent *event);

void
rig_camera_add_input_callback (RigCamera *camera,
                               RigInputCallback callback,
                               void *user_data);
#endif

#endif /* __RIG_CAMERA_H__ */
