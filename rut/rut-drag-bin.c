/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013 Intel Corporation.
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
 *
 */

#include <config.h>

#include <math.h>

#include "rut-bin.h"
#include "rut-shim.h"
#include "rut-stack.h"
#include "rut-rectangle.h"
#include "rut-transform.h"
#include "rut-drag-bin.h"
#include "rut-interfaces.h"
#include "rut-composite-sizable.h"
#include "rut-inputable.h"
#include "rut-pickable.h"
#include "rut-input-region.h"
#include "rut-nine-slice.h"
#include "rut-camera.h"
#include "rut-texture-cache.h"

struct _rut_drag_bin_t {
    rut_object_base_t _base;

    rut_shell_t *shell;

    rut_object_t *child;
    rut_object_t *payload;

    rut_stack_t *stack;
    // rut_input_region_t *input_region;
    rut_bin_t *bin;
    rut_rectangle_t *drag_overlay;

    rut_transform_t *transform;
    rut_nine_slice_t *drag_icon;

    bool in_drag;

    rut_graphable_props_t graphable;

    rut_input_region_t *input_region;
};

rut_type_t rut_drag_bin_type;

static void
_rut_drag_bin_free(void *object)
{
    rut_drag_bin_t *bin = object;

    rut_drag_bin_set_child(bin, NULL);
    rut_object_unref(bin->payload);

    if (!bin->in_drag) {
        rut_object_unref(bin->drag_overlay);
        rut_object_unref(bin->transform);
    }

    rut_shell_remove_pre_paint_callback_by_graphable(bin->shell, bin);

    rut_graphable_destroy(bin);

    rut_object_unref(bin->input_region);

    rut_object_free(rut_drag_bin_t, bin);
}

#if 1
static void
_rut_drag_bin_set_size(rut_object_t *object, float width, float height)
{
    rut_drag_bin_t *bin = object;

    rut_sizable_set_size(bin->input_region, width, height);
    rut_composite_sizable_set_size(bin, width, height);
}
#endif

static bool
_rut_drag_bin_pick(rut_object_t *inputable,
                   rut_object_t *camera,
                   const c_matrix_t *modelview,
                   float x,
                   float y)
{
    rut_drag_bin_t *bin = inputable;
    c_matrix_t matrix;

    if (!modelview) {
        matrix = *rut_camera_get_view_transform(camera);
        rut_graphable_apply_transform(inputable, &matrix);
        modelview = &matrix;
    }

    return rut_pickable_pick(bin->input_region, camera, modelview, x, y);
}

static rut_input_event_status_t
_rut_drag_bin_handle_event(rut_object_t *inputable, rut_input_event_t *event)
{
    rut_drag_bin_t *bin = inputable;
    return rut_inputable_handle_event(bin->input_region, event);
}

static void
_rut_drag_bin_init_type(void)
{
    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child removed */
                                                       NULL, /* child addded */
                                                       NULL /* parent changed */
    };
    static rut_sizable_vtable_t sizable_vtable = {
        _rut_drag_bin_set_size,
        // rut_composite_sizable_set_size,
        rut_composite_sizable_get_size,
        rut_composite_sizable_get_preferred_width,
        rut_composite_sizable_get_preferred_height,
        rut_composite_sizable_add_preferred_size_callback
    };
    static rut_pickable_vtable_t pickable_vtable = { _rut_drag_bin_pick, };
    static rut_inputable_vtable_t inputable_vtable = {
        _rut_drag_bin_handle_event
    };

    rut_type_t *type = &rut_drag_bin_type;
#define TYPE rut_drag_bin_t

    rut_type_init(&rut_drag_bin_type, C_STRINGIFY(TYPE), _rut_drag_bin_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_GRAPHABLE,
                       offsetof(rut_drag_bin_t, graphable),
                       &graphable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_SIZABLE,
                       0, /* no implied properties */
                       &sizable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_COMPOSITE_SIZABLE,
                       offsetof(TYPE, stack),
                       NULL); /* no vtable */
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_PICKABLE,
                       0, /* no implied properties */
                       &pickable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INPUTABLE,
                       0, /* no implied properties */
                       &inputable_vtable);

#undef TYPE
}

static void
start_drag(rut_shell_onscreen_t *onscreen, rut_drag_bin_t *bin)
{
    rut_object_t *root;

    if (bin->in_drag)
        return;

    rut_stack_add(bin->stack, bin->drag_overlay);

    root = rut_graphable_get_root(bin);
    rut_stack_add(root, bin->transform);

    rut_shell_onscreen_start_drag(onscreen, bin->payload);
    rut_shell_queue_redraw(bin->shell);
    bin->in_drag = true;
}

static void
cancel_drag(rut_shell_onscreen_t *onscreen, rut_drag_bin_t *bin)
{
    if (!bin->in_drag)
        return;

    rut_graphable_remove_child(bin->drag_overlay);
    rut_graphable_remove_child(bin->transform);

    rut_shell_onscreen_cancel_drag(onscreen);
    rut_shell_queue_redraw(bin->shell);
    bin->in_drag = false;
}

typedef struct _drag_state_t {
    rut_object_t *camera;
    rut_drag_bin_t *bin;
    float press_x;
    float press_y;
} drag_state_t;

static rut_input_event_status_t
_rut_drag_bin_grab_input_cb(rut_input_event_t *event, void *user_data)
{
    drag_state_t *state = user_data;
    rut_drag_bin_t *bin = state->bin;

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION) {
        if (rut_motion_event_get_action(event) == RUT_MOTION_EVENT_ACTION_UP) {
            rut_shell_ungrab_input(bin->shell,
                                   _rut_drag_bin_grab_input_cb, state);

            c_slice_free(drag_state_t, state);

            if (bin->in_drag) {
                rut_shell_onscreen_t *onscreen =
                    rut_input_event_get_onscreen(event);
                rut_shell_onscreen_drop(onscreen);
                cancel_drag(onscreen, bin);
                return RUT_INPUT_EVENT_STATUS_HANDLED;
            } else
                return RUT_INPUT_EVENT_STATUS_UNHANDLED;
        } else if (rut_motion_event_get_action(event) ==
                   RUT_MOTION_EVENT_ACTION_MOVE) {
            float dx = rut_motion_event_get_x(event) - state->press_x;
            float dy = rut_motion_event_get_y(event) - state->press_y;
            float dist = sqrtf(dx * dx + dy * dy);

            if (dist > 20) {
                c_matrix_t transform;

                start_drag(rut_input_event_get_onscreen(event), bin);

                rut_graphable_get_transform(bin, &transform);

                rut_transform_init_identity(bin->transform);
                rut_transform_transform(bin->transform, &transform);

                rut_transform_translate(bin->transform, dx, dy, 0);
            } else
                cancel_drag(rut_input_event_get_onscreen(event), bin);

            rut_shell_queue_redraw(bin->shell);

            return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static rut_input_event_status_t
_rut_drag_bin_input_cb(rut_input_region_t *region,
                       rut_input_event_t *event,
                       void *user_data)
{
    rut_drag_bin_t *bin = user_data;

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION) {
        if (rut_motion_event_get_action(event) ==
            RUT_MOTION_EVENT_ACTION_DOWN &&
            rut_motion_event_get_button_state(event) == RUT_BUTTON_STATE_1) {
            drag_state_t *state = c_slice_new(drag_state_t);

            state->bin = bin;
            state->camera = rut_input_event_get_camera(event);
            state->press_x = rut_motion_event_get_x(event);
            state->press_y = rut_motion_event_get_y(event);

            rut_shell_grab_input(bin->shell,
                                 rut_input_event_get_camera(event),
                                 _rut_drag_bin_grab_input_cb,
                                 state);

            return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

rut_drag_bin_t *
rut_drag_bin_new(rut_shell_t *shell)
{
    rut_drag_bin_t *bin = rut_object_alloc0(
        rut_drag_bin_t, &rut_drag_bin_type, _rut_drag_bin_init_type);

    bin->shell = shell;

    rut_graphable_init(bin);

    bin->in_drag = false;

    bin->stack = rut_stack_new(shell, 1, 1);
    rut_graphable_add_child(bin, bin->stack);
    rut_object_unref(bin->stack);

    bin->input_region =
        rut_input_region_new_rectangle(0, 0, 1, 1, _rut_drag_bin_input_cb, bin);

    bin->bin = rut_bin_new(shell);
    rut_stack_add(bin->stack, bin->bin);
    rut_object_unref(bin->bin);

    bin->drag_overlay = rut_rectangle_new4f(shell, 1, 1, 0.5, 0.5, 0.5,
                                            0.5);

    bin->transform = rut_transform_new(shell);
    bin->drag_icon = rut_nine_slice_new(shell,
        rut_load_texture_from_data_file(shell, "transparency-grid.png", NULL),
        0,
        0,
        0,
        0,
        100,
        100);
    rut_graphable_add_child(bin->transform, bin->drag_icon);
    rut_object_unref(bin->drag_icon);

    return bin;
}

void
rut_drag_bin_set_child(rut_drag_bin_t *bin, rut_object_t *child_widget)
{
    c_return_if_fail(rut_object_get_type(bin) == &rut_drag_bin_type);
    c_return_if_fail(bin->in_drag == false);

    if (bin->child == child_widget)
        return;

    if (bin->child) {
        rut_graphable_remove_child(bin->child);
        rut_object_unref(bin->child);
    }

    bin->child = child_widget;

    if (child_widget) {
        rut_bin_set_child(bin->bin, child_widget);
        rut_object_ref(child_widget);
    }

    rut_shell_queue_redraw(bin->shell);
}

void
rut_drag_bin_set_payload(rut_drag_bin_t *bin, rut_object_t *payload)
{
    if (bin->payload == payload)
        return;

    if (bin->payload)
        rut_object_unref(bin->payload);

    bin->payload = payload;
    if (payload)
        rut_object_ref(payload);
}
