/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012,2013 Intel Corporation.
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

#include "rut-toggle.h"
#include "rut-interfaces.h"
#include "rut-paintable.h"
#include "rut-camera.h"
#include "rut-inputable.h"
#include "rut-pickable.h"
#include "rut-input-region.h"
#include "rut-introspectable.h"
#include "rut-camera.h"
#include "rut-texture-cache.h"
#include "rut-util.h"

#define RUT_TOGGLE_BOX_WIDTH 15
#define RUT_TOGGLE_BOX_RIGHT_PAD 5
#define RUT_TOGGLE_LABEL_VPAD 23
#define RUT_TOGGLE_MIN_LABEL_WIDTH 30

#define RUT_TOGGLE_MIN_WIDTH ((RUT_TOGGLE_BOX_WIDTH + \
                               RUT_TOGGLE_BOX_RIGHT_PAD + \
                               RUT_TOGGLE_MIN_LABEL_WIDTH)

enum {
    RUT_TOGGLE_PROP_STATE,
    RUT_TOGGLE_PROP_ENABLED,
    RUT_TOGGLE_PROP_TICK,
    RUT_TOGGLE_PROP_TICK_COLOR,
    RUT_TOGGLE_N_PROPS
};

struct _rut_toggle_t {
    rut_object_base_t _base;

    rut_shell_t *shell;

    bool state;
    bool enabled;

    /* While we have the input grabbed we want to reflect what
     * the state will be when the mouse button is released
     * without actually changing the state... */
    bool tentative_set;

    /* FIXME: we don't need a separate tick for every toggle! */
    PangoLayout *tick;

    cg_texture_t *selected_icon;
    cg_texture_t *unselected_icon;

    PangoLayout *label;
    int label_width;
    int label_height;

    float width;
    float height;

    /* FIXME: we should be able to share these pipelines
     * between different toggle boxes. */
    cg_pipeline_t *pipeline_border;
    cg_pipeline_t *pipeline_box;
    cg_pipeline_t *pipeline_selected_icon;
    cg_pipeline_t *pipeline_unselected_icon;

    cg_color_t text_color;
    cg_color_t tick_color;

    rut_input_region_t *input_region;

    c_list_t on_toggle_cb_list;

    rut_graphable_props_t graphable;
    rut_paintable_props_t paintable;

    rut_introspectable_props_t introspectable;
    rig_property_t properties[RUT_TOGGLE_N_PROPS];
};

static rig_property_spec_t _rut_toggle_prop_specs[] = {
    { .name = "state",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .data_offset = offsetof(rut_toggle_t, state),
      .setter.boolean_type = rut_toggle_set_state },
    { .name = "enabled",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .data_offset = offsetof(rut_toggle_t, state),
      .setter.boolean_type = rut_toggle_set_enabled },
    { .name = "tick",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_TEXT,
      .setter.text_type = rut_toggle_set_tick,
      .getter.text_type = rut_toggle_get_tick },
    { .name = "tick_color",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_COLOR,
      .setter.color_type = rut_toggle_set_tick_color,
      .getter.color_type = rut_toggle_get_tick_color },
    { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rut_toggle_free(void *object)
{
    rut_toggle_t *toggle = object;

    rut_closure_list_disconnect_all_FIXME(&toggle->on_toggle_cb_list);

    if (toggle->selected_icon) {
        cg_object_unref(toggle->selected_icon);
        cg_object_unref(toggle->pipeline_selected_icon);
    }
    if (toggle->unselected_icon) {
        cg_object_unref(toggle->unselected_icon);
        cg_object_unref(toggle->pipeline_unselected_icon);
    }
    if (toggle->tick)
        g_object_unref(toggle->tick);
    g_object_unref(toggle->label);

    cg_object_unref(toggle->pipeline_border);
    cg_object_unref(toggle->pipeline_box);

    rut_introspectable_destroy(toggle);
    rut_graphable_destroy(toggle);

    rut_object_free(rut_toggle_t, object);
}

static void
_rut_toggle_paint(rut_object_t *object,
                  rut_paint_context_t *paint_ctx)
{
    rut_toggle_t *toggle = object;
    rut_object_t *camera = paint_ctx->camera;
    cg_framebuffer_t *fb = rut_camera_get_framebuffer(camera);
    int icon_width;

    if (toggle->selected_icon) {
        cg_texture_t *icon;
        cg_pipeline_t *pipeline;
        int icon_y;
        int icon_height;

        if (toggle->state || toggle->tentative_set) {
            icon = toggle->selected_icon;
            pipeline = toggle->pipeline_selected_icon;
        } else {
            icon = toggle->unselected_icon;
            pipeline = toggle->pipeline_unselected_icon;
        }

        icon_y =
            (toggle->label_height / 2.0) - (cg_texture_get_height(icon) / 2.0);
        icon_width = cg_texture_get_width(icon);
        icon_height = cg_texture_get_height(icon);

        cg_framebuffer_draw_rectangle(
            fb, pipeline, 0, icon_y, icon_width, icon_y + icon_height);
    } else {
        /* FIXME: This is a fairly lame way of drawing a check box! */

        int box_y = (toggle->label_height / 2.0) - (RUT_TOGGLE_BOX_WIDTH / 2.0);

        cg_framebuffer_draw_rectangle(fb,
                                      toggle->pipeline_border,
                                      0,
                                      box_y,
                                      RUT_TOGGLE_BOX_WIDTH,
                                      box_y + RUT_TOGGLE_BOX_WIDTH);

        cg_framebuffer_draw_rectangle(fb,
                                      toggle->pipeline_box,
                                      1,
                                      box_y + 1,
                                      RUT_TOGGLE_BOX_WIDTH - 2,
                                      box_y + RUT_TOGGLE_BOX_WIDTH - 2);

        if (toggle->state || toggle->tentative_set)
            cg_pango_show_layout(rut_camera_get_framebuffer(camera),
                                 toggle->tick,
                                 0,
                                 0,
                                 &toggle->tick_color);

        icon_width = RUT_TOGGLE_BOX_WIDTH;
    }

    cg_pango_show_layout(rut_camera_get_framebuffer(camera),
                         toggle->label,
                         icon_width + RUT_TOGGLE_BOX_RIGHT_PAD,
                         0,
                         &toggle->text_color);
}

static void
_rut_toggle_set_size(rut_object_t *object, float width, float height)
{
    /* FIXME: we could elipsize the label if smaller than our preferred size */
}

static void
_rut_toggle_get_size(rut_object_t *object, float *width, float *height)
{
    rut_toggle_t *toggle = object;

    *width = toggle->width;
    *height = toggle->height;
}

static void
_rut_toggle_get_preferred_width(rut_object_t *object,
                                float for_height,
                                float *min_width_p,
                                float *natural_width_p)
{
    rut_toggle_t *toggle = object;
    PangoRectangle logical_rect;
    float width;
    float right_pad;

    pango_layout_get_pixel_extents(toggle->label, NULL, &logical_rect);

    /* Don't bother padding the right of the toggle button if the label
     * is empty */
    if (logical_rect.width > 0)
        right_pad = RUT_TOGGLE_BOX_RIGHT_PAD;
    else
        right_pad = 0.0f;

    if (toggle->selected_icon) {
        width = logical_rect.width +
                cg_texture_get_width(toggle->selected_icon) + right_pad;
    } else
        width = logical_rect.width + RUT_TOGGLE_BOX_WIDTH + right_pad;

    if (min_width_p)
        *min_width_p = width;
    if (natural_width_p)
        *natural_width_p = width;
}

static void
_rut_toggle_get_preferred_height(rut_object_t *object,
                                 float for_width,
                                 float *min_height_p,
                                 float *natural_height_p)
{
    rut_toggle_t *toggle = object;
    PangoRectangle logical_rect;
    float height;

    pango_layout_get_pixel_extents(toggle->label, NULL, &logical_rect);

    if (toggle->selected_icon)
        height = MAX(logical_rect.height,
                     cg_texture_get_height(toggle->selected_icon));
    else
        height = MAX(logical_rect.height, RUT_TOGGLE_BOX_WIDTH);

    if (min_height_p)
        *min_height_p = height;
    if (natural_height_p)
        *natural_height_p = height;
}

rut_type_t rut_toggle_type;

static void
_rut_toggle_init_type(void)
{
    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child remove */
                                                       NULL, /* child add */
                                                       NULL /* parent changed */
    };
    static rut_paintable_vtable_t paintable_vtable = { _rut_toggle_paint };
    static rut_sizable_vtable_t sizable_vtable = {
        _rut_toggle_set_size,            _rut_toggle_get_size,
        _rut_toggle_get_preferred_width, _rut_toggle_get_preferred_height,
        NULL /* add_preferred_size_callback (the preferred size never changes)
              */
    };

    rut_type_t *type = &rut_toggle_type;
#define TYPE rut_toggle_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rut_toggle_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_GRAPHABLE,
                       offsetof(TYPE, graphable),
                       &graphable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_PAINTABLE,
                       offsetof(TYPE, paintable),
                       &paintable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INTROSPECTABLE,
                       offsetof(TYPE, introspectable),
                       NULL); /* no implied vtable */
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_SIZABLE,
                       0, /* no implied properties */
                       &sizable_vtable);

#undef TYPE
}

typedef struct _toggle_grab_state_t {
    rut_object_t *camera;
    rut_input_region_t *region;
    rut_toggle_t *toggle;
} toggle_grab_state_t;

static rut_input_event_status_t
_rut_toggle_grab_input_cb(rut_input_event_t *event, void *user_data)
{
    toggle_grab_state_t *state = user_data;
    rut_toggle_t *toggle = state->toggle;

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION) {
        rut_shell_t *shell = toggle->shell;
        if (rut_motion_event_get_action(event) == RUT_MOTION_EVENT_ACTION_UP) {
            float x = rut_motion_event_get_x(event);
            float y = rut_motion_event_get_y(event);

            rut_shell_ungrab_input(shell, _rut_toggle_grab_input_cb, user_data);

            if (rut_pickable_pick(state->region,
                                  state->camera,
                                  NULL, /* pre-computed modelview */
                                  x,
                                  y)) {
                rut_toggle_set_state(toggle, !toggle->state);

                rut_closure_list_invoke(&toggle->on_toggle_cb_list,
                                        rut_toggle_callback_t,
                                        toggle,
                                        toggle->state);

                c_debug("Toggle click\n");

                c_slice_free(toggle_grab_state_t, state);

                rut_shell_queue_redraw(toggle->shell);

                toggle->tentative_set = false;
            }

            return RUT_INPUT_EVENT_STATUS_HANDLED;
        } else if (rut_motion_event_get_action(event) ==
                   RUT_MOTION_EVENT_ACTION_MOVE) {
            float x = rut_motion_event_get_x(event);
            float y = rut_motion_event_get_y(event);

            if (rut_pickable_pick(state->region,
                                  state->camera,
                                  NULL, /* pre-computed modelview */
                                  x,
                                  y))
                toggle->tentative_set = true;
            else
                toggle->tentative_set = false;

            rut_shell_queue_redraw(toggle->shell);

            return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static rut_input_event_status_t
_rut_toggle_input_cb(rut_input_region_t *region,
                     rut_input_event_t *event,
                     void *user_data)
{
    rut_toggle_t *toggle = user_data;

    c_debug("Toggle input\n");

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION &&
        rut_motion_event_get_action(event) == RUT_MOTION_EVENT_ACTION_DOWN) {
        rut_shell_t *shell = toggle->shell;
        toggle_grab_state_t *state = c_slice_new(toggle_grab_state_t);

        state->toggle = toggle;
        state->camera = rut_input_event_get_camera(event);
        state->region = region;

        rut_shell_grab_input(
            shell, state->camera, _rut_toggle_grab_input_cb, state);

        toggle->tentative_set = true;

        rut_shell_queue_redraw(toggle->shell);

        return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
_rut_toggle_update_colours(rut_toggle_t *toggle)
{
    uint32_t colors[2][2][3] = {
        /* Disabled */
        {
            /* Unset */
            { 0x000000ff, /* border */
              0xffffffff, /* box */
              0x000000ff /* text*/
            },
            /* Set */
            { 0x000000ff, /* border */
              0xffffffff, /* box */
              0x000000ff /* text*/
            },
        },
        /* Enabled */
        {
            /* Unset */
            { 0x000000ff, /* border */
              0xffffffff, /* box */
              0x000000ff /* text*/
            },
            /* Set */
            { 0x000000ff, /* border */
              0xffffffff, /* box */
              0x000000ff /* text*/
            },
        }
    };

    uint32_t border, box, text;

    border = colors[toggle->enabled][toggle->state][0];
    box = colors[toggle->enabled][toggle->state][1];
    text = colors[toggle->enabled][toggle->state][2];

    cg_pipeline_set_color4f(toggle->pipeline_border,
                            RUT_UINT32_RED_AS_FLOAT(border),
                            RUT_UINT32_GREEN_AS_FLOAT(border),
                            RUT_UINT32_BLUE_AS_FLOAT(border),
                            RUT_UINT32_ALPHA_AS_FLOAT(border));
    cg_pipeline_set_color4f(toggle->pipeline_box,
                            RUT_UINT32_RED_AS_FLOAT(box),
                            RUT_UINT32_GREEN_AS_FLOAT(box),
                            RUT_UINT32_BLUE_AS_FLOAT(box),
                            RUT_UINT32_ALPHA_AS_FLOAT(box));
    cg_color_init_from_4f(&toggle->text_color,
                          RUT_UINT32_RED_AS_FLOAT(text),
                          RUT_UINT32_GREEN_AS_FLOAT(text),
                          RUT_UINT32_BLUE_AS_FLOAT(text),
                          RUT_UINT32_ALPHA_AS_FLOAT(text));
    cg_color_init_from_4f(&toggle->tick_color,
                          RUT_UINT32_RED_AS_FLOAT(text),
                          RUT_UINT32_GREEN_AS_FLOAT(text),
                          RUT_UINT32_BLUE_AS_FLOAT(text),
                          RUT_UINT32_ALPHA_AS_FLOAT(text));
}

rut_toggle_t *
rut_toggle_new_with_icons(rut_shell_t *shell,
                          const char *unselected_icon,
                          const char *selected_icon,
                          const char *label)
{
    rut_toggle_t *toggle = rut_object_alloc0(
        rut_toggle_t, &rut_toggle_type, _rut_toggle_init_type);
    PangoRectangle label_size;
    char *font_name;
    PangoFontDescription *font_desc;

    c_list_init(&toggle->on_toggle_cb_list);

    rut_graphable_init(toggle);
    rut_paintable_init(toggle);

    rut_introspectable_init(toggle, _rut_toggle_prop_specs, toggle->properties);

    toggle->shell = shell;

    toggle->state = true;
    toggle->enabled = true;

    if (selected_icon) {
        toggle->selected_icon = rut_load_texture(shell, selected_icon, NULL);
        if (toggle->selected_icon && unselected_icon)
            toggle->unselected_icon =
                rut_load_texture(shell, unselected_icon, NULL);
        if (toggle->unselected_icon) {
            toggle->pipeline_selected_icon = cg_pipeline_new(shell->cg_device);
            cg_pipeline_set_layer_texture(
                toggle->pipeline_selected_icon, 0, toggle->selected_icon);
            toggle->pipeline_unselected_icon =
                cg_pipeline_copy(toggle->pipeline_selected_icon);
            cg_pipeline_set_layer_texture(
                toggle->pipeline_unselected_icon, 0, toggle->unselected_icon);
        } else {
            c_warning("Failed to load toggle icons %s and %s",
                      selected_icon,
                      unselected_icon);
            if (toggle->selected_icon) {
                cg_object_unref(toggle->selected_icon);
                toggle->selected_icon = NULL;
            }
        }
    }

    if (!toggle->selected_icon) {
        toggle->tick = pango_layout_new(shell->pango_context);
        pango_layout_set_font_description(toggle->tick,
                                          shell->pango_font_desc);
        pango_layout_set_text(toggle->tick, "✔", -1);
    }

    font_name =
        rut_settings_get_font_name(shell->settings); /* font_name is allocated */
    font_desc = pango_font_description_from_string(font_name);
    c_free(font_name);

    toggle->label = pango_layout_new(shell->pango_context);
    pango_layout_set_font_description(toggle->label, font_desc);
    pango_layout_set_text(toggle->label, label, -1);

    pango_font_description_free(font_desc);

    pango_layout_get_extents(toggle->label, NULL, &label_size);
    toggle->label_width = PANGO_PIXELS(label_size.width);
    toggle->label_height = PANGO_PIXELS(label_size.height);

    toggle->width =
        toggle->label_width + RUT_TOGGLE_BOX_RIGHT_PAD + RUT_TOGGLE_BOX_WIDTH;
    toggle->height = toggle->label_height + RUT_TOGGLE_LABEL_VPAD;

    toggle->pipeline_border = cg_pipeline_new(shell->cg_device);
    toggle->pipeline_box = cg_pipeline_new(shell->cg_device);

    _rut_toggle_update_colours(toggle);

    toggle->input_region = rut_input_region_new_rectangle(0,
                                                          0,
                                                          RUT_TOGGLE_BOX_WIDTH,
                                                          RUT_TOGGLE_BOX_WIDTH,
                                                          _rut_toggle_input_cb,
                                                          toggle);

    // rut_input_region_set_graphable (toggle->input_region, toggle);
    rut_graphable_add_child(toggle, toggle->input_region);

    return toggle;
}

rut_toggle_t *
rut_toggle_new(rut_shell_t *shell, const char *label)
{
    return rut_toggle_new_with_icons(shell, NULL, NULL, label);
}

rut_closure_t *
rut_toggle_add_on_toggle_callback(rut_toggle_t *toggle,
                                  rut_toggle_callback_t callback,
                                  void *user_data,
                                  rut_closure_destroy_callback_t destroy_cb)
{
    c_return_val_if_fail(callback != NULL, NULL);

    return rut_closure_list_add_FIXME(
        &toggle->on_toggle_cb_list, callback, user_data, destroy_cb);
}

void
rut_toggle_set_enabled(rut_object_t *obj, bool enabled)
{
    rut_toggle_t *toggle = obj;

    if (toggle->enabled == enabled)
        return;

    toggle->enabled = enabled;
    rig_property_dirty(&toggle->shell->property_ctx,
                       &toggle->properties[RUT_TOGGLE_PROP_ENABLED]);
    rut_shell_queue_redraw(toggle->shell);
}

void
rut_toggle_set_state(rut_object_t *obj, bool state)
{
    rut_toggle_t *toggle = obj;

    if (toggle->state == state)
        return;

    toggle->state = state;
    rig_property_dirty(&toggle->shell->property_ctx,
                       &toggle->properties[RUT_TOGGLE_PROP_STATE]);
    rut_shell_queue_redraw(toggle->shell);
}

rig_property_t *
rut_toggle_get_enabled_property(rut_toggle_t *toggle)
{
    return &toggle->properties[RUT_TOGGLE_PROP_STATE];
}

void
rut_toggle_set_tick(rut_object_t *obj, const char *tick)
{
    rut_toggle_t *toggle = obj;

    pango_layout_set_text(toggle->tick, tick, -1);
    rut_shell_queue_redraw(toggle->shell);
}

const char *
rut_toggle_get_tick(rut_object_t *obj)
{
    rut_toggle_t *toggle = obj;

    return pango_layout_get_text(toggle->tick);
}

void
rut_toggle_set_tick_color(rut_object_t *obj, const cg_color_t *color)
{
    rut_toggle_t *toggle = obj;

    toggle->tick_color = *color;
    rut_shell_queue_redraw(toggle->shell);
}

const cg_color_t *
rut_toggle_get_tick_color(rut_object_t *obj)
{
    rut_toggle_t *toggle = obj;

    return &toggle->tick_color;
}
