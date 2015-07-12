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
#include "rut-icon-toggle.h"
#include "rut-bin.h"
#include "rut-icon.h"
#include "rut-composite-sizable.h"
#include "rut-input-region.h"

#include "rut-camera.h"

struct _rut_icon_toggle_t {
    rut_object_base_t _base;

    rut_shell_t *shell;

    bool visual_state;
    bool real_state;

    rut_stack_t *stack;
    rut_bin_t *bin;

    rut_icon_t *icon_set;
    rut_icon_t *icon_unset;

    rut_icon_t *current_icon;

    rut_input_region_t *input_region;
    bool in_grab;

    bool interactive_unset_enabled;

    c_list_t on_toggle_cb_list;

    rut_graphable_props_t graphable;
};

static void
destroy_icons(rut_icon_toggle_t *toggle)
{
    if (toggle->icon_set) {
        rut_object_unref(toggle->icon_set);
        toggle->icon_set = NULL;
    }

    if (toggle->icon_unset) {
        rut_object_unref(toggle->icon_unset);
        toggle->icon_unset = NULL;
    }
}

static void
_rut_icon_toggle_free(void *object)
{
    rut_icon_toggle_t *toggle = object;

    rut_closure_list_disconnect_all_FIXME(&toggle->on_toggle_cb_list);

    destroy_icons(toggle);

    /* NB: This will destroy the stack, layout, label and input_region
     * which we don't hold extra references for... */
    rut_graphable_destroy(toggle);

    rut_object_free(rut_icon_toggle_t, object);
}

#if 0
static rut_property_spec_t
    _rut_icon_toggle_prop_specs[] =
{
    {
        .name = "state",
        .flags = RUT_PROPERTY_FLAG_READWRITE,
        .type = RUT_PROPERTY_TYPE_BOOLEAN,
        .data_offset = offsetof (RutIcnToggle, state),
        .setter.boolean_type = rut_icon_toggle_set_state
    },
    { 0 } /* XXX: Needed for runtime counting of the number of properties */
};
#endif

rut_type_t rut_icon_toggle_type;

static void
_rut_icon_toggle_init_type(void)
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

    rut_type_t *type = &rut_icon_toggle_type;
#define TYPE rut_icon_toggle_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rut_icon_toggle_free);
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

typedef struct _Icontoggle_grab_state_t {
    rut_object_t *camera;
    rut_icon_toggle_t *toggle;
    c_matrix_t transform;
    c_matrix_t inverse_transform;
} Icontoggle_grab_state_t;

static void
update_current_icon(rut_icon_toggle_t *toggle)
{
    rut_icon_t *current;

    if (toggle->visual_state)
        current = toggle->icon_set;
    else
        current = toggle->icon_unset;

    if (toggle->current_icon != current) {
        if (toggle->current_icon)
            rut_bin_set_child(toggle->bin, NULL);
        rut_bin_set_child(toggle->bin, current);
        toggle->current_icon = current;
    }
}

static void
set_visual_state(rut_icon_toggle_t *toggle, bool state)
{
    if (toggle->visual_state == state)
        return;

    toggle->visual_state = state;

    update_current_icon(toggle);
}

static rut_input_event_status_t
_rut_icon_toggle_grab_input_cb(rut_input_event_t *event, void *user_data)
{
    Icontoggle_grab_state_t *state = user_data;
    rut_icon_toggle_t *toggle = state->toggle;

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION) {
        rut_shell_t *shell = toggle->shell;
        if (rut_motion_event_get_action(event) == RUT_MOTION_EVENT_ACTION_UP) {
            rut_shell_ungrab_input(
                shell, _rut_icon_toggle_grab_input_cb, user_data);
            toggle->in_grab = false;

            rut_icon_toggle_set_state(toggle, toggle->visual_state);

            rut_closure_list_invoke(&toggle->on_toggle_cb_list,
                                    rut_icon_toggle_callback_t,
                                    toggle,
                                    toggle->real_state);

            c_slice_free(Icontoggle_grab_state_t, state);

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

            rut_sizable_get_size(toggle, &width, &height);
            if (x < 0 || x > width || y < 0 || y > height)
                set_visual_state(toggle, toggle->real_state);
            else
                set_visual_state(toggle, !toggle->real_state);

            return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static rut_input_event_status_t
_rut_icon_toggle_input_cb(
    rut_input_region_t *region, rut_input_event_t *event, void *user_data)
{
    rut_icon_toggle_t *toggle = user_data;

    if (!toggle->interactive_unset_enabled && toggle->real_state == true)
        return RUT_INPUT_EVENT_STATUS_UNHANDLED;

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION &&
        rut_motion_event_get_action(event) == RUT_MOTION_EVENT_ACTION_DOWN) {
        rut_shell_t *shell = toggle->shell;
        Icontoggle_grab_state_t *state = c_slice_new(Icontoggle_grab_state_t);
        const c_matrix_t *view;

        state->toggle = toggle;
        state->camera = rut_input_event_get_camera(event);
        view = rut_camera_get_view_transform(state->camera);
        state->transform = *view;
        rut_graphable_apply_transform(toggle, &state->transform);
        if (!c_matrix_get_inverse(&state->transform,
                                   &state->inverse_transform)) {
            c_warning("Failed to calculate inverse of toggle transform\n");
            c_slice_free(Icontoggle_grab_state_t, state);
            return RUT_INPUT_EVENT_STATUS_UNHANDLED;
        }

        toggle->in_grab = true;
        rut_shell_grab_input(
            shell, state->camera, _rut_icon_toggle_grab_input_cb, state);

        set_visual_state(toggle, !toggle->real_state);

        return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

rut_icon_toggle_t *
rut_icon_toggle_new(rut_shell_t *shell,
                    const char *set_icon,
                    const char *unset_icon)
{
    rut_icon_toggle_t *toggle = rut_object_alloc0(
        rut_icon_toggle_t, &rut_icon_toggle_type, _rut_icon_toggle_init_type);
    float natural_width, natural_height;

    c_list_init(&toggle->on_toggle_cb_list);

    rut_graphable_init(toggle);

    toggle->shell = shell;

    toggle->interactive_unset_enabled = true;

    toggle->real_state = false;
    toggle->visual_state = false;

    toggle->stack = rut_stack_new(shell, 1, 1);
    rut_graphable_add_child(toggle, toggle->stack);
    rut_object_unref(toggle->stack);

    toggle->bin = rut_bin_new(shell);
    rut_stack_add(toggle->stack, toggle->bin);
    rut_object_unref(toggle->bin);

    rut_icon_toggle_set_set_icon(toggle, set_icon);
    rut_icon_toggle_set_unset_icon(toggle, unset_icon);

    toggle->input_region = rut_input_region_new_rectangle(
        0, 0, 100, 100, _rut_icon_toggle_input_cb, toggle);
    rut_stack_add(toggle->stack, toggle->input_region);
    rut_object_unref(toggle->input_region);

    rut_sizable_get_preferred_width(toggle->stack, -1, NULL, &natural_width);
    rut_sizable_get_preferred_height(
        toggle->stack, natural_width, NULL, &natural_height);
    rut_sizable_set_size(toggle->stack, natural_width, natural_height);

    return toggle;
}

rut_closure_t *
rut_icon_toggle_add_on_toggle_callback(
    rut_icon_toggle_t *toggle,
    rut_icon_toggle_callback_t callback,
    void *user_data,
    rut_closure_destroy_callback_t destroy_cb)
{
    c_return_val_if_fail(callback != NULL, NULL);

    return rut_closure_list_add_FIXME(
        &toggle->on_toggle_cb_list, callback, user_data, destroy_cb);
}

static void
set_icon(rut_icon_toggle_t *toggle, rut_icon_t **icon, const char *icon_name)
{
    if (*icon) {
        rut_object_unref(*icon);
        if (toggle->current_icon == *icon) {
            rut_bin_set_child(toggle->bin, NULL);
            toggle->current_icon = NULL;
        }
    }

    *icon = rut_icon_new(toggle->shell, icon_name);
    update_current_icon(toggle);
}

void
rut_icon_toggle_set_set_icon(rut_icon_toggle_t *toggle, const char *icon)
{
    set_icon(toggle, &toggle->icon_set, icon);
}

void
rut_icon_toggle_set_unset_icon(rut_icon_toggle_t *toggle, const char *icon)
{
    set_icon(toggle, &toggle->icon_unset, icon);
}

void
rut_icon_toggle_set_state(rut_object_t *object, bool state)
{
    rut_icon_toggle_t *toggle = object;
    if (toggle->real_state == state)
        return;

    toggle->real_state = state;

    if (toggle->in_grab)
        set_visual_state(toggle, !toggle->visual_state);
    else
        set_visual_state(toggle, state);

    update_current_icon(toggle);
}

void
rut_icon_toggle_set_interactive_unset_enable(rut_icon_toggle_t *toggle,
                                             bool enabled)
{
    toggle->interactive_unset_enabled = enabled;
}
