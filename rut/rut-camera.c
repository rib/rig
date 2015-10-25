/*
 * Rut
 *
 * Rig Utilities
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

#include <rut-config.h>

#include "rut-camera.h"

rut_shell_t *
rut_camera_get_shell(rut_object_t *object)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    return vtable->get_shell(object);
}

void
rut_camera_set_background_color4f(
    rut_object_t *object, float red, float green, float blue, float alpha)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->set_background_color4f(object, red, green, blue, alpha);
}

void
rut_camera_set_background_color(rut_object_t *object,
                                const cg_color_t *color)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->set_background_color(object, color);
}

const cg_color_t *
rut_camera_get_background_color(rut_object_t *object)
{
    rut_camera_props_t *camera =
        rut_object_get_properties(object, RUT_TRAIT_ID_CAMERA);

    return &camera->bg_color;
}

void
rut_camera_set_clear(rut_object_t *object, bool clear)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->set_clear(object, clear);
}

cg_framebuffer_t *
rut_camera_get_framebuffer(rut_object_t *object)
{
    rut_camera_props_t *camera =
        rut_object_get_properties(object, RUT_TRAIT_ID_CAMERA);

    return camera->fb;
}

void
rut_camera_set_framebuffer(rut_object_t *object,
                           cg_framebuffer_t *framebuffer)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->set_framebuffer(object, framebuffer);
}

void
rut_camera_set_viewport(
    rut_object_t *object, float x, float y, float width, float height)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->set_viewport(object, x, y, width, height);
}

void
rut_camera_set_viewport_x(rut_object_t *object, float x)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->set_viewport_x(object, x);
}

void
rut_camera_set_viewport_y(rut_object_t *object, float y)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->set_viewport_y(object, y);
}

void
rut_camera_set_viewport_width(rut_object_t *object, float width)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->set_viewport_width(object, width);
}

void
rut_camera_set_viewport_height(rut_object_t *object, float height)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->set_viewport_height(object, height);
}

const float *
rut_camera_get_viewport(rut_object_t *object)
{
    rut_camera_props_t *camera =
        rut_object_get_properties(object, RUT_TRAIT_ID_CAMERA);

    return camera->viewport;
}

const c_matrix_t *
rut_camera_get_projection(rut_object_t *object)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    return vtable->get_projection(object);
}

void
rut_camera_set_near_plane(rut_object_t *object, float near)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->set_near_plane(object, near);
}

float
rut_camera_get_near_plane(rut_object_t *object)
{
    rut_camera_props_t *camera =
        rut_object_get_properties(object, RUT_TRAIT_ID_CAMERA);

    return camera->near;
}

void
rut_camera_set_far_plane(rut_object_t *object, float far)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->set_far_plane(object, far);
}

float
rut_camera_get_far_plane(rut_object_t *object)
{
    rut_camera_props_t *camera =
        rut_object_get_properties(object, RUT_TRAIT_ID_CAMERA);

    return camera->far;
}

rut_projection_t
rut_camera_get_projection_mode(rut_object_t *object)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    return vtable->get_projection_mode(object);
}

void
rut_camera_set_projection_mode(rut_object_t *object,
                               rut_projection_t projection)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->set_projection_mode(object, projection);
}

void
rut_camera_set_field_of_view(rut_object_t *object, float fov)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->set_field_of_view(object, fov);
}

float
rut_camera_get_field_of_view(rut_object_t *object)
{
    rut_camera_props_t *camera =
        rut_object_get_properties(object, RUT_TRAIT_ID_CAMERA);

    return camera->perspective.fov;
}

void
rut_camera_set_asymmetric_field_of_view(rut_object_t *object,
                                        float left_fov,
                                        float right_fov,
                                        float bottom_fov,
                                        float top_fov)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->set_asymmetric_field_of_view(object,
                                         left_fov,
                                         right_fov,
                                         bottom_fov,
                                         top_fov);
}

void
rut_camera_get_asymmetric_field_of_view(rut_object_t *object,
                                        float *left_fov,
                                        float *right_fov,
                                        float *bottom_fov,
                                        float *top_fov)
{
    rut_camera_props_t *camera =
        rut_object_get_properties(object, RUT_TRAIT_ID_CAMERA);

    *left_fov = camera->asymmetric_perspective.left_fov;
    *right_fov = camera->asymmetric_perspective.right_fov;
    *bottom_fov = camera->asymmetric_perspective.bottom_fov;
    *top_fov = camera->asymmetric_perspective.top_fov;
}

void
rut_camera_set_orthographic_coordinates(
    rut_object_t *object, float x1, float y1, float x2, float y2)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->set_orthographic_coordinates(object, x1, y1, x2, y2);
}

void
rut_camera_get_orthographic_coordinates(
    rut_object_t *object, float *x1, float *y1, float *x2, float *y2)
{
    rut_camera_props_t *camera =
        rut_object_get_properties(object, RUT_TRAIT_ID_CAMERA);

    *x1 = camera->ortho.x1;
    *y1 = camera->ortho.y1;
    *x2 = camera->ortho.x2;
    *y2 = camera->ortho.y2;
}

const c_matrix_t *
rut_camera_get_inverse_projection(rut_object_t *object)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    return vtable->get_inverse_projection(object);
}

void
rut_camera_set_view_transform(rut_object_t *object,
                              const c_matrix_t *view)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->set_view_transform(object, view);
}

const c_matrix_t *
rut_camera_get_view_transform(rut_object_t *object)
{
    rut_camera_props_t *camera =
        rut_object_get_properties(object, RUT_TRAIT_ID_CAMERA);

    return &camera->view;
}

const c_matrix_t *
rut_camera_get_inverse_view_transform(rut_object_t *object)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    return vtable->get_inverse_view_transform(object);
}

const c_matrix_t *
rut_camera_get_input_transform(rut_object_t *object)
{
    rut_camera_props_t *camera =
        rut_object_get_properties(object, RUT_TRAIT_ID_CAMERA);

    return &camera->input_transform;
}

void
rut_camera_set_input_transform(rut_object_t *object,
                               const c_matrix_t *input_transform)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->set_input_transform(object, input_transform);
}

void
rut_camera_flush(rut_object_t *object)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->flush(object);
}

void
rut_camera_suspend(rut_object_t *object)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->suspend(object);
}

void
rut_camera_resume(rut_object_t *object)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->resume(object);
}

void
rut_camera_end_frame(rut_object_t *object)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->end_frame(object);
}

void
rut_camera_add_input_region(rut_object_t *object,
                            rut_input_region_t *region)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->add_input_region(object, region);
}

void
rut_camera_remove_input_region(rut_object_t *object,
                               rut_input_region_t *region)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->remove_input_region(object, region);
}

c_llist_t *
rut_camera_get_input_regions(rut_object_t *object)
{
    rut_camera_props_t *camera =
        rut_object_get_properties(object, RUT_TRAIT_ID_CAMERA);
    return camera->input_regions;
}

bool
rut_camera_transform_window_coordinate(rut_object_t *object, float *x, float *y)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    return vtable->transform_window_coordinate(object, x, y);
}

void
rut_camera_unproject_coord(rut_object_t *object,
                           const c_matrix_t *modelview,
                           const c_matrix_t *inverse_modelview,
                           float object_coord_z,
                           float *x,
                           float *y)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->unproject_coord(
        object, modelview, inverse_modelview, object_coord_z, x, y);
}

cg_primitive_t *
rut_camera_create_frustum_primitive(rut_object_t *object)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    return vtable->create_frustum_primitive(object);
}

void
rut_camera_set_focal_distance(rut_object_t *object, float focal_distance)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->set_focal_distance(object, focal_distance);
}

float
rut_camera_get_focal_distance(rut_object_t *object)
{
    rut_camera_props_t *camera =
        rut_object_get_properties(object, RUT_TRAIT_ID_CAMERA);

    return camera->focal_distance;
}

void
rut_camera_set_depth_of_field(rut_object_t *object, float depth_of_field)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->set_depth_of_field(object, depth_of_field);
}

float
rut_camera_get_depth_of_field(rut_object_t *object)
{
    rut_camera_props_t *camera =
        rut_object_get_properties(object, RUT_TRAIT_ID_CAMERA);

    return camera->depth_of_field;
}

void
rut_camera_set_zoom(rut_object_t *object, float zoom)
{
    rut_camera_vtable_t *vtable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_CAMERA);

    vtable->set_zoom(object, zoom);
}

float
rut_camera_get_zoom(rut_object_t *object)
{
    rut_camera_props_t *camera =
        rut_object_get_properties(object, RUT_TRAIT_ID_CAMERA);

    return camera->zoom;
}
