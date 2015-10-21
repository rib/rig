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

#ifndef __RIG_ROTATION_TOOL_H__
#define __RIG_ROTATION_TOOL_H__

#include <stdbool.h>

#include <cglib/cglib.h>

#include <rut.h>

#include "rig-camera-view.h"
#include "rig-entity.h"

typedef struct _rig_rotation_tool_t {
    rut_shell_t *shell;

    rig_camera_view_t *view;

    rig_entity_t *camera;
    rut_object_t *camera_component; /* camera component of the camera above */

    bool active;
    rut_closure_t *objects_selection_closure;

    rig_entity_t *selected_entity;

    cg_pipeline_t *default_pipeline;
    cg_primitive_t *rotation_tool;
    cg_primitive_t *rotation_tool_handle;

    rut_input_region_t *rotation_circle;
    rut_arcball_t arcball;
    c_quaternion_t start_rotation;
    c_quaternion_t start_view_rotations;
    bool button_down;
    float position[3]; /* transformed position of the selected entity */
    float screen_pos[2];
    float scale;

    c_list_t rotation_event_cb_list;
} rig_rotation_tool_t;

typedef enum {
    RIG_ROTATION_TOOL_DRAG,
    RIG_ROTATION_TOOL_RELEASE,
    RIG_ROTATION_TOOL_CANCEL
} rig_rotation_tool_event_type_t;

typedef void (*rig_rotation_tool_event_callback_t)(
    rig_rotation_tool_t *tool,
    rig_rotation_tool_event_type_t type,
    const c_quaternion_t *start_rotation,
    const c_quaternion_t *new_rotation,
    void *user_data);

rig_rotation_tool_t *rig_rotation_tool_new(rig_camera_view_t *view);

void rig_rotation_tool_set_active(rig_rotation_tool_t *tool, bool active);

rut_closure_t *
rig_rotation_tool_add_event_callback(rig_rotation_tool_t *tool,
                                     rig_rotation_tool_event_callback_t callback,
                                     void *user_data,
                                     rut_closure_destroy_callback_t destroy_cb);

void rig_rotation_tool_draw(rig_rotation_tool_t *tool, cg_framebuffer_t *fb);

void rig_rotation_tool_destroy(rig_rotation_tool_t *tool);

#endif /* __RIG_ROTATION_TOOL_H__ */
