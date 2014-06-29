/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013,2014  Intel Corporation.
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

#ifndef _RUT_CAMERA_H_
#define _RUT_CAMERA_H_

#include <clib.h>

#include <cogl/cogl.h>

#include "rut-object.h"
#include "rut-interfaces.h"
#include "rut-context.h"

typedef struct _RutCameraProps
{
  cg_color_t bg_color;
  bool clear_fb;

  float viewport[4];

  float near, far;

  float fov; /* perspective */

  float x1, y1, x2, y2; /* orthographic */

  float zoom;

  float focal_distance;
  float depth_of_field;

  cg_matrix_t projection;
  unsigned int projection_age;
  unsigned int projection_cache_age;

  cg_matrix_t inverse_projection;
  unsigned int inverse_projection_age;

  cg_matrix_t view;
  unsigned int view_age;

  cg_matrix_t inverse_view;
  unsigned int inverse_view_age;

  unsigned int transform_age;
  unsigned int at_suspend_transform_age;

  cg_framebuffer_t *fb;

  RutGraphableProps graphable;

  cg_matrix_t input_transform;
  c_list_t *input_regions;

  unsigned int orthographic:1;
  unsigned int in_frame:1;
  unsigned int suspended:1;
} RutCameraProps;

typedef struct _RutCameraVTable
{
  RutContext *(*get_context) (RutObject *camera);

  void (*set_background_color4f) (RutObject *camera,
                                  float red,
                                  float green,
                                  float blue,
                                  float alpha);

  void (*set_background_color) (RutObject *camera,
                                const cg_color_t *color);

  void (*set_clear) (RutObject *camera,
                     bool clear);

  void (*set_framebuffer) (RutObject *camera,
                           cg_framebuffer_t *framebuffer);

  void (*set_viewport) (RutObject *camera,
                        float x,
                        float y,
                        float width,
                        float height);

  void (*set_viewport_x) (RutObject *camera,
                          float x);

  void (*set_viewport_y) (RutObject *camera,
                          float y);

  void (*set_viewport_width) (RutObject *camera,
                              float width);

  void (*set_viewport_height) (RutObject *camera,
                               float height);

  const cg_matrix_t * (*get_projection) (RutObject *camera);

  void (*set_near_plane) (RutObject *camera,
                          float near);

  void (*set_far_plane) (RutObject *camera,
                         float far);

  RutProjection (*get_projection_mode) (RutObject *camera);

  void (*set_projection_mode) (RutObject *camera,
                               RutProjection projection);

  void (*set_field_of_view) (RutObject *camera,
                             float fov);

  void (*set_orthographic_coordinates) (RutObject *camera,
                                        float x1,
                                        float y1,
                                        float x2,
                                        float y2);

  const cg_matrix_t * (*get_inverse_projection) (RutObject *camera);

  void (*set_view_transform) (RutObject *camera,
                              const cg_matrix_t *view);

  const cg_matrix_t * (*get_inverse_view_transform) (RutObject *camera);

  void (*set_input_transform) (RutObject *camera,
                               const cg_matrix_t *input_transform);

  void (*flush) (RutObject *camera);

  void (*suspend) (RutObject *camera);

  void (*resume) (RutObject *camera);

  void (*end_frame) (RutObject *camera);

  void (*add_input_region) (RutObject *camera,
                            RutInputRegion *region);

  void (*remove_input_region) (RutObject *camera,
                               RutInputRegion *region);

  c_list_t *(*get_input_regions) (RutObject *camera);

  bool (*transform_window_coordinate) (RutObject *camera,
                                           float *x,
                                           float *y);

  void (*unproject_coord) (RutObject *camera,
                           const cg_matrix_t *modelview,
                           const cg_matrix_t *inverse_modelview,
                           float object_coord_z,
                           float *x,
                           float *y);

  cg_primitive_t * (*create_frustum_primitive) (RutObject *camera);

  void (*set_focal_distance) (RutObject *camera,
                              float focal_distance);

  void (*set_depth_of_field) (RutObject *camera,
                              float depth_of_field);

  void (*set_zoom) (RutObject *obj,
                    float zoom);

} RutCameraVTable;


RutContext *
rut_camera_get_context (RutObject *camera);

void
rut_camera_set_background_color4f (RutObject *camera,
                                   float red,
                                   float green,
                                   float blue,
                                   float alpha);

void
rut_camera_set_background_color (RutObject *camera,
                                 const cg_color_t *color);

const cg_color_t *
rut_camera_get_background_color (RutObject *camera);

void
rut_camera_set_clear (RutObject *camera,
                      bool clear);

cg_framebuffer_t *
rut_camera_get_framebuffer (RutObject *camera);

void
rut_camera_set_framebuffer (RutObject *camera,
                            cg_framebuffer_t *framebuffer);

void
rut_camera_set_viewport (RutObject *camera,
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
rut_camera_get_viewport (RutObject *camera);

const cg_matrix_t *
rut_camera_get_projection (RutObject *camera);

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
rut_camera_get_projection_mode (RutObject *camera);

void
rut_camera_set_projection_mode (RutObject *camera,
                                RutProjection projection);

void
rut_camera_set_field_of_view (RutObject *camera,
                              float fov);

float
rut_camera_get_field_of_view (RutObject *camera);

void
rut_camera_set_orthographic_coordinates (RutObject *camera,
                                         float x1,
                                         float y1,
                                         float x2,
                                         float y2);

void
rut_camera_get_orthographic_coordinates (RutObject *camera,
                                         float *x1,
                                         float *y1,
                                         float *x2,
                                         float *y2);
const cg_matrix_t *
rut_camera_get_inverse_projection (RutObject *camera);

void
rut_camera_set_view_transform (RutObject *camera,
                               const cg_matrix_t *view);

const cg_matrix_t *
rut_camera_get_view_transform (RutObject *camera);

const cg_matrix_t *
rut_camera_get_inverse_view_transform (RutObject *camera);

const cg_matrix_t *
rut_camera_get_input_transform (RutObject *object);

void
rut_camera_set_input_transform (RutObject *camera,
                                const cg_matrix_t *input_transform);

void
rut_camera_flush (RutObject *camera);

void
rut_camera_suspend (RutObject *camera);

void
rut_camera_resume (RutObject *camera);

void
rut_camera_end_frame (RutObject *camera);

void
rut_camera_add_input_region (RutObject *camera,
                             RutInputRegion *region);

void
rut_camera_remove_input_region (RutObject *camera,
                                RutInputRegion *region);

c_list_t *
rut_camera_get_input_regions (RutObject *object);

bool
rut_camera_transform_window_coordinate (RutObject *camera,
                                        float *x,
                                        float *y);

void
rut_camera_unproject_coord (RutObject *camera,
                            const cg_matrix_t *modelview,
                            const cg_matrix_t *inverse_modelview,
                            float object_coord_z,
                            float *x,
                            float *y);

cg_primitive_t *
rut_camera_create_frustum_primitive (RutObject *camera);

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


#endif /* _RUT_CAMERA_H_ */
