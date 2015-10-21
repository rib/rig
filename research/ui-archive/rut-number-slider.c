/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012 Intel Corporation.
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

#include <cglib/cglib.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <errno.h>

#include "rut-number-slider.h"
#include "rut-text.h"
#include "rut-composite-sizable.h"
#include "rut-input-region.h"

enum {
    RUT_NUMBER_SLIDER_PROP_VALUE,
    RUT_NUMBER_SLIDER_N_PROPS
};

struct _rut_number_slider_t {
    rut_object_base_t _base;

    rut_shell_t *shell;

    rut_graphable_props_t graphable;

    char *markup_label;

    int width, height;

    int decimal_places;

    float min_value, max_value;
    float value;
    float step;

    rut_text_t *text;

    rut_input_region_t *input_region;

    rut_introspectable_props_t introspectable;
    rut_property_t properties[RUT_NUMBER_SLIDER_N_PROPS];
};

static rut_input_event_status_t
rut_number_slider_text_grab_cb(rut_input_event_t *event, void *user_data);

rut_type_t rut_number_slider_type;

static rut_property_spec_t _rut_number_slider_prop_specs[] = {
    { .name = "value",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof(rut_number_slider_t, value),
      .setter.float_type = rut_number_slider_set_value },
    { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rut_number_slider_free(void *object)
{
    rut_number_slider_t *slider = object;

    rut_graphable_remove_child(slider->input_region);
    rut_object_unref(slider->input_region);

    rut_introspectable_destroy(slider);
    rut_graphable_destroy(slider);

    if (slider->markup_label)
        c_free(slider->markup_label);

    rut_object_free(rut_number_slider_t, slider);
}

typedef struct _edit_state_t {
    rut_number_slider_t *slider;

    rut_object_t *camera;

    rut_closure_t *activate_closure;

    /* This is set to true after we get a motion event with the down
     * action regardless of where was clicked */
    bool button_down;
    /* This is set to true if cursor has moved more than a couple of
     * pixels since the button was pressed. Once this happens the press
     * is no longer considered a click but is instead interpreted as a
     * drag to change the value */
    bool button_drag;
    /* Where within the widget the cursor was when the button was
     * originally pressed */
    float button_x, button_y;

    /* The original value when the button was pressed */
    float button_value;

} edit_state_t;

static void
update_text(rut_number_slider_t *slider)
{
    char *label = slider->markup_label ? slider->markup_label : "";
    char *text =
        c_strdup_printf("%s%.*f", label, slider->decimal_places, slider->value);
    rut_text_set_markup(slider->text, text);
    c_free(text);
}

static void
end_text_edit(edit_state_t *state)
{
    rut_number_slider_t *slider = state->slider;

    if (state->activate_closure)
        rut_closure_disconnect_FIXME(state->activate_closure);

    rut_selectable_cancel(slider->text);
    rut_text_set_editable(slider->text, false);

    update_text(slider);

    rut_shell_ungrab_input(
        slider->shell, rut_number_slider_text_grab_cb, state);

    rut_object_unref(slider);
    c_slice_free(edit_state_t, state);
}

static void
rut_number_slider_commit_text(rut_number_slider_t *slider)
{
    const char *text = rut_text_get_text(slider->text);
    double value;

    errno = 0;
    value = strtod(text, NULL);

    if (errno == 0)
        rut_number_slider_set_value(slider, value);
}

static rut_input_event_status_t
rut_number_slider_text_grab_cb(rut_input_event_t *event, void *user_data)
{
    edit_state_t *state = user_data;
    rut_number_slider_t *slider = state->slider;
    float x, y;

    switch (rut_input_event_get_type(event)) {
    case RUT_INPUT_EVENT_TYPE_MOTION:
        /* Check if this is a click outside of the text control */
        if (rut_motion_event_get_action(event) ==
            RUT_MOTION_EVENT_ACTION_DOWN &&
            (!rut_motion_event_unproject(event, slider, &x, &y) || x < 0 ||
             x >= slider->width || y < 0 || y >= slider->height)) {
            rut_number_slider_commit_text(slider);
            end_text_edit(state);
        }
        break;

    case RUT_INPUT_EVENT_TYPE_KEY:
        /* The escape key cancels the text control */
        if (rut_key_event_get_action(event) == RUT_KEY_EVENT_ACTION_DOWN &&
            rut_key_event_get_keysym(event) == RUT_KEY_Escape) {
            end_text_edit(state);
        }
        break;

    default:
        break;
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
rut_number_slider_text_activate_cb(rut_text_t *text,
                                   void *user_data)
{
    edit_state_t *state = user_data;
    rut_number_slider_t *slider = state->slider;

    rut_number_slider_commit_text(slider);
    end_text_edit(state);
}

static void
start_text_edit(edit_state_t *state)
{
    rut_number_slider_t *slider = state->slider;
    char *text = c_strdup_printf("%.*f", slider->decimal_places, slider->value);
    rut_text_set_markup(slider->text, text);
    c_free(text);

    rut_text_set_editable(slider->text, true);
    rut_text_set_cursor_position(slider->text, 0);
    rut_text_set_selection_bound(slider->text, -1);
    rut_text_grab_key_focus(slider->text);

    state->activate_closure =
        rut_text_add_activate_callback(slider->text,
                                       rut_number_slider_text_activate_cb,
                                       state,
                                       NULL /* destroy_cb */);

    rut_shell_grab_input(slider->shell,
                         state->camera,
                         rut_number_slider_text_grab_cb,
                         state);

    rut_shell_queue_redraw(slider->shell);
}

static rut_input_event_status_t
rut_number_slider_grab_input_cb(rut_input_event_t *event, void *user_data)
{
    edit_state_t *state = user_data;
    rut_number_slider_t *slider = state->slider;
    float x, y;

    if (rut_input_event_get_type(event) != RUT_INPUT_EVENT_TYPE_MOTION)
        return RUT_INPUT_EVENT_STATUS_UNHANDLED;

    x = rut_motion_event_get_x(event);
    y = rut_motion_event_get_y(event);

    /* If the cursor has moved at least a pixel since it was clicked
     * then we will mark the button as a drag event so that we don't
     * intepret it as a click when the button is released */
    if (fabsf(x - state->button_x) >= 1.0f ||
        fabsf(y - state->button_y) >= 1.0f)
        state->button_drag = true;

    /* Update the value based on the position if we're in a drag */
    if (state->button_drag)
        rut_number_slider_set_value(
            slider, state->button_value + (x - state->button_x) * slider->step);

    if ((rut_motion_event_get_button_state(event) & RUT_BUTTON_STATE_1) == 0) {
        state->button_down = false;

        /* If we weren't dragging then this must have been an attempt to
         * click somewhere on the widget */
        if (!state->button_drag)
            start_text_edit(state);
        else {
            rut_object_unref(slider);
            c_slice_free(edit_state_t, state);
        }

        rut_shell_queue_redraw(slider->shell);
    }

    return RUT_INPUT_EVENT_STATUS_HANDLED;
}

static rut_input_event_status_t
rut_number_slider_input_region_cb(
    rut_input_region_t *region, rut_input_event_t *event, void *user_data)
{
    rut_number_slider_t *slider = user_data;

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION &&
        rut_motion_event_get_action(event) == RUT_MOTION_EVENT_ACTION_DOWN &&
        rut_motion_event_get_button_state(event) & RUT_BUTTON_STATE_1) {
        edit_state_t *state = c_slice_new0(edit_state_t);

        state->slider = rut_object_ref(slider);
        state->camera = rut_input_event_get_camera(event);
        state->button_down = true;
        state->button_drag = false;
        state->button_value = slider->value;
        state->button_x = rut_motion_event_get_x(event);
        state->button_y = rut_motion_event_get_y(event);

        rut_shell_grab_pointer(slider->shell,
                               state->camera,
                               rut_number_slider_grab_input_cb,
                               state);

        rut_shell_queue_redraw(slider->shell);

        return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
_rut_number_slider_set_size(rut_object_t *object, float width, float height)
{
    rut_number_slider_t *slider = object;

    rut_composite_sizable_set_size(object, width, height);

    slider->width = width;
    slider->height = height;

    rut_input_region_set_rectangle(slider->input_region,
                                   0.0f,
                                   0.0f, /* x0 / y0 */
                                   slider->width,
                                   slider->height /* x1 / y1 */);
}

static void
_rut_number_slider_init_type(void)
{
    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child removed */
                                                       NULL, /* child addded */
                                                       NULL /* parent changed */
    };

    static rut_sizable_vtable_t sizable_vtable = {
        _rut_number_slider_set_size,
        rut_composite_sizable_get_size,
        rut_composite_sizable_get_preferred_width,
        rut_composite_sizable_get_preferred_height,
        rut_composite_sizable_add_preferred_size_callback
    };

    rut_type_t *type = &rut_number_slider_type;
#define TYPE rut_number_slider_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rut_number_slider_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_GRAPHABLE,
                       offsetof(TYPE, graphable),
                       &graphable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INTROSPECTABLE,
                       offsetof(TYPE, introspectable),
                       NULL); /* no implied vtable */
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_SIZABLE,
                       0, /* no implied properties */
                       &sizable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_COMPOSITE_SIZABLE,
                       offsetof(TYPE, text),
                       NULL); /* no vtable */

#undef TYPE
}

rut_number_slider_t *
rut_number_slider_new(rut_shell_t *shell)
{
    rut_number_slider_t *slider =
        rut_object_alloc0(rut_number_slider_t,
                          &rut_number_slider_type,
                          _rut_number_slider_init_type);

    slider->shell = shell;
    slider->step = 1.0f;
    slider->decimal_places = 2;

    slider->max_value = G_MAXFLOAT;

    rut_graphable_init(slider);

    rut_introspectable_init(
        slider, _rut_number_slider_prop_specs, slider->properties);

    slider->text = rut_text_new(shell);
    rut_text_set_use_markup(slider->text, true);
    rut_text_set_editable(slider->text, false);
    rut_text_set_activatable(slider->text, true);
    rut_graphable_add_child(slider, slider->text);
    rut_object_unref(slider->text);

    slider->input_region = rut_input_region_new_rectangle(
        0, 0, 0, 0, rut_number_slider_input_region_cb, slider);
    rut_graphable_add_child(slider, slider->input_region);

    update_text(slider);

    rut_sizable_set_size(slider, 60, 30);

    return slider;
}

void
rut_number_slider_set_markup_label(rut_number_slider_t *slider,
                                   const char *markup)
{
    c_free(slider->markup_label);
    slider->markup_label = NULL;

    if (markup)
        slider->markup_label = c_strdup(markup);
}

void
rut_number_slider_set_min_value(rut_number_slider_t *slider,
                                float min_value)
{
    slider->min_value = min_value;
    rut_number_slider_set_value(slider, slider->value);
}

void
rut_number_slider_set_max_value(rut_number_slider_t *slider,
                                float max_value)
{
    slider->max_value = max_value;
    rut_number_slider_set_value(slider, slider->value);
}

void
rut_number_slider_set_value(rut_object_t *obj, float value)
{
    rut_number_slider_t *slider = obj;

    value = CLAMP(value, slider->min_value, slider->max_value);

    c_assert(!isnan(value));

    if (value == slider->value)
        return;

    slider->value = value;

    update_text(slider);

    rut_property_dirty(&slider->shell->property_ctx,
                       &slider->properties[RUT_NUMBER_SLIDER_PROP_VALUE]);

    rut_shell_queue_redraw(slider->shell);
}

float
rut_number_slider_get_value(rut_number_slider_t *slider)
{
    return slider->value;
}

void
rut_number_slider_set_step(rut_number_slider_t *slider, float step)
{
    slider->step = step;
}

int
rut_number_slider_get_decimal_places(rut_number_slider_t *slider)
{
    return slider->decimal_places;
}

void
rut_number_slider_set_decimal_places(rut_number_slider_t *slider,
                                     int decimal_places)
{
    rut_shell_queue_redraw(slider->shell);

    slider->decimal_places = decimal_places;
    update_text(slider);
}
