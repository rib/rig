/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013  Intel Corporation.
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

#include "rut-shell.h"
#include "rut-interfaces.h"
#include "rut-text.h"
#include "rut-transform.h"
#include "rut-stack.h"
#include "rut-box-layout.h"
#include "rut-icon-button.h"
#include "rut-bin.h"
#include "rut-icon.h"
#include "rut-composite-sizable.h"
#include "rut-input-region.h"

#include "rut-camera.h"

typedef enum _icon_button_state_t {
    ICON_BUTTON_STATE_NORMAL,
    ICON_BUTTON_STATE_HOVER,
    ICON_BUTTON_STATE_ACTIVE,
    ICON_BUTTON_STATE_ACTIVE_CANCEL,
    ICON_BUTTON_STATE_DISABLED
} icon_button_state_t;

struct _rut_icon_button_t {
    rut_object_base_t _base;

    rut_shell_t *shell;

    icon_button_state_t state;

    rut_stack_t *stack;
    rut_box_layout_t *layout;
    rut_bin_t *bin;

    rut_icon_t *icon_normal;
    rut_icon_t *icon_hover;
    rut_icon_t *icon_active;
    rut_icon_t *icon_disabled;

    rut_icon_t *current_icon;

    rut_text_t *label;
    rut_icon_button_position_t label_position;

    rut_input_region_t *input_region;

    c_list_t on_click_cb_list;

    rut_graphable_props_t graphable;
};

static void
destroy_icons(rut_icon_button_t *button)
{
    if (button->icon_normal) {
        rut_object_unref(button->icon_normal);
        button->icon_normal = NULL;
    }

    if (button->icon_hover) {
        rut_object_unref(button->icon_hover);
        button->icon_hover = NULL;
    }

    if (button->icon_active) {
        rut_object_unref(button->icon_active);
        button->icon_active = NULL;
    }

    if (button->icon_disabled) {
        rut_object_unref(button->icon_disabled);
        button->icon_disabled = NULL;
    }
}

static void
_rut_icon_button_free(void *object)
{
    rut_icon_button_t *button = object;

    rut_closure_list_disconnect_all_FIXME(&button->on_click_cb_list);

    destroy_icons(button);

    /* NB: This will destroy the stack, layout, label and input_region
     * which we don't hold extra references for... */
    rut_graphable_destroy(button);

    rut_object_free(rut_icon_button_t, object);
}

rut_type_t rut_icon_button_type;

static void
_rut_icon_button_init_type(void)
{
    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child remove */
                                                       NULL, /* child add */
                                                       NULL /* parent changed */
    };

    static rut_sizable_vtable_t sizable_vtable = {
        rut_composite_sizable_set_size,
        rut_composite_sizable_get_size,
        rut_composite_sizable_get_preferred_width,
        rut_composite_sizable_get_preferred_height,
        rut_composite_sizable_add_preferred_size_callback
    };

    rut_type_t *type = &rut_icon_button_type;
#define TYPE rut_icon_button_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rut_icon_button_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_GRAPHABLE,
                       offsetof(TYPE, graphable),
                       &graphable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_SIZABLE,
                       0, /* no implied properties */
                       &sizable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_COMPOSITE_SIZABLE,
                       offsetof(TYPE, stack),
                       NULL); /* no vtable */

#undef TYPE
}

typedef struct _icon_button_grab_state_t {
    rut_object_t *camera;
    rut_icon_button_t *button;
    c_matrix_t transform;
    c_matrix_t inverse_transform;
} icon_button_grab_state_t;

static void
update_current_icon(rut_icon_button_t *button)
{
    rut_icon_t *current;

    switch (button->state) {
    case ICON_BUTTON_STATE_NORMAL:
        current = button->icon_normal;
        break;
    case ICON_BUTTON_STATE_HOVER:
        current = button->icon_hover;
        break;
    case ICON_BUTTON_STATE_ACTIVE:
        current = button->icon_active;
        break;
    case ICON_BUTTON_STATE_ACTIVE_CANCEL:
        current = button->icon_normal;
        break;
    case ICON_BUTTON_STATE_DISABLED:
        current = button->icon_disabled;
        break;
    }

    if (button->current_icon != current) {
        if (button->current_icon)
            rut_bin_set_child(button->bin, NULL);
        rut_bin_set_child(button->bin, current);
        button->current_icon = current;
    }
}

static void
set_state(rut_icon_button_t *button, icon_button_state_t state)
{
    if (button->state == state)
        return;

    button->state = state;
    update_current_icon(button);
}

static rut_input_event_status_t
_rut_icon_button_grab_input_cb(rut_input_event_t *event, void *user_data)
{
    icon_button_grab_state_t *state = user_data;
    rut_icon_button_t *button = state->button;

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION) {
        rut_shell_t *shell = button->shell;
        if (rut_motion_event_get_action(event) == RUT_MOTION_EVENT_ACTION_UP) {
            rut_shell_ungrab_input(
                shell, _rut_icon_button_grab_input_cb, user_data);

            /* NB: It's possible the click callbacks could result in the
             * button's last reference being released... */
            rut_object_ref(button);

            rut_closure_list_invoke(&button->on_click_cb_list,
                                    rut_icon_button_click_callback_t,
                                    button);

            c_slice_free(icon_button_grab_state_t, state);

            set_state(button, ICON_BUTTON_STATE_NORMAL);

            rut_object_unref(button);

            return RUT_INPUT_EVENT_STATUS_HANDLED;
        } else if (rut_motion_event_get_action(event) ==
                   RUT_MOTION_EVENT_ACTION_MOVE) {
            float x = rut_motion_event_get_x(event);
            float y = rut_motion_event_get_y(event);
            float width, height;
            rut_object_t *camera = state->camera;

            rut_camera_unproject_coord(camera,
                                       &state->transform,
                                       &state->inverse_transform,
                                       0,
                                       &x,
                                       &y);

            rut_sizable_get_size(button, &width, &height);
            if (x < 0 || x > width || y < 0 || y > height)
                set_state(button, ICON_BUTTON_STATE_ACTIVE_CANCEL);
            else
                set_state(button, ICON_BUTTON_STATE_ACTIVE);

            return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static rut_input_event_status_t
_rut_icon_button_input_cb(
    rut_input_region_t *region, rut_input_event_t *event, void *user_data)
{
    rut_icon_button_t *button = user_data;

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION &&
        rut_motion_event_get_action(event) == RUT_MOTION_EVENT_ACTION_DOWN) {
        rut_shell_t *shell = button->shell;
        icon_button_grab_state_t *state = c_slice_new(icon_button_grab_state_t);
        const c_matrix_t *view;

        state->button = button;
        state->camera = rut_input_event_get_camera(event);
        view = rut_camera_get_view_transform(state->camera);
        state->transform = *view;
        rut_graphable_apply_transform(button, &state->transform);
        if (!c_matrix_get_inverse(&state->transform,
                                   &state->inverse_transform)) {
            c_warning("Failed to calculate inverse of button transform\n");
            c_slice_free(icon_button_grab_state_t, state);
            return RUT_INPUT_EVENT_STATUS_UNHANDLED;
        }

        rut_shell_grab_input(
            shell, state->camera, _rut_icon_button_grab_input_cb, state);

        set_state(button, ICON_BUTTON_STATE_ACTIVE);

        return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
update_layout(rut_icon_button_t *button)
{
    rut_box_layout_packing_t packing;

    switch (button->label_position) {
    case RUT_ICON_BUTTON_POSITION_ABOVE:
        packing = RUT_BOX_LAYOUT_PACKING_BOTTOM_TO_TOP;
        break;
    case RUT_ICON_BUTTON_POSITION_BELOW:
        packing = RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM;
        break;
    case RUT_ICON_BUTTON_POSITION_SIDE: {
        rut_text_direction_t dir = rut_shell_get_text_direction(button->shell);
        if (dir == RUT_TEXT_DIRECTION_LEFT_TO_RIGHT)
            packing = RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT;
        else
            packing = RUT_BOX_LAYOUT_PACKING_RIGHT_TO_LEFT;
        break;
    }
    }

    rut_box_layout_set_packing(button->layout, packing);
}

rut_icon_button_t *
rut_icon_button_new(rut_shell_t *shell,
                    const char *label,
                    rut_icon_button_position_t label_position,
                    const char *normal_icon,
                    const char *hover_icon,
                    const char *active_icon,
                    const char *disabled_icon)
{
    rut_icon_button_t *button = rut_object_alloc0(
        rut_icon_button_t, &rut_icon_button_type, _rut_icon_button_init_type);
    float natural_width, natural_height;

    c_list_init(&button->on_click_cb_list);

    rut_graphable_init(button);

    button->shell = shell;

    button->state = ICON_BUTTON_STATE_NORMAL;

    button->stack = rut_stack_new(shell, 100, 100);
    rut_graphable_add_child(button, button->stack);
    rut_object_unref(button->stack);

    button->layout =
        rut_box_layout_new(shell, RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM);
    rut_stack_add(button->stack, button->layout);
    rut_object_unref(button->layout);

    button->bin = rut_bin_new(shell);
    rut_box_layout_add(button->layout, true, button->bin);
    rut_object_unref(button->bin);

    button->label_position = label_position;

    if (label) {
        rut_bin_t *bin = rut_bin_new(shell);

        rut_bin_set_x_position(bin, RUT_BIN_POSITION_CENTER);

        button->label = rut_text_new_with_text(shell, NULL, label);
        rut_bin_set_child(bin, button->label);
        rut_object_unref(button->label);

        rut_box_layout_add(button->layout, false, bin);
        rut_object_unref(bin);

        update_layout(button);
    }

    rut_icon_button_set_normal(button, normal_icon);
    rut_icon_button_set_hover(button, hover_icon);
    rut_icon_button_set_active(button, active_icon);
    rut_icon_button_set_disabled(button, disabled_icon);

    button->input_region = rut_input_region_new_rectangle(
        0, 0, 100, 100, _rut_icon_button_input_cb, button);
    rut_stack_add(button->stack, button->input_region);
    rut_object_unref(button->input_region);

    rut_sizable_get_preferred_width(button->stack, -1, NULL, &natural_width);
    rut_sizable_get_preferred_height(
        button->stack, natural_width, NULL, &natural_height);
    rut_sizable_set_size(button->stack, natural_width, natural_height);

    return button;
}

rut_closure_t *
rut_icon_button_add_on_click_callback(rut_icon_button_t *button,
                                      rut_icon_button_click_callback_t callback,
                                      void *user_data,
                                      rut_closure_destroy_callback_t destroy_cb)
{
    c_return_val_if_fail(callback != NULL, NULL);

    return rut_closure_list_add_FIXME(
        &button->on_click_cb_list, callback, user_data, destroy_cb);
}

static void
set_icon(rut_icon_button_t *button, rut_icon_t **icon, const char *icon_name)
{
    if (*icon) {
        rut_object_unref(*icon);
        if (button->current_icon == *icon) {
            rut_bin_set_child(button->bin, NULL);
            button->current_icon = NULL;
        }
    }

    *icon = rut_icon_new(button->shell, icon_name);
    update_current_icon(button);
}

void
rut_icon_button_set_normal(rut_icon_button_t *button, const char *icon)
{
    set_icon(button, &button->icon_normal, icon);
}

void
rut_icon_button_set_hover(rut_icon_button_t *button, const char *icon)
{
    set_icon(button, &button->icon_hover, icon);
}

void
rut_icon_button_set_active(rut_icon_button_t *button, const char *icon)
{
    set_icon(button, &button->icon_active, icon);
}

void
rut_icon_button_set_disabled(rut_icon_button_t *button, const char *icon)
{
    set_icon(button, &button->icon_disabled, icon);
}

void
rut_icon_button_set_label_color(rut_icon_button_t *button,
                                const cg_color_t *color)
{
    if (button->label)
        rut_text_set_color(button->label, color);
}
