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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __RUT_CAMERA_H__
#define __RUT_CAMERA_H__

#include <cogl/cogl.h>

#include "rut-entity.h"
#include "rut-shell.h"
#include "rut-context.h"

typedef void (*RutCameraPaintCallback) (RutCamera *camera, void *user_data);

void
_rut_camera_init_type (void);

RutCamera *
rut_camera_new (RutContext *ctx, CoglFramebuffer *framebuffer);

void
rut_camera_set_background_color4f (RutCamera *camera,
                                   float red,
                                   float green,
                                   float blue,
                                   float alpha);

void
rut_camera_set_background_color (RutObject *camera,
                                 const CoglColor *color);

const CoglColor *
rut_camera_get_background_color (RutObject *camera);

void
rut_camera_set_clear (RutCamera *camera,
                      CoglBool clear);

CoglFramebuffer *
rut_camera_get_framebuffer (RutCamera *camera);

void
rut_camera_set_framebuffer (RutCamera *camera,
                            CoglFramebuffer *framebuffer);

void
rut_camera_set_viewport (RutCamera *camera,
                         float x,
                         float y,
                         float width,
                         float height);

void
rut_camera_set_viewport_x (RutObject *camera,
                           float x);

void
rut_camera_set_viewport_y (RutObject *camera,
                           float y);

void
rut_camera_set_viewport_width (RutObject *camera,
                               float width);

void
rut_camera_set_viewport_height (RutObject *camera,
                                float height);

const float *
rut_camera_get_viewport (RutCamera *camera);

const CoglMatrix *
rut_camera_get_projection (RutCamera *camera);

void
rut_camera_set_near_plane (RutObject *camera,
                           float near);

float
rut_camera_get_near_plane (RutObject *camera);

void
rut_camera_set_far_plane (RutObject *camera,
                          float far);

float
rut_camera_get_far_plane (RutObject *camera);

RutProjection
rut_camera_get_projection_mode (RutCamera *camera);

void
rut_camera_set_projection_mode (RutCamera *camera,
                                RutProjection projection);

void
rut_camera_set_field_of_view (RutObject *camera,
                              float fov);

float
rut_camera_get_field_of_view (RutObject *camera);

void
rut_camera_set_orthographic_coordinates (RutCamera *camera,
                                         float x1,
                                         float y1,
                                         float x2,
                                         float y2);

const CoglMatrix *
rut_camera_get_inverse_projection (RutCamera *camera);

void
rut_camera_set_view_transform (RutCamera *camera,
                               const CoglMatrix *view);

const CoglMatrix *
rut_camera_get_view_transform (RutCamera *camera);

const CoglMatrix *
rut_camera_get_inverse_view_transform (RutCamera *camera);

void
rut_camera_set_input_transform (RutCamera *camera,
                                const CoglMatrix *input_transform);

void
rut_camera_flush (RutCamera *camera);

void
rut_camera_suspend (RutCamera *camera);

void
rut_camera_resume (RutCamera *camera);

void
rut_camera_end_frame (RutCamera *camera);

void
rut_camera_add_input_region (RutCamera *camera,
                             RutInputRegion *region);

void
rut_camera_remove_input_region (RutCamera *camera,
                                RutInputRegion *region);

CoglBool
rut_camera_transform_window_coordinate (RutCamera *camera,
                                        float *x,
                                        float *y);

void
rut_camera_unproject_coord (RutCamera *camera,
                            const CoglMatrix *modelview,
                            const CoglMatrix *inverse_modelview,
                            float object_coord_z,
                            float *x,
                            float *y);

CoglPrimitive *
rut_camera_create_frustum_primitive (RutCamera *camera);

void
rut_camera_set_focal_distance (RutObject *camera,
                               float focal_distance);

float
rut_camera_get_focal_distance (RutObject *camera);

void
rut_camera_set_depth_of_field (RutObject *camera,
                               float depth_of_field);

float
rut_camera_get_depth_of_field (RutObject *camera);

void
rut_camera_set_zoom (RutObject *obj,
                     float zoom);

float
rut_camera_get_zoom (RutObject *camera);

#endif /* __RUT_CAMERA_H__ */
