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

#include <rig-config.h>

#include <clib.h>

#include <cglib/cglib.h>
#include <math.h>

#include <rut.h>

#include "rig-camera.h"
#include "rig-engine.h"
#include "rig-entity.h"
#include "rig-entity-inlines.h"

typedef struct {
    float x, y, z, w;
} Rutvertex4_t;

rut_ui_enum_t _rut_projection_ui_enum = {
    .nick = "Projection",
    .values = { { RUT_PROJECTION_PERSPECTIVE, "Perspective",
                  "Perspective Projection" },
                { RUT_PROJECTION_ORTHOGRAPHIC, "Orthographic",
                  "Orthographic Projection" },
                { 0 } }
};


static cg_user_data_key_t fb_camera_key;

/* This structure delimits the state that will be copied
 * between cameras during _rig_camera_copy()... */
struct camera_properties
{
    rut_camera_props_t base;

    c_matrix_t projection;
    unsigned int projection_age;
    unsigned int projection_cache_age;

    c_matrix_t inverse_projection;
    unsigned int inverse_projection_age;

    unsigned int view_age;

    c_matrix_t inverse_view;
    unsigned int inverse_view_age;

    unsigned int transform_age;
    unsigned int at_suspend_transform_age;
};

struct _rig_camera_t {
    rut_object_base_t _base;

    rut_componentable_props_t component;

    struct camera_properties props;

    rut_introspectable_props_t introspectable;
    rut_property_t properties[RIG_CAMERA_N_PROPS];

    unsigned int in_frame : 1;
    unsigned int suspended : 1;
};

typedef struct _camera_flush_state_t {
    rig_camera_t *current_camera;
    unsigned int transform_age;
} camera_flush_state_t;

static void
free_camera_flush_state(void *user_data)
{
    camera_flush_state_t *state = user_data;
    c_slice_free(camera_flush_state_t, state);
}

static rut_object_t *
_rig_camera_copy(rut_object_t *obj)
{
    rig_camera_t *camera = obj;
    rig_engine_t *engine = rig_component_props_get_engine(&camera->component);
    rig_camera_t *copy = rig_camera_new(engine,
                                        -1, /* ortho/vp width */
                                        -1, /* ortho/vp height */
                                        camera->props.base.fb); /* may be NULL */

    copy->props = camera->props;

    copy->props.base.input_regions = NULL;
    /* TODO: copy input regions */
#warning "TODO: _rig_camera_copy() should copy input regions"

    return copy;
}

void
rig_camera_set_background_color4f(
    rut_object_t *object, float red, float green, float blue, float alpha)
{
    rig_camera_t *camera = object;
    rut_property_context_t *prop_ctx;

    cg_color_init_from_4f(&camera->props.base.bg_color, red, green, blue, alpha);

    prop_ctx = rig_component_props_get_property_context(&camera->component);
    rut_property_dirty(prop_ctx, &camera->properties[RIG_CAMERA_PROP_BG_COLOR]);
}

void
rig_camera_set_background_color(rut_object_t *obj, const cg_color_t *color)
{
    rig_camera_t *camera = obj;
    rut_property_context_t *prop_ctx;

    camera->props.base.bg_color = *color;

    prop_ctx = rig_component_props_get_property_context(&camera->component);
    rut_property_dirty(prop_ctx, &camera->properties[RIG_CAMERA_PROP_BG_COLOR]);
}

const cg_color_t *
rig_camera_get_background_color(rut_object_t *obj)
{
    rig_camera_t *camera = obj;

    return &camera->props.base.bg_color;
}

void
rig_camera_set_clear(rut_object_t *object, bool clear)
{
    rig_camera_t *camera = object;
    if (clear)
        camera->props.base.clear_fb = true;
    else
        camera->props.base.clear_fb = false;
}

cg_framebuffer_t *
rig_camera_get_framebuffer(rut_object_t *object)
{
    rig_camera_t *camera = object;
    return camera->props.base.fb;
}

void
rig_camera_set_framebuffer(rut_object_t *object,
                           cg_framebuffer_t *framebuffer)
{
    rig_camera_t *camera = object;
    if (camera->props.base.fb == framebuffer)
        return;

    if (camera->props.base.fb)
        cg_object_unref(camera->props.base.fb);

    camera->props.base.fb = cg_object_ref(framebuffer);
}

static void
_rig_camera_set_viewport(
    rut_object_t *object, float x, float y, float width, float height)
{
    rig_camera_t *camera = object;
    if (camera->props.base.viewport[0] == x && camera->props.base.viewport[1] == y &&
        camera->props.base.viewport[2] == width &&
        camera->props.base.viewport[3] == height)
        return;

    /* If the aspect ratio changes we may need to update the projection
     * matrix... */
    if (camera->props.base.mode == RUT_PROJECTION_PERSPECTIVE &&
        (camera->props.base.viewport[2] / camera->props.base.viewport[3]) !=
        (width / height))
        camera->props.projection_age++;

    camera->props.base.viewport[0] = x;
    camera->props.base.viewport[1] = y;
    camera->props.base.viewport[2] = width;
    camera->props.base.viewport[3] = height;

    camera->props.transform_age++;
}

void
rig_camera_set_viewport(
    rut_object_t *object, float x, float y, float width, float height)
{
    rig_camera_t *camera = object;
    rut_property_context_t *prop_ctx;

    _rig_camera_set_viewport(camera, x, y, width, height);

    prop_ctx = rig_component_props_get_property_context(&camera->component);
    rut_property_dirty(prop_ctx, &camera->properties[RIG_CAMERA_PROP_VIEWPORT_X]);
    rut_property_dirty(prop_ctx, &camera->properties[RIG_CAMERA_PROP_VIEWPORT_Y]);
    rut_property_dirty(prop_ctx, &camera->properties[RIG_CAMERA_PROP_VIEWPORT_WIDTH]);
    rut_property_dirty(prop_ctx, &camera->properties[RIG_CAMERA_PROP_VIEWPORT_HEIGHT]);
}

void
rig_camera_set_viewport_x(rut_object_t *obj, float x)
{
    rig_camera_t *camera = obj;
    rut_property_context_t *prop_ctx;

    _rig_camera_set_viewport(camera,
                             x,
                             camera->props.base.viewport[1],
                             camera->props.base.viewport[2],
                             camera->props.base.viewport[3]);

    prop_ctx = rig_component_props_get_property_context(&camera->component);
    rut_property_dirty(prop_ctx, &camera->properties[RIG_CAMERA_PROP_VIEWPORT_X]);
}

void
rig_camera_set_viewport_y(rut_object_t *obj, float y)
{
    rig_camera_t *camera = obj;
    rut_property_context_t *prop_ctx;

    _rig_camera_set_viewport(camera,
                             camera->props.base.viewport[0],
                             y,
                             camera->props.base.viewport[2],
                             camera->props.base.viewport[3]);

    prop_ctx = rig_component_props_get_property_context(&camera->component);
    rut_property_dirty(prop_ctx, &camera->properties[RIG_CAMERA_PROP_VIEWPORT_Y]);
}

void
rig_camera_set_viewport_width(rut_object_t *obj, float width)
{
    rig_camera_t *camera = obj;
    rut_property_context_t *prop_ctx;

    _rig_camera_set_viewport(camera,
                             camera->props.base.viewport[0],
                             camera->props.base.viewport[1],
                             width,
                             camera->props.base.viewport[3]);

    prop_ctx = rig_component_props_get_property_context(&camera->component);
    rut_property_dirty(prop_ctx, &camera->properties[RIG_CAMERA_PROP_VIEWPORT_WIDTH]);
}

void
rig_camera_set_viewport_height(rut_object_t *obj, float height)
{
    rig_camera_t *camera = obj;
    rut_property_context_t *prop_ctx;

    _rig_camera_set_viewport(camera,
                             camera->props.base.viewport[0],
                             camera->props.base.viewport[1],
                             camera->props.base.viewport[2],
                             height);

    prop_ctx = rig_component_props_get_property_context(&camera->component);
    rut_property_dirty(prop_ctx, &camera->properties[RIG_CAMERA_PROP_VIEWPORT_HEIGHT]);
}

const float *
rig_camera_get_viewport(rut_object_t *object)
{
    rig_camera_t *camera = object;
    return camera->props.base.viewport;
}

const c_matrix_t *
rig_camera_get_projection(rut_object_t *object)
{
    rig_camera_t *camera = object;
    if (C_UNLIKELY(camera->props.projection_cache_age !=
                   camera->props.projection_age)) {
        c_matrix_init_identity(&camera->props.projection);

        switch (camera->props.base.mode)
        {
        case RUT_PROJECTION_ORTHOGRAPHIC:
            {
                if (camera->props.base.zoom != 1) {
                    float center_x = camera->props.base.ortho.x1 +
                        (camera->props.base.ortho.x2 - camera->props.base.ortho.x1) / 2.0;
                    float center_y = camera->props.base.ortho.y1 +
                        (camera->props.base.ortho.y2 - camera->props.base.ortho.y1) / 2.0;
                    float inverse_scale = 1.0 / camera->props.base.zoom;
                    float dx = (camera->props.base.ortho.x2 - center_x) * inverse_scale;
                    float dy = (camera->props.base.ortho.y2 - center_y) * inverse_scale;

                    camera->props.base.ortho.x1 = center_x - dx;
                    camera->props.base.ortho.x2 = center_x + dx;
                    camera->props.base.ortho.y1 = center_y - dy;
                    camera->props.base.ortho.y2 = center_y + dy;
                }

                c_matrix_orthographic(&camera->props.projection,
                                       camera->props.base.ortho.x1,
                                       camera->props.base.ortho.y1,
                                       camera->props.base.ortho.x2,
                                       camera->props.base.ortho.y2,
                                       camera->props.base.near,
                                       camera->props.base.far);

                break;
            }
        case RUT_PROJECTION_PERSPECTIVE:
            {
                float aspect_ratio =
                    camera->props.base.viewport[2] / camera->props.base.viewport[3];
                rut_util_matrix_scaled_perspective(&camera->props.projection,
                                                   camera->props.base.perspective.fov,
                                                   aspect_ratio,
                                                   camera->props.base.near,
                                                   camera->props.base.far,
                                                   camera->props.base.zoom);
#if 0
                c_debug ("fov=%f, aspect=%f, near=%f, far=%f, zoom=%f\n",
                         camera->props.base.perspective.fov,
                         aspect_ratio,
                         camera->props.base.near,
                         camera->props.base.far,
                         camera->props.base.zoom);
#endif
                break;
            }

        case RUT_PROJECTION_ASYMMETRIC_PERSPECTIVE:
            {
#define D_TO_R(X) (X *(C_PI / 180.0f))
                float near = camera->props.base.near;
                float left = -tanf(D_TO_R(camera->props.base.asymmetric_perspective.left_fov)) * near;
                float right = tanf(D_TO_R(camera->props.base.asymmetric_perspective.right_fov)) * near;
                float bottom = -tanf(D_TO_R(camera->props.base.asymmetric_perspective.bottom_fov)) * near;
                float top = tanf(D_TO_R(camera->props.base.asymmetric_perspective.top_fov)) * near;
#undef D_TO_R

                rut_util_matrix_scaled_frustum(&camera->props.projection,
                                               left,
                                               right,
                                               bottom,
                                               top,
                                               camera->props.base.near,
                                               camera->props.base.far,
                                               camera->props.base.zoom);
                break;
            }
        case RUT_PROJECTION_NDC:
            {
                c_matrix_init_identity(&camera->props.projection);
                break;
            }
        }

        camera->props.projection_cache_age = camera->props.projection_age;
    }

    return &camera->props.projection;
}

void
rig_camera_set_near_plane(rut_object_t *obj, float near)
{
    rig_camera_t *camera = obj;
    rut_property_context_t *prop_ctx;

    if (camera->props.base.near == near)
        return;

    camera->props.base.near = near;

    prop_ctx = rig_component_props_get_property_context(&camera->component);
    rut_property_dirty(prop_ctx, &camera->properties[RIG_CAMERA_PROP_NEAR]);

    camera->props.projection_age++;
    camera->props.transform_age++;
}

float
rig_camera_get_near_plane(rut_object_t *obj)
{
    rig_camera_t *camera = obj;

    return camera->props.base.near;
}

void
rig_camera_set_far_plane(rut_object_t *obj, float far)
{
    rig_camera_t *camera = obj;
    rut_property_context_t *prop_ctx;

    if (camera->props.base.far == far)
        return;

    camera->props.base.far = far;

    prop_ctx = rig_component_props_get_property_context(&camera->component);
    rut_property_dirty(prop_ctx, &camera->properties[RIG_CAMERA_PROP_FAR]);

    camera->props.projection_age++;
    camera->props.transform_age++;
}

float
rig_camera_get_far_plane(rut_object_t *obj)
{
    rig_camera_t *camera = obj;

    return camera->props.base.far;
}

rut_projection_t
rig_camera_get_projection_mode(rut_object_t *object)
{
    rig_camera_t *camera = object;
    return camera->props.base.mode;
}

void
rig_camera_set_projection_mode(rut_object_t *object,
                               rut_projection_t projection)
{
    rig_camera_t *camera = object;
    rut_property_context_t *prop_ctx;

    if (camera->props.base.mode == projection)
        return;

    camera->props.base.mode = projection;

    prop_ctx = rig_component_props_get_property_context(&camera->component);
    rut_property_dirty(prop_ctx, &camera->properties[RIG_CAMERA_PROP_MODE]);

    camera->props.projection_age++;
    camera->props.transform_age++;
}

void
rig_camera_set_field_of_view(rut_object_t *obj, float fov)
{
    rig_camera_t *camera = obj;
    rut_property_context_t *prop_ctx;

    if (camera->props.base.perspective.fov == fov)
        return;

    camera->props.base.perspective.fov = fov;

    prop_ctx = rig_component_props_get_property_context(&camera->component);
    rut_property_dirty(prop_ctx, &camera->properties[RIG_CAMERA_PROP_FOV]);

    if (camera->props.base.mode == RUT_PROJECTION_PERSPECTIVE) {
        camera->props.projection_age++;
        camera->props.transform_age++;
    }
}

float
rig_camera_get_field_of_view(rut_object_t *obj)
{
    rig_camera_t *camera = obj;

    return camera->props.base.perspective.fov;
}

void
rig_camera_set_asymmetric_field_of_view(rut_object_t *object,
                                        float left_fov,
                                        float right_fov,
                                        float bottom_fov,
                                        float top_fov)
{
    rig_camera_t *camera = object;

    if (camera->props.base.asymmetric_perspective.left_fov == left_fov &&
        camera->props.base.asymmetric_perspective.right_fov == right_fov &&
        camera->props.base.asymmetric_perspective.bottom_fov == bottom_fov &&
        camera->props.base.asymmetric_perspective.top_fov == top_fov)
    {
        return;
    }

    camera->props.base.asymmetric_perspective.left_fov = left_fov;
    camera->props.base.asymmetric_perspective.right_fov = right_fov;
    camera->props.base.asymmetric_perspective.bottom_fov = bottom_fov;
    camera->props.base.asymmetric_perspective.top_fov = top_fov;

    if (camera->props.base.mode == RUT_PROJECTION_ASYMMETRIC_PERSPECTIVE) {
        camera->props.projection_age++;
        camera->props.transform_age++;
    }
}

static void
rig_camera_set_orthographic_coordinates_fv(rut_object_t *object, const float ortho_vec[4])
{
    rig_camera_t *camera = object;

    if (camera->props.base.ortho.x1 == ortho_vec[0] &&
        camera->props.base.ortho.y1 == ortho_vec[1] &&
        camera->props.base.ortho.x2 == ortho_vec[2] &&
        camera->props.base.ortho.y2 == ortho_vec[3])
        return;

    memcpy(camera->props.base.ortho_vec, ortho_vec, sizeof(float) * 4);

    if (camera->props.base.mode == RUT_PROJECTION_ORTHOGRAPHIC)
        camera->props.projection_age++;
}
void
rig_camera_set_orthographic_coordinates(
    rut_object_t *object, float x1, float y1, float x2, float y2)
{
    rig_camera_set_orthographic_coordinates_fv(object,
                                               (float [4]){x1, y1, x2, y2});
}

const c_matrix_t *
rig_camera_get_inverse_projection(rut_object_t *object)
{
    rig_camera_t *camera = object;
    const c_matrix_t *projection;

    if (camera->props.inverse_projection_age == camera->props.projection_age)
        return &camera->props.inverse_projection;

    projection = rig_camera_get_projection(camera);

    if (!c_matrix_get_inverse(projection, &camera->props.inverse_projection))
        return NULL;

    camera->props.inverse_projection_age = camera->props.projection_age;
    return &camera->props.inverse_projection;
}

void
rig_camera_set_view_transform(rut_object_t *object,
                              const c_matrix_t *view)
{
    rig_camera_t *camera = object;
    camera->props.base.view = *view;

    camera->props.view_age++;
    camera->props.transform_age++;
}

const c_matrix_t *
rig_camera_get_view_transform(rut_object_t *object)
{
    rig_camera_t *camera = object;
    return &camera->props.base.view;
}

const c_matrix_t *
rig_camera_get_inverse_view_transform(rut_object_t *object)
{
    rig_camera_t *camera = object;

    if (camera->props.inverse_view_age == camera->props.view_age)
        return &camera->props.inverse_view;

    if (!c_matrix_get_inverse(&camera->props.base.view,
                               &camera->props.inverse_view))
        return NULL;

    camera->props.inverse_view_age = camera->props.view_age;
    return &camera->props.inverse_view;
}

void
rig_camera_set_input_transform(rut_object_t *object,
                               const c_matrix_t *input_transform)
{
    rig_camera_t *camera = object;
    camera->props.base.input_transform = *input_transform;
}

void
rig_camera_add_input_region(rut_object_t *object,
                            rut_input_region_t *region)
{
    rig_camera_t *camera = object;
    if (c_llist_find(camera->props.base.input_regions, region))
        return;

    rut_object_ref(region);
    camera->props.base.input_regions =
        c_llist_prepend(camera->props.base.input_regions, region);
}

void
rig_camera_remove_input_region(rut_object_t *object,
                               rut_input_region_t *region)
{
    rig_camera_t *camera = object;
    c_llist_t *link = c_llist_find(camera->props.base.input_regions, region);
    if (link) {
        rut_object_unref(region);
        camera->props.base.input_regions =
            c_llist_delete_link(camera->props.base.input_regions, link);
    }
}

bool
rig_camera_transform_window_coordinate(rut_object_t *object, float *x, float *y)
{
    rig_camera_t *camera = object;
    float *viewport = camera->props.base.viewport;
    *x -= viewport[0];
    *y -= viewport[1];

    if (*x < 0 || *x >= viewport[2] || *y < 0 || *y >= viewport[3])
        return false;
    else
        return true;
}

void
rig_camera_unproject_coord(rut_object_t *object,
                           const c_matrix_t *modelview,
                           const c_matrix_t *inverse_modelview,
                           float object_coord_z,
                           float *x,
                           float *y)
{
    rig_camera_t *camera = object;
    const c_matrix_t *projection = rig_camera_get_projection(camera);
    const c_matrix_t *inverse_projection =
        rig_camera_get_inverse_projection(camera);
    // float z;
    float ndc_x, ndc_y, ndc_z, ndc_w;
    float eye_x, eye_y, eye_z, eye_w;
    const float *viewport = rig_camera_get_viewport(camera);

    /* Convert item z into NDC z */
    {
        // float x = 0, y = 0, z = 0, w = 1;
        float z = 0, w = 1;
        float tmp_x, tmp_y, tmp_z;
        const c_matrix_t *m = modelview;

        tmp_x = m->xw;
        tmp_y = m->yw;
        tmp_z = m->zw;

        m = projection;
        z = m->zx * tmp_x + m->zy * tmp_y + m->zz * tmp_z + m->zw;
        w = m->wx * tmp_x + m->wy * tmp_y + m->wz * tmp_z + m->ww;

        ndc_z = z / w;
    }

    /* Undo the Viewport transform, putting us in Normalized Device Coords */
    ndc_x = (*x - viewport[0]) * 2.0f / viewport[2] - 1.0f;
    ndc_y = ((viewport[3] - 1 + viewport[1] - *y) * 2.0f / viewport[3] - 1.0f);

    /* Undo the Projection, putting us in Eye Coords. */
    ndc_w = 1;
    c_matrix_transform_point(
        inverse_projection, &ndc_x, &ndc_y, &ndc_z, &ndc_w);
    eye_x = ndc_x / ndc_w;
    eye_y = ndc_y / ndc_w;
    eye_z = ndc_z / ndc_w;
    eye_w = 1;

    /* Undo the Modelview transform, putting us in Object Coords */
    c_matrix_transform_point(
        inverse_modelview, &eye_x, &eye_y, &eye_z, &eye_w);

    *x = eye_x;
    *y = eye_y;
    //*z = eye_z;
}

static void
_rig_camera_flush_transforms(rig_camera_t *camera)
{
    const c_matrix_t *projection;
    cg_framebuffer_t *fb = camera->props.base.fb;
    camera_flush_state_t *state;

    /* While a camera is in a suspended state then we don't expect to
     * _flush() and use that camera before it is restored. */
    c_return_if_fail(camera->suspended == false);

    state = cg_object_get_user_data(fb, &fb_camera_key);
    if (!state) {
        state = c_slice_new(camera_flush_state_t);
        cg_object_set_user_data(
            fb, &fb_camera_key, state, free_camera_flush_state);
    } else if (state->current_camera == camera &&
               camera->props.transform_age == state->transform_age)
        goto done;

    if (camera->in_frame) {
        c_warning("Un-balanced rig_camera_flush/_end calls: "
                  "repeat _flush() calls before _end()");
    }

    cg_framebuffer_set_viewport(fb,
                                camera->props.base.viewport[0],
                                camera->props.base.viewport[1],
                                camera->props.base.viewport[2],
                                camera->props.base.viewport[3]);

    projection = rig_camera_get_projection(camera);
    cg_framebuffer_set_projection_matrix(fb, projection);

    cg_framebuffer_set_modelview_matrix(fb, &camera->props.base.view);

    state->current_camera = camera;
    state->transform_age = camera->props.transform_age;

done:
    camera->in_frame = true;
}

void
rig_camera_flush(rut_object_t *object)
{
    rig_camera_t *camera = object;
    _rig_camera_flush_transforms(camera);

    if (camera->props.base.clear_fb) {
        cg_framebuffer_clear4f(camera->props.base.fb,
                               CG_BUFFER_BIT_COLOR | CG_BUFFER_BIT_DEPTH |
                               CG_BUFFER_BIT_STENCIL,
                               camera->props.base.bg_color.red,
                               camera->props.base.bg_color.green,
                               camera->props.base.bg_color.blue,
                               camera->props.base.bg_color.alpha);
    }
}

void
rig_camera_end_frame(rut_object_t *object)
{
    rig_camera_t *camera = object;
    if (C_UNLIKELY(camera->in_frame != true))
        c_warning("Un-balanced rig_camera_flush/end frame calls. "
                  "_end before _flush");
    camera->in_frame = false;
}

void
rig_camera_set_focal_distance(rut_object_t *obj, float focal_distance)
{
    rig_camera_t *camera = obj;
    rut_property_context_t *prop_ctx;
    rut_shell_t *shell;

    if (camera->props.base.focal_distance == focal_distance)
        return;

    camera->props.base.focal_distance = focal_distance;

    shell = rig_component_props_get_shell(&camera->component);
    rut_shell_queue_redraw(shell);

    prop_ctx = rig_component_props_get_property_context(&camera->component);
    rut_property_dirty(prop_ctx, &camera->properties[RIG_CAMERA_PROP_FOCAL_DISTANCE]);
}

float
rig_camera_get_focal_distance(rut_object_t *obj)
{
    rig_camera_t *camera = obj;

    return camera->props.base.focal_distance;
}

void
rig_camera_set_depth_of_field(rut_object_t *obj, float depth_of_field)
{
    rig_camera_t *camera = obj;
    rut_property_context_t *prop_ctx;
    rut_shell_t *shell;

    if (camera->props.base.depth_of_field == depth_of_field)
        return;

    camera->props.base.depth_of_field = depth_of_field;

    shell = rig_component_props_get_shell(&camera->component);
    rut_shell_queue_redraw(shell);

    prop_ctx = rig_component_props_get_property_context(&camera->component);
    rut_property_dirty(prop_ctx, &camera->properties[RIG_CAMERA_PROP_FOCAL_DISTANCE]);
}

float
rig_camera_get_depth_of_field(rut_object_t *obj)
{
    rig_camera_t *camera = obj;

    return camera->props.base.depth_of_field;
}

void
rig_camera_suspend(rut_object_t *object)
{
    rig_camera_t *camera = object;
    camera_flush_state_t *state;

    /* There's not point suspending a frame that hasn't been flushed */
    c_return_if_fail(camera->in_frame == true);

    c_return_if_fail(camera->suspended == false);

    state = cg_object_get_user_data(camera->props.base.fb, &fb_camera_key);

    /* We only expect to be saving a camera that has been flushed */
    c_return_if_fail(state != NULL);

    /* While the camera is in a suspended state we aren't expecting the
     * camera to be touched but we want to double check that at least
     * the transform hasn't been touched when we come to resume the
     * camera... */
    camera->props.at_suspend_transform_age = camera->props.transform_age;

    /* When we resume the camer we'll need to restore the modelview,
     * projection and viewport transforms. The easiest way for us to
     * handle restoring the modelview is to use the framebuffer's
     * matrix stack... */
    cg_framebuffer_push_matrix(camera->props.base.fb);

    camera->suspended = true;
    camera->in_frame = false;
}

void
rig_camera_resume(rut_object_t *object)
{
    rig_camera_t *camera = object;
    camera_flush_state_t *state;
    cg_framebuffer_t *fb = camera->props.base.fb;

    c_return_if_fail(camera->in_frame == false);
    c_return_if_fail(camera->suspended == true);

    /* While a camera is in a suspended state we don't expect the camera
     * to be touched so its transforms shouldn't have changed... */
    c_return_if_fail(camera->props.at_suspend_transform_age ==
                     camera->props.transform_age);

    state = cg_object_get_user_data(fb, &fb_camera_key);

    /* We only expect to be restoring a camera that has been flushed
     * before */
    c_return_if_fail(state != NULL);

    cg_framebuffer_pop_matrix(fb);

    /* If the save turned out to be redundant then we have nothing
     * else to restore... */
    if (state->current_camera == camera)
        goto done;

    cg_framebuffer_set_viewport(fb,
                                camera->props.base.viewport[0],
                                camera->props.base.viewport[1],
                                camera->props.base.viewport[2],
                                camera->props.base.viewport[3]);

    cg_framebuffer_set_projection_matrix(fb, &camera->props.projection);

    state->current_camera = camera;
    state->transform_age = camera->props.transform_age;

done:
    camera->in_frame = true;
    camera->suspended = false;
}

void
rig_camera_set_zoom(rut_object_t *object, float zoom)
{
    rig_camera_t *camera = object;
    rut_property_context_t *prop_ctx;
    rut_shell_t *shell;

    if (camera->props.base.zoom == zoom)
        return;

    camera->props.base.zoom = zoom;

    shell = rig_component_props_get_shell(&camera->component);
    rut_shell_queue_redraw(shell);

    prop_ctx = rig_component_props_get_property_context(&camera->component);
    rut_property_dirty(prop_ctx, &camera->properties[RIG_CAMERA_PROP_ZOOM]);

    camera->props.projection_age++;
    camera->props.transform_age++;
}

float
rig_camera_get_zoom(rut_object_t *object)
{
    rig_camera_t *camera = object;

    return camera->props.base.zoom;
}

rut_shell_t *
rig_camera_get_shell(rut_object_t *object)
{
    rig_camera_t *camera = object;

    return rig_component_props_get_shell(&camera->component);
}

cg_primitive_t *
rig_camera_create_frustum_primitive(rut_object_t *object)
{
    rig_camera_t *camera = object;
    rut_shell_t *shell = rig_component_props_get_shell(&camera->component);
    cg_device_t *dev = shell->cg_device;
    Rutvertex4_t vertices[8] = {
        /* near plane in projection space */
        { -1, -1, -1, 1, },
        { 1, -1, -1, 1, },
        { 1, 1, -1, 1, },
        { -1, 1, -1, 1, },
        /* far plane in projection space */
        { -1, -1, 1, 1, },
        { 1, -1, 1, 1, },
        { 1, 1, 1, 1, },
        { -1, 1, 1, 1, }
    };
    const c_matrix_t *projection_inv;
    cg_attribute_buffer_t *attribute_buffer;
    cg_attribute_t *attributes[1];
    cg_primitive_t *primitive;
    cg_indices_t *indices;
    unsigned char indices_data[24] = { 0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6,
                                       6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7 };
    int i;

    projection_inv = rig_camera_get_inverse_projection(camera);

    for (i = 0; i < 8; i++) {
        c_matrix_transform_point(projection_inv,
                                  &vertices[i].x,
                                  &vertices[i].y,
                                  &vertices[i].z,
                                  &vertices[i].w);
        vertices[i].x /= vertices[i].w;
        vertices[i].y /= vertices[i].w;
        vertices[i].z /= vertices[i].w;
        vertices[i].w /= 1.0f;
    }

    attribute_buffer =
        cg_attribute_buffer_new(dev, 8 * sizeof(Rutvertex4_t), vertices);

    attributes[0] = cg_attribute_new(attribute_buffer,
                                     "cg_position_in",
                                     sizeof(Rutvertex4_t),
                                     offsetof(Rutvertex4_t, x),
                                     3,
                                     CG_ATTRIBUTE_TYPE_FLOAT);

    indices = cg_indices_new(dev,
                             CG_INDICES_TYPE_UNSIGNED_BYTE,
                             indices_data,
                             C_N_ELEMENTS(indices_data));

    primitive = cg_primitive_new_with_attributes(
        CG_VERTICES_MODE_LINES, 8, attributes, 1);

    cg_primitive_set_indices(primitive, indices, C_N_ELEMENTS(indices_data));

    cg_object_unref(attribute_buffer);
    cg_object_unref(attributes[0]);
    cg_object_unref(indices);

    return primitive;
}

static void
_rig_camera_free(void *object)
{
    rig_camera_t *camera = object;

#ifdef RIG_ENABLE_DEBUG
    {
        rut_componentable_props_t *component =
            rut_object_get_properties(object, RUT_TRAIT_ID_COMPONENTABLE);
        c_return_if_fail(!component->parented);
    }
#endif

    if (camera->props.base.fb)
        cg_object_unref(camera->props.base.fb);

    while (camera->props.base.input_regions)
        rig_camera_remove_input_region(camera,
                                       camera->props.base.input_regions->data);

    rut_introspectable_destroy(camera);

    rut_object_free(rig_camera_t, object);
}

static rut_property_spec_t _rig_camera_prop_specs[] = {
    { .name = "mode",
      .nick = "Mode",
      .type = RUT_PROPERTY_TYPE_ENUM,
      .getter.any_type = rig_camera_get_projection_mode,
      .setter.any_type = rig_camera_set_projection_mode,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_VALIDATE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .validation = { .ui_enum = &_rut_projection_ui_enum } },
    { .name = "viewport_x",
      .nick = "Viewport X",
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof(rig_camera_t, props.base.viewport[0]),
      .setter.float_type = rig_camera_set_viewport_x },
    { .name = "viewport_y",
      .nick = "Viewport Y",
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof(rig_camera_t, props.base.viewport[1]),
      .setter.float_type = rig_camera_set_viewport_y },
    { .name = "viewport_width",
      .nick = "Viewport Width",
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof(rig_camera_t, props.base.viewport[2]),
      .setter.float_type = rig_camera_set_viewport_width },
    { .name = "viewport_height",
      .nick = "Viewport Height",
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof(rig_camera_t, props.base.viewport[3]),
      .setter.float_type = rig_camera_set_viewport_height },
    { .name = "ortho",
      .nick = "Orthographic Coordinates",
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .type = RUT_PROPERTY_TYPE_VEC4,
      .data_offset = offsetof(rig_camera_t, props.base.ortho_vec),
      .setter.vec4_type = rig_camera_set_orthographic_coordinates_fv },
    { .name = "fov",
      .nick = "Field Of View",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .getter.float_type = rig_camera_get_field_of_view,
      .setter.float_type = rig_camera_set_field_of_view,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_VALIDATE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .validation = { .float_range = { .min = 1, .max = 135 } },
      .animatable = true },
    { .name = "near",
      .nick = "Near Plane",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .getter.float_type = rig_camera_get_near_plane,
      .setter.float_type = rig_camera_set_near_plane,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .animatable = true },
    { .name = "far",
      .nick = "Far Plane",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .getter.float_type = rig_camera_get_far_plane,
      .setter.float_type = rig_camera_set_far_plane,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .animatable = true },
    { .name = "zoom",
      .nick = "Zoom",
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof(rig_camera_t, props.base.zoom),
      .setter.float_type = rig_camera_set_zoom },
    { .name = "background_color",
      .nick = "Background Color",
      .type = RUT_PROPERTY_TYPE_COLOR,
      .getter.color_type = rig_camera_get_background_color,
      .setter.color_type = rig_camera_set_background_color,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .animatable = true },
    { .name = "clear",
      .nick = "Clear",
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .data_offset = offsetof(rig_camera_t, props.base.clear_fb),
      .setter.boolean_type = rig_camera_set_clear,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
    },
    { .name = "focal_distance",
      .nick = "Focal Distance",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .setter.float_type = rig_camera_set_focal_distance,
      .data_offset = offsetof(rig_camera_t, props.base.focal_distance),
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .animatable = true },
    { .name = "depth_of_field",
      .nick = "Depth Of Field",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .setter.float_type = rig_camera_set_depth_of_field,
      .data_offset = offsetof(rig_camera_t, props.base.depth_of_field),
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .animatable = true },

    /* FIXME: Figure out how to expose the orthographic coordinates as
     * properties? */
    { 0 }
};

rut_type_t rig_camera_type;

void
_rig_camera_init_type(void)
{

    static rut_componentable_vtable_t componentable_vtable = {
        .copy = _rig_camera_copy
    };

    /* TODO: reduce this size of this vtable and go straight to the
     * props for more things...
     */
    static rut_camera_vtable_t camera_vtable = {
        .get_shell = rig_camera_get_shell,
        .set_background_color4f = rig_camera_set_background_color4f,
        .set_background_color = rig_camera_set_background_color,
        .set_clear = rig_camera_set_clear,
        .set_framebuffer = rig_camera_set_framebuffer,
        .set_viewport = rig_camera_set_viewport,
        .set_viewport_x = rig_camera_set_viewport_x,
        .set_viewport_y = rig_camera_set_viewport_y,
        .set_viewport_width = rig_camera_set_viewport_width,
        .set_viewport_height = rig_camera_set_viewport_height,
        .get_projection = rig_camera_get_projection,
        .set_near_plane = rig_camera_set_near_plane,
        .set_far_plane = rig_camera_set_far_plane,
        .get_projection_mode = rig_camera_get_projection_mode,
        .set_projection_mode = rig_camera_set_projection_mode,
        .set_field_of_view = rig_camera_set_field_of_view,
        .set_asymmetric_field_of_view = rig_camera_set_asymmetric_field_of_view,
        .set_orthographic_coordinates = rig_camera_set_orthographic_coordinates,
        .get_inverse_projection = rig_camera_get_inverse_projection,
        .set_view_transform = rig_camera_set_view_transform,
        .get_inverse_view_transform = rig_camera_get_inverse_view_transform,
        .set_input_transform = rig_camera_set_input_transform,
        .flush = rig_camera_flush,
        .suspend = rig_camera_suspend,
        .resume = rig_camera_resume,
        .end_frame = rig_camera_end_frame,
        .add_input_region = rig_camera_add_input_region,
        .remove_input_region = rig_camera_remove_input_region,
        .transform_window_coordinate = rig_camera_transform_window_coordinate,
        .unproject_coord = rig_camera_unproject_coord,
        .create_frustum_primitive = rig_camera_create_frustum_primitive,
        .set_focal_distance = rig_camera_set_focal_distance,
        .set_depth_of_field = rig_camera_set_depth_of_field,
        .set_zoom = rig_camera_set_zoom,
    };

    rut_type_t *type = &rig_camera_type;
#define TYPE rig_camera_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_camera_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_COMPONENTABLE,
                       offsetof(TYPE, component),
                       &componentable_vtable);
    rut_type_add_trait(
        type, RUT_TRAIT_ID_CAMERA, offsetof(TYPE, props), &camera_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INTROSPECTABLE,
                       offsetof(TYPE, introspectable),
                       NULL); /* no implied vtable */

#undef TYPE
}

rig_camera_t *
rig_camera_new(rig_engine_t *engine,
               float width,
               float height,
               cg_framebuffer_t *framebuffer)
{
    rig_camera_t *camera = rut_object_alloc0(
        rig_camera_t, &rig_camera_type, _rig_camera_init_type);

    rut_introspectable_init(camera, _rig_camera_prop_specs, camera->properties);

    camera->component.type = RUT_COMPONENT_TYPE_CAMERA;
    camera->component.parented = false;
    camera->component.engine = engine;

    rig_camera_set_background_color4f(camera, 0, 0, 0, 1);
    camera->props.base.clear_fb = true;

    // rut_graphable_init (camera);

    camera->props.base.mode = RUT_PROJECTION_ORTHOGRAPHIC;
    camera->props.base.ortho.x1 = 0;
    camera->props.base.ortho.y1 = 0;
    camera->props.base.ortho.x2 = width;
    camera->props.base.ortho.y2 = height;

    camera->props.base.viewport[2] = width;
    camera->props.base.viewport[3] = height;

    camera->props.base.near = -1;
    camera->props.base.far = 100;

    camera->props.base.zoom = 1;

    camera->props.base.focal_distance = 30;
    camera->props.base.depth_of_field = 3;

    camera->props.projection_cache_age = -1;
    camera->props.inverse_projection_age = -1;

    c_matrix_init_identity(&camera->props.base.view);
    camera->props.inverse_view_age = -1;

    camera->props.transform_age = 0;

    c_matrix_init_identity(&camera->props.base.input_transform);

    if (framebuffer) {
        int width = cg_framebuffer_get_width(framebuffer);
        int height = cg_framebuffer_get_height(framebuffer);
        camera->props.base.fb = cg_object_ref(framebuffer);
        camera->props.base.viewport[2] = width;
        camera->props.base.viewport[3] = height;
        camera->props.base.ortho.x2 = width;
        camera->props.base.ortho.y2 = height;
    }

    return camera;
}
