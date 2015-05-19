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

#include <rut.h>

/* Forward declare this since there is a circluar header dependency
 * between rig-camera-view.h and rig-engine.h */
typedef struct _rig_camera_view_t rig_camera_view_t;

#ifdef RIG_EDITOR_ENABLED
#include "rig-selection-tool.h"
#include "rig-rotation-tool.h"
#endif

#include "rig-engine.h"
#include "rig-ui.h"
#include "rig-dof-effect.h"

#include "components/rig-camera.h"

typedef struct _entity_translate_grab_closure_t entity_translate_grab_closure_t;
typedef struct _entities_translate_grab_closure_t
    entities_translate_grab_closure_t;

typedef struct {
    rig_entity_t *origin_offset; /* negative offset */
    rig_entity_t *dev_scale; /* scale to fit device coords */
    rig_entity_t *screen_pos; /* position screen in edit view */
} rig_camera_view_device_transforms_t;

typedef enum _rig_camera_view_mode_t {
    RIG_CAMERA_VIEW_MODE_PLAY = 1,
    RIG_CAMERA_VIEW_MODE_EDIT,
} rig_camera_view_mode_t;

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

struct _rig_camera_view_t {
    rut_object_base_t _base;

    rig_frontend_t *frontend;
    rig_engine_t *engine;

    rut_shell_t *shell;

    rig_ui_t *ui;

    cg_framebuffer_t *fb;

    bool play_mode;

    rut_graphable_props_t graphable;
    rut_paintable_props_t paintable;

    float width, height;

    cg_pipeline_t *bg_pipeline;

    float origin[3];

    float device_scale;

    entities_translate_grab_closure_t *entities_translate_grab_closure;

    rig_entity_t *play_camera;
    rut_object_t *play_camera_component;

#ifdef RIG_EDITOR_ENABLED
    rig_entity_t *play_camera_handle;
#endif

    bool enable_dof;

    int fb_x;
    int fb_y;

    rig_camera_t *composite_camera;

#ifdef RIG_EDITOR_ENABLED
    rut_graph_t *tool_overlay;
    rig_selection_tool_t *selection_tool;
    rig_rotation_tool_t *rotation_tool;
    rig_tool_id_t tool_id;
#endif
};

extern rut_type_t rig_view_type;

rig_camera_view_t *rig_camera_view_new(rig_frontend_t *frontend);

void rig_camera_view_set_framebuffer(rig_camera_view_t *view,
                                     cg_framebuffer_t *fb);

void rig_camera_view_set_ui(rig_camera_view_t *view, rig_ui_t *ui);

void rig_camera_view_paint(rig_camera_view_t *view,
                           rut_object_t *renderer);

#endif /* _RIG_CAMERA_VIEW_H_ */
