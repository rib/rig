/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012,2013  Intel Corporation
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

#include <config.h>

#include <rut.h>

#include "rig-camera-view.h"
#include "rig-selection-tool.h"

typedef struct _entity_state_t entity_state_t;

typedef struct _control_point_t {
    entity_state_t *entity_state;
    float x, y, z;

    rut_transform_t *transform;
    rut_nine_slice_t *marker;
    rut_input_region_t *input_region;

    float position[3]; /* transformed position of the selected entity */
    float screen_pos[2];

} control_point_t;

struct _entity_state_t {
    rig_selection_tool_t *tool;
    rig_entity_t *entity;

    rut_object_t *sizeable;

    c_llist_t *control_points;
};

typedef struct _grab_state_t {
    rig_selection_tool_t *tool;
    entity_state_t *entity_state;
    control_point_t *point;
} grab_state_t;

static rut_input_event_status_t
control_point_grab_cb(rut_input_event_t *event,
                      void *user_data)
{
    grab_state_t *state = user_data;
    rig_selection_tool_t *tool = state->tool;
    // entity_state_t *entity_state = state->entity_state;
    // control_point_t *point = state->point;
    rut_input_event_status_t status = RUT_INPUT_EVENT_STATUS_UNHANDLED;
    rut_motion_event_action_t action;

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_KEY &&
        rut_key_event_get_keysym(event) == RUT_KEY_Escape) {
        rut_shell_ungrab_input(
            tool->view->shell, control_point_grab_cb, state);

        c_slice_free(grab_state_t, state);

        return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

    if (rut_input_event_get_type(event) != RUT_INPUT_EVENT_TYPE_MOTION)
        return RUT_INPUT_EVENT_STATUS_UNHANDLED;

    action = rut_motion_event_get_action(event);

    switch (action) {
    case RUT_MOTION_EVENT_ACTION_MOVE:
    case RUT_MOTION_EVENT_ACTION_UP: {
        // float x = rut_motion_event_get_x (event);
        // float y = rut_motion_event_get_y (event);

        if (action == RUT_MOTION_EVENT_ACTION_UP) {
            if ((rut_motion_event_get_button_state(event) &
                 RUT_BUTTON_STATE_1) == 0) {
                status = RUT_INPUT_EVENT_STATUS_HANDLED;

                rut_shell_ungrab_input(
                    tool->shell, control_point_grab_cb, state);

                c_slice_free(grab_state_t, state);
            }
        } else
            status = RUT_INPUT_EVENT_STATUS_HANDLED;
    } break;

    default:
        break;
    }

    return status;
}

static rut_input_event_status_t
control_point_input_cb(
    rut_input_region_t *region, rut_input_event_t *event, void *user_data)
{
    control_point_t *point = user_data;
    entity_state_t *entity_state = point->entity_state;
    rig_selection_tool_t *tool = entity_state->tool;

    c_return_val_if_fail(tool->selected_entities != NULL,
                         RUT_INPUT_EVENT_STATUS_UNHANDLED);

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION &&
        rut_motion_event_get_action(event) == RUT_MOTION_EVENT_ACTION_DOWN &&
        rut_motion_event_get_button_state(event) == RUT_BUTTON_STATE_1) {
        // float x = rut_motion_event_get_x (event);
        // float y = rut_motion_event_get_y (event);
        grab_state_t *state = c_slice_new0(grab_state_t);

        state->tool = tool;
        state->entity_state = entity_state;
        state->point = point;

        rut_shell_grab_input(tool->shell,
                             rut_input_event_get_camera(event),
                             control_point_grab_cb,
                             state);

        return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
create_dummy_control_points(entity_state_t *entity_state)
{
    rig_selection_tool_t *tool = entity_state->tool;
    cg_texture_t *tex =
        rut_load_texture_from_data_file(tool->shell, "dot.png", NULL);
    control_point_t *point;

    point = c_slice_new0(control_point_t);
    point->entity_state = entity_state;
    point->x = 0;
    point->y = 0;
    point->z = 0;

    point->transform = rut_transform_new(tool->shell);
    rut_graphable_add_child(tool->tool_overlay, point->transform);
    rut_object_unref(point->transform);

    point->marker = rut_nine_slice_new(tool->shell, tex, 0, 0, 0, 0, 10, 10);
    rut_graphable_add_child(point->transform, point->marker);
    rut_object_unref(point->marker);

    point->input_region =
        rut_input_region_new_circle(0, 0, 5, control_point_input_cb, point);
    rut_graphable_add_child(tool->tool_overlay, point->input_region);
    rut_object_unref(point->input_region);
    entity_state->control_points =
        c_llist_prepend(entity_state->control_points, point);

    point = c_slice_new0(control_point_t);
    point->entity_state = entity_state;
    point->x = 100;
    point->y = 0;
    point->z = 0;

    point->transform = rut_transform_new(tool->shell);
    rut_graphable_add_child(tool->tool_overlay, point->transform);
    rut_object_unref(point->transform);

    point->marker = rut_nine_slice_new(tool->shell, tex, 0, 0, 0, 0, 10, 10);
    rut_graphable_add_child(point->transform, point->marker);
    rut_object_unref(point->marker);

    point->input_region =
        rut_input_region_new_circle(0, 0, 5, control_point_input_cb, point);
    rut_graphable_add_child(tool->tool_overlay, point->input_region);
    rut_object_unref(point->input_region);
    entity_state->control_points =
        c_llist_prepend(entity_state->control_points, point);

    cg_object_unref(tex);
}

static void
create_box_control(entity_state_t *entity_state, float x, float y, float z)
{
    rig_selection_tool_t *tool = entity_state->tool;
    cg_texture_t *tex =
        rut_load_texture_from_data_file(tool->shell, "dot.png", NULL);
    control_point_t *point;

    point = c_slice_new0(control_point_t);
    point->entity_state = entity_state;
    point->x = x;
    point->y = y;
    point->z = z;

    point->transform = rut_transform_new(tool->shell);
    rut_graphable_add_child(tool->tool_overlay, point->transform);
    rut_object_unref(point->transform);

    point->marker = rut_nine_slice_new(tool->shell, tex, 0, 0, 0, 0, 10, 10);
    rut_graphable_add_child(point->transform, point->marker);
    rut_object_unref(point->marker);

    point->input_region =
        rut_input_region_new_circle(0, 0, 5, control_point_input_cb, point);
    rut_graphable_add_child(tool->tool_overlay, point->input_region);
    rut_object_unref(point->input_region);
    entity_state->control_points =
        c_llist_prepend(entity_state->control_points, point);

    cg_object_unref(tex);
}

static void
create_sizeable_control_points(entity_state_t *entity_state)
{
    float width, height;

    rut_sizable_get_size(entity_state->sizeable, &width, &height);

    create_box_control(entity_state, 0, 0, 0);
    create_box_control(entity_state, 0, height, 0);
    create_box_control(entity_state, width, height, 0);
    create_box_control(entity_state, width, 0, 0);
}

static void
entity_state_destroy(entity_state_t *entity_state)
{
    c_llist_t *l;

    for (l = entity_state->control_points; l; l = l->next) {
        control_point_t *point = l->data;
        rut_graphable_remove_child(point->input_region);
        rut_graphable_remove_child(point->transform);
    }

    rut_object_unref(entity_state->entity);

    c_slice_free(entity_state_t, entity_state);
}

static bool
match_component_sizeable(rut_object_t *component, void *user_data)
{
    if (rut_object_is(component, RUT_TRAIT_ID_SIZABLE)) {
        rut_object_t **sizeable = user_data;
        *sizeable = component;
        return false; /* break */
    }

    return true; /* continue */
}

static rut_object_t *
find_sizeable_component(rig_entity_t *entity)
{
    rut_object_t *sizeable = NULL;
    rig_entity_foreach_component(entity, match_component_sizeable, &sizeable);
    return sizeable;
}

static void
objects_selection_event_cb(rig_objects_selection_t *selection,
                           rig_objects_selection_event_t event,
                           rut_object_t *object,
                           void *user_data)
{
    rig_selection_tool_t *tool = user_data;
    entity_state_t *entity_state;
    c_llist_t *l;

    if (!tool->active && event == RIG_OBJECTS_SELECTION_ADD_EVENT)
        return;

    if (rut_object_get_type(object) != &rig_entity_type)
        return;

    for (l = tool->selected_entities; l; l = l->next) {
        entity_state = l->data;
        if (entity_state->entity == object)
            break;
    }
    if (l == NULL)
        entity_state = NULL;

    switch (event) {
    case RIG_OBJECTS_SELECTION_ADD_EVENT: {
        c_return_if_fail(entity_state == NULL);

        entity_state = c_slice_new0(entity_state_t);
        entity_state->tool = tool;
        entity_state->entity = rut_object_ref(object);
        entity_state->control_points = NULL;

        tool->selected_entities =
            c_llist_prepend(tool->selected_entities, entity_state);

        entity_state->sizeable = find_sizeable_component(entity_state->entity);
        if (entity_state->sizeable)
            create_sizeable_control_points(entity_state);
        else
            create_dummy_control_points(entity_state);

        break;
    }

    case RIG_OBJECTS_SELECTION_REMOVE_EVENT:
        c_return_if_fail(entity_state != NULL);

        tool->selected_entities =
            c_llist_remove(tool->selected_entities, entity_state);
        entity_state_destroy(entity_state);
        break;
    }
}

rig_selection_tool_t *
rig_selection_tool_new(rig_camera_view_t *view,
                       rut_object_t *overlay)
{
    rig_selection_tool_t *tool = c_slice_new0(rig_selection_tool_t);
    rut_shell_t *shell = view->shell;

    tool->view = view;
    tool->shell = shell;

    /* Note: we don't take a reference on this overlay to avoid creating
     * a circular reference. */
    tool->tool_overlay = overlay;

    tool->camera = view->view_camera;
    tool->camera_component =
        rig_entity_get_component(tool->camera, RIG_COMPONENT_TYPE_CAMERA);

    c_list_init(&tool->selection_event_cb_list);

    /* pipeline to draw the tool */
    tool->default_pipeline = cg_pipeline_new(shell->cg_device);

    return tool;
}

void
rig_selection_tool_set_active(rig_selection_tool_t *tool, bool active)
{
    rig_editor_t *editor = rig_engine_get_editor(tool->view->engine);
    rig_objects_selection_t *selection =
        rig_editor_get_objects_selection(editor);
    c_llist_t *l;

    if (tool->active == active)
        return;

    tool->active = active;

    if (active) {
        tool->objects_selection_closure =
            rig_objects_selection_add_event_callback(selection,
                                                     objects_selection_event_cb,
                                                     tool,
                                                     NULL); /* destroy notify */

        for (l = selection->objects; l; l = l->next) {
            objects_selection_event_cb(
                selection, RIG_OBJECTS_SELECTION_ADD_EVENT, l->data, tool);
        }
    } else {
        for (l = selection->objects; l; l = l->next) {
            objects_selection_event_cb(
                selection, RIG_OBJECTS_SELECTION_REMOVE_EVENT, l->data, tool);
        }

        rut_closure_disconnect_FIXME(tool->objects_selection_closure);
        tool->objects_selection_closure = NULL;
    }
}

static void
get_modelview_matrix(rig_entity_t *camera,
                     rig_entity_t *entity,
                     c_matrix_t *modelview)
{
    rut_object_t *camera_component =
        rig_entity_get_component(camera, RIG_COMPONENT_TYPE_CAMERA);
    *modelview = *rut_camera_get_view_transform(camera_component);

    c_matrix_multiply(modelview, modelview, rig_entity_get_transform(entity));
}

bool
map_window_coords_to_overlay_coord(rut_object_t *camera,      /* 2d ui camera */
                                   rut_object_t *overlay,      /* camera-view
                                                                  overlay */
                                   float *x,
                                   float *y)
{
    c_matrix_t transform;
    c_matrix_t inverse_transform;

    rut_graphable_get_modelview(overlay, camera, &transform);

    if (!c_matrix_get_inverse(&transform, &inverse_transform))
        return false;

    rut_camera_unproject_coord(camera,
                               &transform,
                               &inverse_transform,
                               0, /* object_coord_z */
                               x,
                               y);

    return true;
}

/* Scale from OpenGL normalized device coordinates (ranging from -1 to 1)
 * to Cogl window/framebuffer coordinates (ranging from 0 to buffer-size) with
 * (0,0) being top left. */
#define VIEWPORT_TRANSFORM_X(x, vp_origin_x, vp_width)                         \
    ((((x) + 1.0) * ((vp_width) / 2.0)) + (vp_origin_x))
/* Note: for Y we first flip all coordinates around the X axis while in
 * normalized device coodinates */
#define VIEWPORT_TRANSFORM_Y(y, vp_origin_y, vp_height)                        \
    ((((-(y)) + 1.0) * ((vp_height) / 2.0)) + (vp_origin_y))

void
update_control_point_positions(rig_selection_tool_t *tool,
                               rut_object_t *paint_camera) /* 2d ui camera */
{
    rut_object_t *camera = tool->camera_component;
    c_llist_t *l;

    for (l = tool->selected_entities; l; l = l->next) {
        entity_state_t *entity_state = l->data;
        c_matrix_t transform;
        const c_matrix_t *projection;
        float screen_space[4], x, y;
        const float *viewport;
        c_llist_t *l2;

        get_modelview_matrix(tool->camera, entity_state->entity, &transform);

        projection = rut_camera_get_projection(camera);

        viewport = rut_camera_get_viewport(camera);

        for (l2 = entity_state->control_points; l2; l2 = l2->next) {
            control_point_t *point = l2->data;

            point->position[0] = point->x;
            point->position[1] = point->y;
            point->position[2] = point->z;

            c_matrix_transform_points(&transform,
                                       3, /* num components for input */
                                       sizeof(float) * 3, /* input stride */
                                       point->position,
                                       sizeof(float) * 3, /* output stride */
                                       point->position,
                                       1 /* n_points */);

            /* update the input region, need project the transformed point and
             * do
             * the viewport transform */
            screen_space[0] = point->position[0];
            screen_space[1] = point->position[1];
            screen_space[2] = point->position[2];
            c_matrix_project_points(projection,
                                     3, /* num components for input */
                                     sizeof(float) * 3, /* input stride */
                                     screen_space,
                                     sizeof(float) * 4, /* output stride */
                                     screen_space,
                                     1 /* n_points */);

            /* perspective divide */
            screen_space[0] /= screen_space[3];
            screen_space[1] /= screen_space[3];

            /* apply viewport transform */
            x = VIEWPORT_TRANSFORM_X(screen_space[0], viewport[0], viewport[2]);
            y = VIEWPORT_TRANSFORM_Y(screen_space[1], viewport[1], viewport[3]);

            point->screen_pos[0] = x;
            point->screen_pos[1] = y;

            map_window_coords_to_overlay_coord(
                paint_camera, tool->tool_overlay, &x, &y);

            rut_transform_init_identity(point->transform);
            rut_transform_translate(point->transform, x, y, 0);
            rut_input_region_set_circle(point->input_region, x, y, 10);
        }
    }
}

void
rig_selection_tool_update(rig_selection_tool_t *tool,
                          rut_object_t *paint_camera)
{
    c_return_if_fail(tool->active);

    if (!tool->selected_entities)
        return;

    update_control_point_positions(tool, paint_camera);
}

rut_closure_t *
rig_selection_tool_add_event_callback(
    rig_selection_tool_t *tool,
    rig_selection_tool_event_callback_t callback,
    void *user_data,
    rut_closure_destroy_callback_t destroy_cb)
{
    return rut_closure_list_add_FIXME(
        &tool->selection_event_cb_list, callback, user_data, destroy_cb);
}

void
rig_selection_tool_destroy(rig_selection_tool_t *tool)
{
    c_llist_t *l;

    rut_closure_list_disconnect_all_FIXME(&tool->selection_event_cb_list);

    cg_object_unref(tool->default_pipeline);

    for (l = tool->selected_entities; l; l = l->next)
        entity_state_destroy(l->data);
    c_llist_free(tool->selected_entities);

    c_slice_free(rig_selection_tool_t, tool);
}
