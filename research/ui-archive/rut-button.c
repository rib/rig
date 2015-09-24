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
#include "rut-paintable.h"
#include "rut-transform.h"
#include "rut-input-region.h"
#include "rut-button.h"
#include "rut-camera.h"
#include "rut-nine-slice.h"
#include "rut-texture-cache.h"

#define BUTTON_HPAD 10
#define BUTTON_VPAD 23

typedef enum _button_state_t {
    BUTTON_STATE_NORMAL,
    BUTTON_STATE_HOVER,
    BUTTON_STATE_ACTIVE,
    BUTTON_STATE_ACTIVE_CANCEL,
    BUTTON_STATE_DISABLED
} button_state_t;

struct _rut_button_t {
    rut_object_base_t _base;

    rut_shell_t *shell;

    button_state_t state;

    rut_transform_t *text_transform;
    rut_text_t *text;

    float width;
    float height;

    cg_texture_t *normal_texture;
    cg_texture_t *hover_texture;
    cg_texture_t *active_texture;
    cg_texture_t *disabled_texture;

    rut_nine_slice_t *background_normal;
    rut_nine_slice_t *background_hover;
    rut_nine_slice_t *background_active;
    rut_nine_slice_t *background_disabled;

    cg_color_t text_color;

    rut_input_region_t *input_region;

    c_list_t on_click_cb_list;

    rut_graphable_props_t graphable;
    rut_paintable_props_t paintable;
};

static void
destroy_button_slices(rut_button_t *button)
{
    if (button->background_normal) {
        rut_object_unref(button->background_normal);
        button->background_normal = NULL;
    }

    if (button->background_hover) {
        rut_object_unref(button->background_hover);
        button->background_hover = NULL;
    }

    if (button->background_active) {
        rut_object_unref(button->background_active);
        button->background_active = NULL;
    }

    if (button->background_disabled) {
        rut_object_unref(button->background_disabled);
        button->background_disabled = NULL;
    }
}

static void
_rut_button_free(void *object)
{
    rut_button_t *button = object;

    rut_closure_list_disconnect_all_FIXME(&button->on_click_cb_list);

    destroy_button_slices(button);

    if (button->normal_texture) {
        cg_object_unref(button->normal_texture);
        button->normal_texture = NULL;
    }

    if (button->hover_texture) {
        cg_object_unref(button->hover_texture);
        button->hover_texture = NULL;
    }

    if (button->active_texture) {
        cg_object_unref(button->active_texture);
        button->active_texture = NULL;
    }

    if (button->disabled_texture) {
        cg_object_unref(button->disabled_texture);
        button->disabled_texture = NULL;
    }

    rut_graphable_remove_child(button->text);
    rut_object_unref(button->text);

    rut_graphable_remove_child(button->text_transform);
    rut_object_unref(button->text_transform);

    rut_graphable_destroy(button);

    rut_shell_remove_pre_paint_callback_by_graphable(button->shell,
                                                     button);

    rut_object_free(rut_button_t, object);
}

static void
_rut_button_paint(rut_object_t *object,
                  rut_paint_context_t *paint_ctx)
{
    rut_button_t *button = object;

    switch (button->state) {
    case BUTTON_STATE_NORMAL:
        rut_nine_slice_set_size(
            button->background_normal, button->width, button->height);
        rut_paintable_paint(button->background_normal, paint_ctx);
        break;
    case BUTTON_STATE_HOVER:
        rut_nine_slice_set_size(
            button->background_hover, button->width, button->height);
        rut_paintable_paint(button->background_hover, paint_ctx);
        break;
    case BUTTON_STATE_ACTIVE:
        rut_nine_slice_set_size(
            button->background_active, button->width, button->height);
        rut_paintable_paint(button->background_active, paint_ctx);
        break;
    case BUTTON_STATE_ACTIVE_CANCEL:
        rut_nine_slice_set_size(
            button->background_active, button->width, button->height);
        rut_paintable_paint(button->background_active, paint_ctx);
        break;
    case BUTTON_STATE_DISABLED:
        rut_nine_slice_set_size(
            button->background_disabled, button->width, button->height);
        rut_paintable_paint(button->background_disabled, paint_ctx);
        break;
    }
}

static void
rut_button_get_preferred_width(void *object,
                               float for_height,
                               float *min_width_p,
                               float *natural_width_p)
{
    rut_button_t *button = object;

    rut_sizable_get_preferred_width(
        button->text, for_height, min_width_p, natural_width_p);

    if (min_width_p)
        *min_width_p += BUTTON_HPAD;
    if (natural_width_p)
        *natural_width_p += BUTTON_HPAD;
}

static void
rut_button_get_preferred_height(void *object,
                                float for_width,
                                float *min_height_p,
                                float *natural_height_p)
{
    rut_button_t *button = object;

    rut_sizable_get_preferred_height(
        button->text, for_width, min_height_p, natural_height_p);

    if (min_height_p)
        *min_height_p += BUTTON_VPAD;
    if (natural_height_p)
        *natural_height_p += BUTTON_VPAD;
}

rut_type_t rut_button_type;

static void
_rut_button_init_type(void)
{
    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child remove */
                                                       NULL, /* child add */
                                                       NULL /* parent changed */
    };

    static rut_paintable_vtable_t paintable_vtable = { _rut_button_paint };

    static rut_sizable_vtable_t sizable_vtable = {
        rut_button_set_size,            rut_button_get_size,
        rut_button_get_preferred_width, rut_button_get_preferred_height,
        NULL /* add_preferred_size_callback */
    };

    rut_type_t *type = &rut_button_type;
#define TYPE rut_button_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rut_button_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_GRAPHABLE,
                       offsetof(TYPE, graphable),
                       &graphable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_PAINTABLE,
                       offsetof(TYPE, paintable),
                       &paintable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_SIZABLE,
                       0, /* no implied properties */
                       &sizable_vtable);

#undef TYPE
}

struct grab_state {
    rut_object_t *camera;
    rut_button_t *button;
    c_matrix_t transform;
    c_matrix_t inverse_transform;
};

static rut_input_event_status_t
_rut_button_grab_input_cb(rut_input_event_t *event, void *user_data)
{
    struct grab_state *state = user_data;
    rut_button_t *button = state->button;

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION) {
        rut_shell_t *shell = button->shell;
        if (rut_motion_event_get_action(event) == RUT_MOTION_EVENT_ACTION_UP) {
            rut_shell_ungrab_input(shell, _rut_button_grab_input_cb, user_data);

            rut_closure_list_invoke(
                &button->on_click_cb_list, rut_button_click_callback_t, button);

            c_slice_free(struct grab_state, state);

            button->state = BUTTON_STATE_NORMAL;
            rut_shell_queue_redraw(button->shell);

            return RUT_INPUT_EVENT_STATUS_HANDLED;
        } else if (rut_motion_event_get_action(event) ==
                   RUT_MOTION_EVENT_ACTION_MOVE) {
            float x = rut_motion_event_get_x(event);
            float y = rut_motion_event_get_y(event);
            rut_object_t *camera = state->camera;

            rut_camera_unproject_coord(camera,
                                       &state->transform,
                                       &state->inverse_transform,
                                       0,
                                       &x,
                                       &y);

            if (x < 0 || x > button->width || y < 0 || y > button->height)
                button->state = BUTTON_STATE_ACTIVE_CANCEL;
            else
                button->state = BUTTON_STATE_ACTIVE;

            rut_shell_queue_redraw(button->shell);

            return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static rut_input_event_status_t
_rut_button_input_cb(rut_input_region_t *region,
                     rut_input_event_t *event,
                     void *user_data)
{
    rut_button_t *button = user_data;

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION &&
        rut_motion_event_get_action(event) == RUT_MOTION_EVENT_ACTION_DOWN) {
        rut_shell_t *shell = button->shell;
        struct grab_state *state = c_slice_new(struct grab_state);
        const c_matrix_t *view;

        state->button = button;
        state->camera = rut_input_event_get_camera(event);
        view = rut_camera_get_view_transform(state->camera);
        state->transform = *view;
        rut_graphable_apply_transform(button, &state->transform);
        if (!c_matrix_get_inverse(&state->transform,
                                   &state->inverse_transform)) {
            c_warning("Failed to calculate inverse of button transform\n");
            c_slice_free(struct grab_state, state);
            return RUT_INPUT_EVENT_STATUS_UNHANDLED;
        }

        rut_shell_grab_input(
            shell, state->camera, _rut_button_grab_input_cb, state);
        // button->grab_x = rut_motion_event_get_x (event);
        // button->grab_y = rut_motion_event_get_y (event);

        button->state = BUTTON_STATE_ACTIVE;
        rut_shell_queue_redraw(button->shell);

        return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
_rut_button_allocate_cb(rut_object_t *graphable, void *user_data)
{
    rut_button_t *button = graphable;
    float text_min_width, text_natural_width;
    float text_min_height, text_natural_height;
    int text_width, text_height;
    int text_x, text_y;

    rut_sizable_get_preferred_width(
        button->text, -1, &text_min_width, &text_natural_width);

    rut_sizable_get_preferred_height(
        button->text, -1, &text_min_height, &text_natural_height);

    if (button->width > (BUTTON_HPAD + text_natural_width))
        text_width = text_natural_width;
    else
        text_width = MAX(0, button->width - BUTTON_HPAD);

    if (button->height > (BUTTON_VPAD + text_natural_height))
        text_height = text_natural_height;
    else
        text_height = MAX(0, button->height - BUTTON_VPAD);

    rut_sizable_set_size(button->text, text_width, text_height);

    rut_transform_init_identity(button->text_transform);

    text_x = (button->width / 2) - (text_width / 2.0);
    text_y = (button->height / 2) - (text_height / 2.0);
    rut_transform_translate(button->text_transform, text_x, text_y, 0);
}

static void
queue_allocation(rut_button_t *button)
{
    rut_shell_add_pre_paint_callback(button->shell,
                                     button,
                                     _rut_button_allocate_cb,
                                     NULL /* user_data */);
}

rut_button_t *
rut_button_new(rut_shell_t *shell, const char *label)
{
    rut_button_t *button = rut_object_alloc0(
        rut_button_t, &rut_button_type, _rut_button_init_type);
    c_error_t *error = NULL;
    float text_width, text_height;

    c_list_init(&button->on_click_cb_list);

    rut_graphable_init(button);
    rut_paintable_init(button);

    button->shell = shell;

    button->state = BUTTON_STATE_NORMAL;

    button->normal_texture =
        rut_load_texture_from_data_file(shell, "button.png", &error);
    if (button->normal_texture) {
        button->background_normal = rut_nine_slice_new(shell,
                                                       button->normal_texture,
                                                       11,
                                                       5,
                                                       13,
                                                       5,
                                                       button->width,
                                                       button->height);
    } else {
        c_warning("Failed to load button texture: %s", error->message);
        c_error_free(error);
    }

    button->hover_texture =
        rut_load_texture_from_data_file(shell, "button-hover.png", &error);
    if (button->hover_texture) {
        button->background_hover = rut_nine_slice_new(shell,
                                                      button->hover_texture,
                                                      11,
                                                      5,
                                                      13,
                                                      5,
                                                      button->width,
                                                      button->height);
    } else {
        c_warning("Failed to load button-hover texture: %s", error->message);
        c_error_free(error);
    }

    button->active_texture =
        rut_load_texture_from_data_file(shell, "button-active.png", &error);
    if (button->active_texture) {
        button->background_active = rut_nine_slice_new(shell,
                                                       button->active_texture,
                                                       11,
                                                       5,
                                                       13,
                                                       5,
                                                       button->width,
                                                       button->height);
    } else {
        c_warning("Failed to load button-active texture: %s", error->message);
        c_error_free(error);
    }

    button->disabled_texture =
        rut_load_texture_from_data_file(shell, "button-disabled.png", &error);
    if (button->disabled_texture) {
        button->background_disabled =
            rut_nine_slice_new(shell,
                               button->disabled_texture,
                               11,
                               5,
                               13,
                               5,
                               button->width,
                               button->height);
    } else {
        c_warning("Failed to load button-disabled texture: %s", error->message);
        c_error_free(error);
    }

    button->text = rut_text_new_with_text(shell, NULL, label);
    button->text_transform = rut_transform_new(shell);
    rut_graphable_add_child(button, button->text_transform);
    rut_graphable_add_child(button->text_transform, button->text);

    rut_sizable_get_size(button->text, &text_width, &text_height);
    button->width = text_width + BUTTON_HPAD;
    button->height = text_height + BUTTON_VPAD;

    cg_color_init_from_4f(&button->text_color, 0, 0, 0, 1);

    button->input_region = rut_input_region_new_rectangle(
        0, 0, button->width, button->height, _rut_button_input_cb, button);

    // rut_input_region_set_graphable (button->input_region, button);
    rut_graphable_add_child(button, button->input_region);

    queue_allocation(button);

    return button;
}

rut_closure_t *
rut_button_add_on_click_callback(rut_button_t *button,
                                 rut_button_click_callback_t callback,
                                 void *user_data,
                                 rut_closure_destroy_callback_t destroy_cb)
{
    c_return_val_if_fail(callback != NULL, NULL);

    return rut_closure_list_add_FIXME(
        &button->on_click_cb_list, callback, user_data, destroy_cb);
}

void
rut_button_set_size(rut_object_t *self, float width, float height)
{
    rut_button_t *button = self;

    if (button->width == width && button->height == height)
        return;

    button->width = width;
    button->height = height;

    rut_input_region_set_rectangle(
        button->input_region, 0, 0, button->width, button->height);

    queue_allocation(button);
}

void
rut_button_get_size(rut_object_t *self, float *width, float *height)
{
    rut_button_t *button = self;
    *width = button->width;
    *height = button->height;
}
