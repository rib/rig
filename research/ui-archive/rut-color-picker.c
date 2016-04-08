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
#include <math.h>

#include "rut-shell.h"
#include "rut-interfaces.h"
#include "rut-paintable.h"
#include "rig-property.h"
#include "rut-input-region.h"
#include "rut-color-picker.h"
#include "rut-introspectable.h"
#include "rut-camera.h"
#include "rut-texture-cache.h"

enum {
    RUT_COLOR_PICKER_PROP_COLOR,
    RUT_COLOR_PICKER_N_PROPS
};

typedef enum {
    RUT_COLOR_PICKER_GRAB_NONE,
    RUT_COLOR_PICKER_GRAB_HS,
    RUT_COLOR_PICKER_GRAB_V
} rut_color_picker_grab_t;

struct _rut_color_picker_t {
    rut_object_base_t _base;

    rut_shell_t *shell;

    rut_graphable_props_t graphable;
    rut_paintable_props_t paintable;

    rut_introspectable_props_t introspectable;
    rig_property_t properties[RUT_COLOR_PICKER_N_PROPS];

    bool hs_pipeline_dirty;
    cg_pipeline_t *hs_pipeline;

    bool v_pipeline_dirty;
    cg_pipeline_t *v_pipeline;

    int width, height;

    int dot_width, dot_height;
    cg_pipeline_t *dot_pipeline;

    cg_pipeline_t *bg_pipeline;

    rut_color_picker_grab_t grab;
    rut_input_region_t *input_region;

    cg_color_t color;
    /* The current component values of the HSV colour */
    float hue, saturation, value;
};

rut_type_t rut_color_picker_type;

#define RUT_COLOR_PICKER_HS_SIZE 128
#define RUT_COLOR_PICKER_V_WIDTH 16
#define RUT_COLOR_PICKER_V_HEIGHT 128
#define RUT_COLOR_PICKER_PADDING 8

#define RUT_COLOR_PICKER_HS_X RUT_COLOR_PICKER_PADDING
#define RUT_COLOR_PICKER_HS_Y RUT_COLOR_PICKER_PADDING
#define RUT_COLOR_PICKER_HS_CENTER_X                                           \
    (RUT_COLOR_PICKER_HS_X + RUT_COLOR_PICKER_HS_SIZE / 2.0f)
#define RUT_COLOR_PICKER_HS_CENTER_Y                                           \
    (RUT_COLOR_PICKER_HS_Y + RUT_COLOR_PICKER_HS_SIZE / 2.0f)

#define RUT_COLOR_PICKER_V_X                                                   \
    (RUT_COLOR_PICKER_HS_SIZE + RUT_COLOR_PICKER_PADDING * 2.0f)
#define RUT_COLOR_PICKER_V_Y RUT_COLOR_PICKER_PADDING

#define RUT_COLOR_PICKER_TOTAL_WIDTH                                           \
    (RUT_COLOR_PICKER_HS_SIZE + RUT_COLOR_PICKER_V_WIDTH +                     \
     RUT_COLOR_PICKER_PADDING * 3.0f)

#define RUT_COLOR_PICKER_TOTAL_HEIGHT                                          \
    (MAX(RUT_COLOR_PICKER_HS_SIZE, RUT_COLOR_PICKER_V_HEIGHT) +                \
     RUT_COLOR_PICKER_PADDING * 2.0f)

/* The portion of the edge of the HS circle to blend so that it is
 * nicely anti-aliased */
#define RUT_COLOR_PICKER_HS_BLEND_EDGE 0.98f

static rig_property_spec_t _rut_color_picker_prop_specs[] = {
    { .name = "color",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_COLOR,
      .data_offset = offsetof(rut_color_picker_t, color),
      .setter.color_type = rut_color_picker_set_color },
    { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void ungrab(rut_color_picker_t *picker);

static void
_rut_color_picker_free(void *object)
{
    rut_color_picker_t *picker = object;

    ungrab(picker);

    cg_object_unref(picker->hs_pipeline);
    cg_object_unref(picker->v_pipeline);
    cg_object_unref(picker->dot_pipeline);
    cg_object_unref(picker->bg_pipeline);

    rut_graphable_remove_child(picker->input_region);
    rut_object_unref(picker->input_region);

    rut_object_unref(picker->shell);

    rut_introspectable_destroy(picker);
    rut_graphable_destroy(picker);

    rut_object_free(rut_color_picker_t, picker);
}

static void
hsv_to_rgb(const float hsv[3], float rgb[3])
{
    /* Based on Wikipedia:
       http://en.wikipedia.org/wiki/HSL_and_HSV#From_HSV */
    float h = hsv[0];
    float s = hsv[1];
    float v = hsv[2];
    float hh = h * 6.0f / (2.0f * G_PI);
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(hh, 2.0f) - 1.0f));
    float r, g, b;
    float m;

    if (hh < 1.0f) {
        r = c;
        g = x;
        b = 0;
    } else if (hh < 2.0f) {
        r = x;
        g = c;
        b = 0;
    } else if (hh < 3.0f) {
        r = 0;
        g = c;
        b = x;
    } else if (hh < 4.0f) {
        r = 0;
        g = x;
        b = c;
    } else if (hh < 5.0f) {
        r = x;
        g = 0;
        b = c;
    } else {
        r = c;
        g = 0;
        b = x;
    }

    m = v - c;
    rgb[0] = r + m;
    rgb[1] = g + m;
    rgb[2] = b + m;
}

static void
rgb_to_hsv(const float rgb[3], float hsv[3])
{
    /* Based on this:
       http://en.literateprograms.org/RGB_to_HSV_color_space_conversion_%28C%29
     */
    float r = rgb[0];
    float g = rgb[1];
    float b = rgb[2];
    float rgb_max, rgb_min;
    float h, s, v;

    v = MAX(g, b);
    v = MAX(r, v);

    if (v <= 0.0f) {
        memset(hsv, 0, sizeof(float) * 3);
        return;
    }

    r /= v;
    g /= v;
    b /= v;

    rgb_min = MIN(g, b);
    rgb_min = MIN(r, rgb_min);

    s = 1.0f - rgb_min;

    if (s <= 0.0f)
        h = 0.0f;
    else {
        /* Normalize saturation to 1 */
        r = (r - rgb_min) / s;
        g = (g - rgb_min) / s;
        b = (b - rgb_min) / s;

        rgb_max = MAX(g, b);
        rgb_max = MAX(r, rgb_max);
        rgb_min = MIN(g, b);
        rgb_min = MIN(r, rgb_min);

        if (rgb_max == r) {
            h = G_PI / 3.0f * (g - b);
            if (h < 0.0f)
                h += G_PI * 2.0f;
        } else if (rgb_max == g)
            h = G_PI / 3.0f * (2.0f + b - r);
        else /* rgb_max == b */
            h = G_PI / 3.0f * (4.0f + r - g);
    }

    hsv[0] = h;
    hsv[1] = s;
    hsv[2] = v;
}

static void
ensure_hs_pipeline(rut_color_picker_t *picker)
{
    cg_bitmap_t *bitmap;
    cg_pixel_buffer_t *buffer;
    cg_pipeline_t *pipeline;
    cg_texture_2d_t *texture;
    int rowstride;
    uint8_t *data, *p;
    int x, y;

    if (!picker->hs_pipeline_dirty)
        return;

    bitmap = cg_bitmap_new_with_size(picker->shell->cg_device,
                                     RUT_COLOR_PICKER_HS_SIZE,
                                     RUT_COLOR_PICKER_HS_SIZE,
                                     CG_PIXEL_FORMAT_RGBA_8888_PRE);
    rowstride = cg_bitmap_get_rowstride(bitmap);
    buffer = cg_bitmap_get_buffer(bitmap);

    data = cg_buffer_map(
        buffer, CG_BUFFER_ACCESS_WRITE, CG_BUFFER_MAP_HINT_DISCARD, NULL);

    p = data;

    for (y = 0; y < RUT_COLOR_PICKER_HS_SIZE; y++) {
        for (x = 0; x < RUT_COLOR_PICKER_HS_SIZE; x++) {
            float dx = x * 2.0f / RUT_COLOR_PICKER_HS_SIZE - 1.0f;
            float dy = y * 2.0f / RUT_COLOR_PICKER_HS_SIZE - 1.0f;
            float hsv[3];

            hsv[1] = sqrtf(dx * dx + dy * dy);

            if (hsv[1] >= 1.0f) {
                /* Outside of the circle the texture will be fully
                 * transparent */
                p[0] = 0;
                p[1] = 0;
                p[2] = 0;
                p[3] = 0;
            } else {
                float rgb[3];
                float alpha;

                hsv[2] = picker->value;
                hsv[0] = atan2f(dy, dx) + G_PI;

                hsv_to_rgb(hsv, rgb);

                /* Blend the edges of the circle a bit so that it
                 * looks anti-aliased */
                if (hsv[1] >= RUT_COLOR_PICKER_HS_BLEND_EDGE)
                    alpha = (((RUT_COLOR_PICKER_HS_BLEND_EDGE - hsv[1]) /
                              (1.0f - RUT_COLOR_PICKER_HS_BLEND_EDGE) +
                              1.0f) *
                             255.0f);
                else
                    alpha = 255.0f;

                p[0] = nearbyintf(rgb[0] * alpha);
                p[1] = nearbyintf(rgb[1] * alpha);
                p[2] = nearbyintf(rgb[2] * alpha);
                p[3] = alpha;
            }

            p += 4;
        }

        p += rowstride - RUT_COLOR_PICKER_HS_SIZE * 4;
    }

    cg_buffer_unmap(buffer);

    texture = cg_texture_2d_new_from_bitmap(bitmap);

    pipeline = cg_pipeline_copy(picker->hs_pipeline);
    cg_pipeline_set_layer_texture(pipeline,
                                  0, /* layer */
                                  texture);
    cg_object_unref(picker->hs_pipeline);
    picker->hs_pipeline = pipeline;

    cg_object_unref(texture);
    cg_object_unref(bitmap);

    picker->hs_pipeline_dirty = false;
}

static void
ensure_v_pipeline(rut_color_picker_t *picker)
{
    cg_bitmap_t *bitmap;
    cg_pixel_buffer_t *buffer;
    cg_pipeline_t *pipeline;
    cg_texture_2d_t *texture;
    int rowstride;
    uint8_t *data, *p;
    float hsv[3];
    int y;

    if (!picker->v_pipeline_dirty)
        return;

    bitmap = cg_bitmap_new_with_size(picker->shell->cg_device,
                                     1, /* width */
                                     RUT_COLOR_PICKER_V_HEIGHT,
                                     CG_PIXEL_FORMAT_RGB_888);
    rowstride = cg_bitmap_get_rowstride(bitmap);
    buffer = cg_bitmap_get_buffer(bitmap);

    data = cg_buffer_map(
        buffer, CG_BUFFER_ACCESS_WRITE, CG_BUFFER_MAP_HINT_DISCARD, NULL);

    p = data;

    hsv[0] = picker->hue;
    hsv[1] = picker->saturation;

    for (y = 0; y < RUT_COLOR_PICKER_HS_SIZE; y++) {
        float rgb[3];

        hsv[2] = 1.0f - y / (RUT_COLOR_PICKER_HS_SIZE - 1.0f);

        hsv_to_rgb(hsv, rgb);

        p[0] = nearbyintf(rgb[0] * 255.0f);
        p[1] = nearbyintf(rgb[1] * 255.0f);
        p[2] = nearbyintf(rgb[2] * 255.0f);

        p += rowstride;
    }

    cg_buffer_unmap(buffer);

    texture = cg_texture_2d_new_from_bitmap(bitmap);

    pipeline = cg_pipeline_copy(picker->v_pipeline);
    cg_pipeline_set_layer_texture(pipeline,
                                  0, /* layer */
                                  texture);
    cg_object_unref(picker->v_pipeline);
    picker->v_pipeline = pipeline;

    cg_object_unref(texture);
    cg_object_unref(bitmap);

    picker->v_pipeline_dirty = false;
}

static void
draw_dot(rut_color_picker_t *picker, cg_framebuffer_t *fb, float x, float y)
{
    cg_framebuffer_draw_rectangle(fb,
                                  picker->dot_pipeline,
                                  x - picker->dot_width / 2.0f,
                                  y - picker->dot_height / 2.0f,
                                  x + picker->dot_width / 2.0f,
                                  y + picker->dot_height / 2.0f);
}

static void
_rut_color_picker_paint(rut_object_t *object,
                        rut_paint_context_t *paint_ctx)
{
    rut_color_picker_t *picker = (rut_color_picker_t *)object;
    rut_object_t *camera = paint_ctx->camera;
    cg_framebuffer_t *fb = rut_camera_get_framebuffer(camera);

    cg_framebuffer_draw_rectangle(
        fb, picker->bg_pipeline, 0.0f, 0.0f, picker->width, picker->height);

    ensure_hs_pipeline(picker);
    ensure_v_pipeline(picker);

    cg_framebuffer_draw_rectangle(
        fb,
        picker->hs_pipeline,
        RUT_COLOR_PICKER_HS_X,
        RUT_COLOR_PICKER_HS_Y,
        RUT_COLOR_PICKER_HS_X + RUT_COLOR_PICKER_HS_SIZE,
        RUT_COLOR_PICKER_HS_Y + RUT_COLOR_PICKER_HS_SIZE);
    cg_framebuffer_draw_rectangle(
        fb,
        picker->v_pipeline,
        RUT_COLOR_PICKER_V_X,
        RUT_COLOR_PICKER_V_Y,
        RUT_COLOR_PICKER_V_X + RUT_COLOR_PICKER_V_WIDTH,
        RUT_COLOR_PICKER_V_Y + RUT_COLOR_PICKER_V_HEIGHT);

    draw_dot(picker,
             fb,
             RUT_COLOR_PICKER_HS_CENTER_X - cos(picker->hue) *
             RUT_COLOR_PICKER_HS_SIZE /
             2.0f * picker->saturation,
             RUT_COLOR_PICKER_HS_CENTER_Y - sin(picker->hue) *
             RUT_COLOR_PICKER_HS_SIZE /
             2.0f * picker->saturation);
    draw_dot(picker,
             fb,
             RUT_COLOR_PICKER_V_X + RUT_COLOR_PICKER_V_WIDTH / 2.0f,
             RUT_COLOR_PICKER_V_Y +
             RUT_COLOR_PICKER_V_HEIGHT * (1.0f - picker->value));
}

static void
rut_color_picker_set_size(rut_object_t *object, float width, float height)
{
    rut_color_picker_t *picker = object;

    rut_shell_queue_redraw(picker->shell);

    picker->width = width;
    picker->height = height;
}

static void
rut_color_picker_get_size(rut_object_t *object, float *width, float *height)
{
    rut_color_picker_t *picker = object;

    *width = picker->width;
    *height = picker->height;
}

static void
rut_color_picker_get_preferred_width(rut_object_t *object,
                                     float for_height,
                                     float *min_width_p,
                                     float *natural_width_p)
{
    if (min_width_p)
        *min_width_p = RUT_COLOR_PICKER_TOTAL_WIDTH;
    if (natural_width_p)
        *natural_width_p = RUT_COLOR_PICKER_TOTAL_WIDTH;
}

static void
rut_color_picker_get_preferred_height(rut_object_t *object,
                                      float for_width,
                                      float *min_height_p,
                                      float *natural_height_p)
{
    if (min_height_p)
        *min_height_p = RUT_COLOR_PICKER_TOTAL_HEIGHT;
    if (natural_height_p)
        *natural_height_p = RUT_COLOR_PICKER_TOTAL_HEIGHT;
}

static void
_rut_color_picker_init_type(void)
{
    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child removed */
                                                       NULL, /* child addded */
                                                       NULL /* parent changed */
    };

    static rut_paintable_vtable_t paintable_vtable = {
        _rut_color_picker_paint
    };

    static rut_sizable_vtable_t sizable_vtable = {
        rut_color_picker_set_size,
        rut_color_picker_get_size,
        rut_color_picker_get_preferred_width,
        rut_color_picker_get_preferred_height,
        NULL /* add_preferred_size_callback */
    };

    rut_type_t *type = &rut_color_picker_type;
#define TYPE rut_color_picker_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rut_color_picker_free);
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

static cg_pipeline_t *
create_hs_pipeline(cg_device_t *dev)
{
    cg_pipeline_t *pipeline;

    pipeline = cg_pipeline_new(dev);

    cg_pipeline_set_layer_null_texture(pipeline, 0, CG_TEXTURE_TYPE_2D);
    cg_pipeline_set_layer_filters(pipeline,
                                  0, /* layer */
                                  CG_PIPELINE_FILTER_NEAREST,
                                  CG_PIPELINE_FILTER_NEAREST);
    cg_pipeline_set_layer_wrap_mode(pipeline,
                                    0, /* layer */
                                    CG_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);

    return pipeline;
}

static void
create_dot_pipeline(rut_color_picker_t *picker)
{
    cg_texture_t *texture;
    c_error_t *error = NULL;

    picker->dot_pipeline = cg_pipeline_new(picker->shell->cg_device);

    texture = rut_load_texture_from_data_file(
        picker->shell, "color-picker-dot.png", &error);

    if (texture == NULL) {
        picker->dot_width = 8;
        picker->dot_height = 8;
        c_error_free(error);
    } else {
        picker->dot_width = cg_texture_get_width(texture);
        picker->dot_height = cg_texture_get_height(texture);

        cg_pipeline_set_layer_texture(picker->dot_pipeline,
                                      0, /* layer */
                                      texture);

        cg_object_unref(texture);
    }
}

static cg_pipeline_t *
create_bg_pipeline(cg_device_t *dev)
{
    cg_pipeline_t *pipeline;

    pipeline = cg_pipeline_new(dev);

    cg_pipeline_set_color4ub(pipeline, 0, 0, 0, 200);

    return pipeline;
}

static void
set_value(rut_color_picker_t *picker, float value)
{
    if (picker->value != value) {
        picker->hs_pipeline_dirty = true;
        picker->value = value;
    }
}

static void
set_hue_saturation(rut_color_picker_t *picker, float hue, float saturation)
{
    if (picker->hue != hue || picker->saturation != saturation) {
        picker->v_pipeline_dirty = true;
        picker->hue = hue;
        picker->saturation = saturation;
    }
}

static void
set_color_hsv(rut_color_picker_t *picker, const float hsv[3])
{
    hsv_to_rgb(hsv, &picker->color.red);

    rig_property_dirty(&picker->shell->property_ctx,
                       &picker->properties[RUT_COLOR_PICKER_PROP_COLOR]);

    rut_shell_queue_redraw(picker->shell);
}

static void
update_hs_from_event(rut_color_picker_t *picker,
                     rut_input_event_t *event)
{
    float x, y, dx, dy;
    float hsv[3];

    rut_motion_event_unproject(event, picker, &x, &y);

    dx = x - RUT_COLOR_PICKER_HS_CENTER_X;
    dy = y - RUT_COLOR_PICKER_HS_CENTER_Y;

    hsv[0] = atan2f(dy, dx) + G_PI;
    hsv[1] = sqrtf(dx * dx + dy * dy) / RUT_COLOR_PICKER_HS_SIZE * 2.0f;
    hsv[2] = picker->value;

    if (hsv[1] > 1.0f)
        hsv[1] = 1.0f;

    set_hue_saturation(picker, hsv[0], hsv[1]);

    set_color_hsv(picker, hsv);
}

static void
update_v_from_event(rut_color_picker_t *picker,
                    rut_input_event_t *event)
{
    float x, y;
    float hsv[3];

    rut_motion_event_unproject(event, picker, &x, &y);

    hsv[0] = picker->hue;
    hsv[1] = picker->saturation;
    hsv[2] = 1.0f - (y - RUT_COLOR_PICKER_V_Y) / RUT_COLOR_PICKER_V_HEIGHT;

    if (hsv[2] > 1.0f)
        hsv[2] = 1.0f;
    else if (hsv[2] < 0.0f)
        hsv[2] = 0.0f;

    set_value(picker, hsv[2]);

    set_color_hsv(picker, hsv);
}

static rut_input_event_status_t
grab_input_cb(rut_input_event_t *event,
              void *user_data)
{
    rut_color_picker_t *picker = user_data;

    if (rut_input_event_get_type(event) != RUT_INPUT_EVENT_TYPE_MOTION)
        return RUT_INPUT_EVENT_STATUS_UNHANDLED;

    if (rut_motion_event_get_action(event) == RUT_MOTION_EVENT_ACTION_MOVE) {
        switch (picker->grab) {
        case RUT_COLOR_PICKER_GRAB_V:
            update_v_from_event(picker, event);
            break;

        case RUT_COLOR_PICKER_GRAB_HS:
            update_hs_from_event(picker, event);
            break;

        default:
            break;
        }
    }

    if ((rut_motion_event_get_button_state(event) & RUT_BUTTON_STATE_1) == 0)
        ungrab(picker);

    return RUT_INPUT_EVENT_STATUS_HANDLED;
}

static void
ungrab(rut_color_picker_t *picker)
{
    if (picker->grab != RUT_COLOR_PICKER_GRAB_NONE) {
        rut_shell_ungrab_input(picker->shell, grab_input_cb, picker);

        picker->grab = RUT_COLOR_PICKER_GRAB_NONE;
    }
}

static rut_input_event_status_t
input_region_cb(rut_input_region_t *region,
                rut_input_event_t *event,
                void *user_data)
{
    rut_color_picker_t *picker = user_data;
    rut_object_t *camera;
    float x, y;

    if (picker->grab == RUT_COLOR_PICKER_GRAB_NONE &&
        rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION &&
        rut_motion_event_get_action(event) == RUT_MOTION_EVENT_ACTION_DOWN &&
        (rut_motion_event_get_button_state(event) &RUT_BUTTON_STATE_1) &&
        (camera = rut_input_event_get_camera(event)) &&
        rut_motion_event_unproject(event, picker, &x, &y)) {
        if (x >= RUT_COLOR_PICKER_V_X &&
            x < RUT_COLOR_PICKER_V_X + RUT_COLOR_PICKER_V_WIDTH &&
            y >= RUT_COLOR_PICKER_V_Y &&
            y < RUT_COLOR_PICKER_V_Y + RUT_COLOR_PICKER_V_HEIGHT) {
            picker->grab = RUT_COLOR_PICKER_GRAB_V;
            rut_shell_grab_input(
                picker->shell, camera, grab_input_cb, picker);

            update_v_from_event(picker, event);

            return RUT_INPUT_EVENT_STATUS_HANDLED;
        } else {
            float dx = x - RUT_COLOR_PICKER_HS_CENTER_X;
            float dy = y - RUT_COLOR_PICKER_HS_CENTER_Y;
            float distance = sqrtf(dx * dx + dy * dy);

            if (distance < RUT_COLOR_PICKER_HS_SIZE / 2.0f) {
                picker->grab = RUT_COLOR_PICKER_GRAB_HS;

                rut_shell_grab_input(
                    picker->shell, camera, grab_input_cb, picker);

                update_hs_from_event(picker, event);

                return RUT_INPUT_EVENT_STATUS_HANDLED;
            }
        }
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

rut_color_picker_t *
rut_color_picker_new(rut_shell_t *shell)
{
    rut_color_picker_t *picker = rut_object_alloc0(rut_color_picker_t,
                                                   &rut_color_picker_type,
                                                   _rut_color_picker_init_type);

    picker->shell = rut_object_ref(shell);

    cg_color_init_from_4ub(&picker->color, 0, 0, 0, 255);

    picker->hs_pipeline = create_hs_pipeline(shell->cg_device);
    picker->hs_pipeline_dirty = true;

    picker->v_pipeline = cg_pipeline_copy(picker->hs_pipeline);
    picker->v_pipeline_dirty = true;

    create_dot_pipeline(picker);

    picker->bg_pipeline = create_bg_pipeline(shell->cg_device);

    rut_paintable_init(picker);
    rut_graphable_init(picker);

    rut_introspectable_init(
        picker, _rut_color_picker_prop_specs, picker->properties);

    picker->input_region =
        rut_input_region_new_rectangle( /* x1 */
            RUT_COLOR_PICKER_HS_X,
            /* y1 */
            RUT_COLOR_PICKER_HS_Y,
            /* x2 */
            RUT_COLOR_PICKER_V_X +
            RUT_COLOR_PICKER_V_WIDTH,
            /* y2 */
            RUT_COLOR_PICKER_V_Y +
            RUT_COLOR_PICKER_V_HEIGHT,
            input_region_cb,
            picker);
    rut_graphable_add_child(picker, picker->input_region);

    rut_sizable_set_size(
        picker, RUT_COLOR_PICKER_TOTAL_WIDTH, RUT_COLOR_PICKER_TOTAL_HEIGHT);

    return picker;
}

void
rut_color_picker_set_color(rut_object_t *obj, const cg_color_t *color)
{
    rut_color_picker_t *picker = obj;

    if (memcmp(&picker->color, color, sizeof(cg_color_t))) {
        float hsv[3];

        picker->color = *color;

        rgb_to_hsv(&color->red, hsv);

        set_hue_saturation(picker, hsv[0], hsv[1]);
        set_value(picker, hsv[2]);

        rig_property_dirty(&picker->shell->property_ctx,
                           &picker->properties[RUT_COLOR_PICKER_PROP_COLOR]);

        rut_shell_queue_redraw(picker->shell);
    }
}

const cg_color_t *
rut_color_picker_get_color(rut_color_picker_t *picker)
{
    return &picker->color;
}
