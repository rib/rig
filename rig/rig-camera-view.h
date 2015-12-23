/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013  Intel Corporation
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

#ifndef _RIG_CAMERA_VIEW_H_
#define _RIG_CAMERA_VIEW_H_

#ifdef ENABLE_OCULUS_RIFT
#include "OVR_CAPI.h"
#endif

#include <rut.h>

/* Forward declare this since there is a circluar header dependency
 * between rig-camera-view.h and rig-engine.h */
typedef struct _rig_camera_view_t rig_camera_view_t;

#include "rig-engine.h"
#include "rig-ui.h"
#include "rig-dof-effect.h"

#include "components/rig-camera.h"

typedef struct _entity_translate_grab_closure_t entity_translate_grab_closure_t;
typedef struct _entities_translate_grab_closure_t
    entities_translate_grab_closure_t;

enum eye_type {
    RIG_EYE_LEFT = 0,
    RIG_EYE_RIGHT = 1,
};

struct eye_frustum {
    float up_tangent;
    float down_tangent;
    float left_tangent;
    float right_tangent;
};

struct eye {
    enum eye_type type;

    cg_texture_2d_t *tex;
    cg_offscreen_t *fb;

    rig_entity_t *camera;
    rut_object_t *camera_component;

    cg_pipeline_t *distort_pipeline;
    int eye_to_source_uv_scale_loc;
    int eye_to_source_uv_offset_loc;
    int eye_rotation_start_loc;
    int eye_rotation_end_loc;

#ifdef ENABLE_OCULUS_RIFT
    //struct eye_frustum eye_frustum;
    ovrFovPort fov;

    ovrEyeRenderDesc render_desc;

    ovrPosef head_pose;
#endif /* ENABLE_OCULUS_RIFT */

    float eye_to_source_uv_scale[2];
    float eye_to_source_uv_offset[2];

    cg_attribute_buffer_t *attrib_buf;
    cg_attribute_t *attribs[6];
    cg_primitive_t *distortion_prim;

    cg_index_buffer_t *index_buf;
    cg_indices_t *indices;

    //float x0, y0, x1, y1;

    float viewport[4];
};

struct rig_hmd {
    struct eye eyes[2];

#ifdef ENABLE_OCULUS_RIFT
    ovrHmd ovr_hmd;
#endif /* ENABLE_OCULUS_RIFT */

    rig_camera_t *composite_camera;
};

struct _rig_camera_view_t {
    rut_object_base_t _base;

    rig_frontend_t *frontend;
    rig_engine_t *engine;

    rut_shell_t *shell;

    rig_ui_t *ui;

    cg_framebuffer_t *fb;

    rut_graphable_props_t graphable;
    rut_paintable_props_t paintable;

    float width, height;

    rig_entity_t *camera;
    rut_object_t *camera_component;

    bool hmd_mode;
    struct rig_hmd hmd;

    cg_primitive_t *debug_triangle;
    cg_pipeline_t *debug_pipeline;

    bool enable_dof;

    int fb_x;
    int fb_y;
};

rig_camera_view_t *rig_camera_view_new(rig_frontend_t *frontend);

void rig_camera_view_set_framebuffer(rig_camera_view_t *view,
                                     cg_framebuffer_t *fb);

void rig_camera_view_set_ui(rig_camera_view_t *view, rig_ui_t *ui);

void rig_camera_view_set_camera_entity(rig_camera_view_t *view,
                                       rig_entity_t *camera);

void rig_camera_view_paint(rig_camera_view_t *view,
                           rut_object_t *renderer);

#endif /* _RIG_CAMERA_VIEW_H_ */
