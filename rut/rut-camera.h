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

#include <cglib/cglib.h>

#include "rut-object.h"
#include "rut-interfaces.h"
#include "rut-shell.h"

typedef struct _rut_camera_props_t {

    rut_projection_t mode;

    cg_color_t bg_color;
    bool clear_fb;

    float viewport[4];

    float near, far;

    struct {
        float fov;
    } perspective;
    struct {
        float left_fov;
        float right_fov;
        float bottom_fov;
        float top_fov;
    } asymmetric_perspective;
    union {
        struct {
            float x1, y1, x2, y2;
        } ortho;
        float ortho_vec[4];
    };

    float zoom;

    float focal_distance;
    float depth_of_field;

    c_matrix_t view;

    cg_framebuffer_t *fb;

    c_matrix_t input_transform;
    c_llist_t *input_regions;

} rut_camera_props_t;

typedef struct _rut_camera_vtable_t {
    rut_shell_t *(*get_shell)(rut_object_t *camera);

    void (*set_background_color4f)(
        rut_object_t *camera, float red, float green, float blue, float alpha);

    void (*set_background_color)(rut_object_t *camera, const cg_color_t *color);

    void (*set_clear)(rut_object_t *camera, bool clear);

    void (*set_framebuffer)(rut_object_t *camera,
                            cg_framebuffer_t *framebuffer);

    void (*set_viewport)(
        rut_object_t *camera, float x, float y, float width, float height);

    void (*set_viewport_x)(rut_object_t *camera, float x);

    void (*set_viewport_y)(rut_object_t *camera, float y);

    void (*set_viewport_width)(rut_object_t *camera, float width);

    void (*set_viewport_height)(rut_object_t *camera, float height);

    const c_matrix_t *(*get_projection)(rut_object_t *camera);

    void (*set_near_plane)(rut_object_t *camera, float near);

    void (*set_far_plane)(rut_object_t *camera, float far);

    rut_projection_t (*get_projection_mode)(rut_object_t *camera);

    void (*set_projection_mode)(rut_object_t *camera,
                                rut_projection_t projection);

    void (*set_field_of_view)(rut_object_t *camera, float fov);

    void (*set_asymmetric_field_of_view)(rut_object_t *object,
                                         float left_fov,
                                         float right_fov,
                                         float bottom_fov,
                                         float top_fov);

    void (*set_orthographic_coordinates)(
        rut_object_t *camera, float x1, float y1, float x2, float y2);

    const c_matrix_t *(*get_inverse_projection)(rut_object_t *camera);

    void (*set_view_transform)(rut_object_t *camera, const c_matrix_t *view);

    const c_matrix_t *(*get_inverse_view_transform)(rut_object_t *camera);

    void (*set_input_transform)(rut_object_t *camera,
                                const c_matrix_t *input_transform);

    void (*flush)(rut_object_t *camera);

    void (*suspend)(rut_object_t *camera);

    void (*resume)(rut_object_t *camera);

    void (*end_frame)(rut_object_t *camera);

    void (*add_input_region)(rut_object_t *camera, rut_input_region_t *region);

    void (*remove_input_region)(rut_object_t *camera,
                                rut_input_region_t *region);

    c_llist_t *(*get_input_regions)(rut_object_t *camera);

    bool (*transform_window_coordinate)(rut_object_t *camera,
                                        float *x,
                                        float *y);

    void (*unproject_coord)(rut_object_t *camera,
                            const c_matrix_t *modelview,
                            const c_matrix_t *inverse_modelview,
                            float object_coord_z,
                            float *x,
                            float *y);

    cg_primitive_t *(*create_frustum_primitive)(rut_object_t *camera);

    void (*set_focal_distance)(rut_object_t *camera, float focal_distance);

    void (*set_depth_of_field)(rut_object_t *camera, float depth_of_field);

    void (*set_zoom)(rut_object_t *obj, float zoom);

} rut_camera_vtable_t;

rut_shell_t *rut_camera_get_shell(rut_object_t *camera);

void rut_camera_set_background_color4f(
    rut_object_t *camera, float red, float green, float blue, float alpha);

void rut_camera_set_background_color(rut_object_t *camera,
                                     const cg_color_t *color);

const cg_color_t *rut_camera_get_background_color(rut_object_t *camera);

void rut_camera_set_clear(rut_object_t *camera, bool clear);

cg_framebuffer_t *rut_camera_get_framebuffer(rut_object_t *camera);

void rut_camera_set_framebuffer(rut_object_t *camera,
                                cg_framebuffer_t *framebuffer);

void rut_camera_set_viewport(
    rut_object_t *camera, float x, float y, float width, float height);

void rut_camera_set_viewport_x(rut_object_t *camera, float x);

void rut_camera_set_viewport_y(rut_object_t *camera, float y);

void rut_camera_set_viewport_width(rut_object_t *camera, float width);

void rut_camera_set_viewport_height(rut_object_t *camera, float height);

const float *rut_camera_get_viewport(rut_object_t *camera);

const c_matrix_t *rut_camera_get_projection(rut_object_t *camera);

void rut_camera_set_near_plane(rut_object_t *camera, float near);

float rut_camera_get_near_plane(rut_object_t *camera);

void rut_camera_set_far_plane(rut_object_t *camera, float far);

float rut_camera_get_far_plane(rut_object_t *camera);

rut_projection_t rut_camera_get_projection_mode(rut_object_t *camera);

void rut_camera_set_projection_mode(rut_object_t *camera,
                                    rut_projection_t projection);

void rut_camera_set_field_of_view(rut_object_t *camera, float fov);

float rut_camera_get_field_of_view(rut_object_t *camera);

void rut_camera_get_asymmetric_field_of_view(rut_object_t *object,
                                             float *left_fov,
                                             float *right_fov,
                                             float *bottom_fov,
                                             float *top_fov);

void rut_camera_set_asymmetric_field_of_view(rut_object_t *object,
                                             float left_fov,
                                             float right_fov,
                                             float bottom_fov,
                                             float top_fov);

void rut_camera_set_orthographic_coordinates(
    rut_object_t *camera, float x1, float y1, float x2, float y2);

void rut_camera_get_orthographic_coordinates(
    rut_object_t *camera, float *x1, float *y1, float *x2, float *y2);
const c_matrix_t *rut_camera_get_inverse_projection(rut_object_t *camera);

void rut_camera_set_view_transform(rut_object_t *camera,
                                   const c_matrix_t *view);

const c_matrix_t *rut_camera_get_view_transform(rut_object_t *camera);

const c_matrix_t *rut_camera_get_inverse_view_transform(rut_object_t *camera);

const c_matrix_t *rut_camera_get_input_transform(rut_object_t *object);

void rut_camera_set_input_transform(rut_object_t *camera,
                                    const c_matrix_t *input_transform);

void rut_camera_flush(rut_object_t *camera);

void rut_camera_suspend(rut_object_t *camera);

void rut_camera_resume(rut_object_t *camera);

void rut_camera_end_frame(rut_object_t *camera);

void rut_camera_add_input_region(rut_object_t *camera,
                                 rut_input_region_t *region);

void rut_camera_remove_input_region(rut_object_t *camera,
                                    rut_input_region_t *region);

c_llist_t *rut_camera_get_input_regions(rut_object_t *object);

bool rut_camera_transform_window_coordinate(rut_object_t *camera,
                                            float *x,
                                            float *y);

void rut_camera_unproject_coord(rut_object_t *camera,
                                const c_matrix_t *modelview,
                                const c_matrix_t *inverse_modelview,
                                float object_coord_z,
                                float *x,
                                float *y);

cg_primitive_t *rut_camera_create_frustum_primitive(rut_object_t *camera);

void rut_camera_set_focal_distance(rut_object_t *camera, float focal_distance);

float rut_camera_get_focal_distance(rut_object_t *camera);

void rut_camera_set_depth_of_field(rut_object_t *camera, float depth_of_field);

float rut_camera_get_depth_of_field(rut_object_t *camera);

void rut_camera_set_zoom(rut_object_t *obj, float zoom);

float rut_camera_get_zoom(rut_object_t *camera);

#endif /* _RUT_CAMERA_H_ */
