/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012  Intel Corporation.
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

#include "rut-slider.h"
#include "rut-object.h"
#include "rig-introspectable.h"
#include "rut-input-region.h"
#include "rut-transform.h"
#include "rut-nine-slice.h"
#include "rut-paintable.h"
#include "rut-texture-cache.h"

enum {
    RUT_SLIDER_PROP_PROGRESS,
    RUT_SLIDER_N_PROPS
};

struct _rut_slider_t {
    rut_object_base_t _base;

    /* FIXME: It doesn't seem right that we should have to save a
     * pointer to the context for input here... */
    rut_shell_t *shell;

    /* FIXME: It also doesn't seem right to have to save a pointer
     * to the camera here so we can queue a redraw */
    // rut_object_t *camera;

    rut_graphable_props_t graphable;
    rut_paintable_props_t paintable;

    rut_nine_slice_t *background;
    rut_nine_slice_t *handle;
    rut_transform_t *handle_transform;

    rut_input_region_t *input_region;
    float grab_x;
    float grab_y;
    float grab_progress;

    rut_axis_t axis;
    float range_min;
    float range_max;
    float length;
    float progress;

    rig_introspectable_props_t introspectable;
    rig_property_t properties[RUT_SLIDER_N_PROPS];
};

static rig_property_spec_t _rut_slider_prop_specs[] = {
    { .name = "progress",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof(rut_slider_t, progress),
      .setter.float_type = rut_slider_set_progress },
    { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rut_slider_free(void *object)
{
    rut_slider_t *slider = object;

    rut_object_unref(slider->input_region);

    rut_graphable_remove_child(slider->handle_transform);

    rut_object_unref(slider->handle_transform);
    rut_object_unref(slider->handle);
    rut_object_unref(slider->background);

    rig_introspectable_destroy(slider);

    rut_graphable_destroy(slider);

    rut_object_free(rut_slider_t, object);
}

static void
_rut_slider_paint(rut_object_t *object,
                  rut_paint_context_t *paint_ctx)
{
    rut_slider_t *slider = object;
    rut_paintable_vtable_t *bg_paintable =
        rut_object_get_vtable(slider->background, RUT_TRAIT_ID_PAINTABLE);

    bg_paintable->paint(slider->background, paint_ctx);
}

#if 0
static void
_rut_slider_set_camera (rut_object_t *object,
                        rut_object_t *camera)
{
    rut_slider_t *slider = object;

    if (camera)
        rut_camera_add_input_region (camera, slider->input_region);

    slider->camera = camera;
}
#endif

rut_type_t rut_slider_type;

static void
_rut_slider_init_type(void)
{
    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child remove */
                                                       NULL, /* child add */
                                                       NULL /* parent changed */
    };

    static rut_paintable_vtable_t paintable_vtable = { _rut_slider_paint };

    rut_type_t *type = &rut_slider_type;
#define TYPE rut_slider_t

    rut_type_init(&rut_slider_type, C_STRINGIFY(TYPE), _rut_slider_free);
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

#undef TYPE
}

static rut_input_event_status_t
_rut_slider_grab_input_cb(rut_input_event_t *event, void *user_data)
{
    rut_slider_t *slider = user_data;

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION) {
        rut_shell_t *shell = slider->shell;
        if (rut_motion_event_get_action(event) == RUT_MOTION_EVENT_ACTION_UP) {
            rut_shell_ungrab_input(shell, _rut_slider_grab_input_cb, user_data);
            return RUT_INPUT_EVENT_STATUS_HANDLED;
        } else if (rut_motion_event_get_action(event) ==
                   RUT_MOTION_EVENT_ACTION_MOVE) {
            float diff;
            float progress;

            if (slider->axis == RUT_AXIS_X)
                diff = rut_motion_event_get_x(event) - slider->grab_x;
            else
                diff = rut_motion_event_get_y(event) - slider->grab_y;

            progress =
                CLAMP(slider->grab_progress + diff / slider->length, 0, 1);

            rut_slider_set_progress(slider, progress);

            return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static rut_input_event_status_t
_rut_slider_input_cb(rut_input_region_t *region,
                     rut_input_event_t *event,
                     void *user_data)
{
    rut_slider_t *slider = user_data;

    // c_debug ("Slider input\n");

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION &&
        rut_motion_event_get_action(event) == RUT_MOTION_EVENT_ACTION_DOWN) {
        rut_shell_t *shell = slider->shell;
        rut_shell_grab_input(shell,
                             rut_input_event_get_camera(event),
                             _rut_slider_grab_input_cb,
                             slider);
        slider->grab_x = rut_motion_event_get_x(event);
        slider->grab_y = rut_motion_event_get_y(event);
        slider->grab_progress = slider->progress;
        return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

rut_slider_t *
rut_slider_new(rut_shell_t *shell, rut_axis_t axis, float min, float max,
               float length)
{
    rut_slider_t *slider = rut_object_alloc0(
        rut_slider_t, &rut_slider_type, _rut_slider_init_type);
    cg_texture_t *bg_texture;
    cg_texture_t *handle_texture;
    c_error_t *error = NULL;
    // PangoRectangle label_size;
    float width;
    float height;

    rut_graphable_init(slider);
    rut_paintable_init(slider);

    slider->shell = shell;

    slider->axis = axis;
    slider->range_min = min;
    slider->range_max = max;
    slider->length = length;
    slider->progress = 0;

    bg_texture =
        rut_load_texture_from_data_file(shell, "slider-background.png",
                                        &error);
    if (!bg_texture) {
        c_warning("Failed to load slider-background.png: %s", error->message);
        c_error_free(error);
    }

    handle_texture =
        rut_load_texture_from_data_file(shell, "slider-handle.png", &error);
    if (!handle_texture) {
        c_warning("Failed to load slider-handle.png: %s", error->message);
        c_error_free(error);
    }

    if (axis == RUT_AXIS_X) {
        width = length;
        height = 20;
    } else {
        width = 20;
        height = length;
    }

    slider->background =
        rut_nine_slice_new(shell, bg_texture, 2, 3, 3, 3, width, height);

    if (axis == RUT_AXIS_X)
        width = 20;
    else
        height = 20;

    slider->handle_transform = rut_transform_new(shell);
    slider->handle =
        rut_nine_slice_new(shell, handle_texture, 4, 5, 6, 5, width,
                           height);
    rut_graphable_add_child(slider->handle_transform, slider->handle);
    rut_graphable_add_child(slider, slider->handle_transform);

    slider->input_region = rut_input_region_new_rectangle(
        0, 0, width, height, _rut_slider_input_cb, slider);

    // rut_input_region_set_graphable (slider->input_region, slider->handle);
    rut_graphable_add_child(slider, slider->input_region);

    rig_introspectable_init(slider, _rut_slider_prop_specs, slider->properties);

    return slider;
}

void
rut_slider_set_range(rut_slider_t *slider, float min, float max)
{
    slider->range_min = min;
    slider->range_max = max;
}

void
rut_slider_set_length(rut_slider_t *slider, float length)
{
    slider->length = length;
}

void
rut_slider_set_progress(rut_object_t *obj, float progress)
{
    rut_slider_t *slider = obj;
    float translation;

    if (slider->progress == progress)
        return;

    slider->progress = progress;
    rig_property_dirty(&slider->shell->property_ctx,
                       &slider->properties[RUT_SLIDER_PROP_PROGRESS]);

    translation = (slider->length - 20) * slider->progress;

    rut_transform_init_identity(slider->handle_transform);

    if (slider->axis == RUT_AXIS_X)
        rut_transform_translate(slider->handle_transform, translation, 0, 0);
    else
        rut_transform_translate(slider->handle_transform, 0, translation, 0);

    rut_shell_queue_redraw(slider->shell);

    // c_debug ("progress = %f\n", slider->progress);
}
