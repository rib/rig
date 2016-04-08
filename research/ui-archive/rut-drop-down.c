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
#include <cogl-path/cogl-path.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <errno.h>

#include "rut-drop-down.h"
#include "rut-text.h"
#include "rut-inputable.h"
#include "rut-pickable.h"
#include "rut-input-region.h"
#include "rut-texture-cache.h"

#include "rut-camera.h"

#define RUT_DROP_DOWN_EDGE_WIDTH 8
#define RUT_DROP_DOWN_EDGE_HEIGHT 16

#define RUT_DROP_DOWN_FONT_SIZE 10

enum {
    RUT_DROP_DOWN_PROP_VALUE,
    RUT_DROP_DOWN_N_PROPS
};

typedef struct {
    PangoLayout *layout;
    PangoRectangle logical_rect;
    PangoRectangle ink_rect;
} rut_drop_down_layout_t;

struct _rut_drop_down_t {
    rut_object_base_t _base;

    rut_shell_t *shell;

    rut_graphable_props_t graphable;
    rut_paintable_props_t paintable;

    cg_pipeline_t *bg_pipeline;
    cg_pipeline_t *highlighted_bg_pipeline;

    int width, height;

    /* Index of the selected value */
    int value_index;

    int n_values;
    rut_drop_down_value_t *values;

    rut_drop_down_layout_t *layouts;

    PangoFontDescription *font_description;

    rut_input_region_t *input_region;

    rig_introspectable_props_t introspectable;
    rig_property_t properties[RUT_DROP_DOWN_N_PROPS];

    /* This is set to true whenever the primary mouse button is clicked
     * on the widget and we have the grab */
    bool button_down;
    /* This is set to true when button_down is true and the pointer is
     * within the button */
    bool highlighted;

    bool selector_shown;
    int selector_x;
    int selector_y;
    int selector_width;
    int selector_height;
    int selector_value;
    cg_path_t *selector_outline_path;
    cg_pipeline_t *selector_outline_pipeline;
};

/* Some of the pipelines are cached and attached to the cg_device_t so
 * that multiple drop downs created using the same cg_device_t will
 * use the same pipelines */
typedef struct {
    cg_pipeline_t *bg_pipeline;
    cg_pipeline_t *highlighted_bg_pipeline;
} rut_drop_down_context_data_t;

rut_type_t rut_drop_down_type;

static void rut_drop_down_hide_selector(rut_drop_down_t *drop);

static rig_property_spec_t _rut_drop_down_prop_specs[] = {
    { .name = "value",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .getter.integer_type = rut_drop_down_get_value,
      .setter.integer_type = rut_drop_down_set_value },
    { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static rut_drop_down_context_data_t *
rut_drop_down_get_context_data(rut_shell_t *shell)
{
    static cg_user_data_key_t context_data_key;
    rut_drop_down_context_data_t *context_data = cg_object_get_user_data(
        CG_OBJECT(shell->cg_device), &context_data_key);

    if (context_data == NULL) {
        context_data = c_new0(rut_drop_down_context_data_t, 1);
        cg_object_set_user_data(CG_OBJECT(shell->cg_device),
                                &context_data_key,
                                context_data,
                                c_free);
    }

    return context_data;
}

static cg_pipeline_t *
rut_drop_down_create_bg_pipeline(rut_shell_t *shell)
{
    rut_drop_down_context_data_t *context_data =
        rut_drop_down_get_context_data(shell);

    /* The pipeline is cached so that if multiple drop downs are created
     * they will share a reference to the same pipeline */
    if (context_data->bg_pipeline)
        return cg_object_ref(context_data->bg_pipeline);
    else {
        cg_pipeline_t *pipeline = cg_pipeline_new(shell->cg_device);
        static cg_user_data_key_t bg_pipeline_destroy_key;
        cg_texture_t *bg_texture;
        c_error_t *error = NULL;

        bg_texture = rut_load_texture_from_data_file(shell,
                                                     "drop-down-background.png",
                                                     &error);
        if (bg_texture) {
            const cg_pipeline_wrap_mode_t wrap_mode =
                CG_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE;

            cg_pipeline_set_layer_texture(pipeline, 0, bg_texture);
            cg_pipeline_set_layer_wrap_mode(pipeline,
                                            0, /* layer_index */
                                            wrap_mode);
            cg_pipeline_set_layer_filters(pipeline,
                                          0, /* layer_index */
                                          CG_PIPELINE_FILTER_NEAREST,
                                          CG_PIPELINE_FILTER_NEAREST);
        } else {
            c_warning("Failed to load drop-down-background.png: %s",
                      error->message);
            c_error_free(error);
        }

        /* When the last drop down is destroyed the pipeline will be
         * destroyed and we'll set context->bg_pipeline to NULL so that
         * it will be recreated for the next drop down */
        cg_object_set_user_data(
            CG_OBJECT(pipeline),
            &bg_pipeline_destroy_key,
            &context_data->bg_pipeline,
            (cg_user_data_destroy_callback_t)g_nullify_pointer);

        context_data->bg_pipeline = pipeline;

        return pipeline;
    }
}

static cg_pipeline_t *
rut_drop_down_create_highlighted_bg_pipeline(rut_shell_t *shell)
{
    rut_drop_down_context_data_t *context_data =
        rut_drop_down_get_context_data(shell);

    /* The pipeline is cached so that if multiple drop downs are created
     * they will share a reference to the same pipeline */
    if (context_data->highlighted_bg_pipeline)
        return cg_object_ref(context_data->highlighted_bg_pipeline);
    else {
        cg_pipeline_t *bg_pipeline = rut_drop_down_create_bg_pipeline(shell);
        cg_pipeline_t *pipeline = cg_pipeline_copy(bg_pipeline);
        static cg_user_data_key_t pipeline_destroy_key;
        cg_snippet_t *snippet;

        cg_object_unref(bg_pipeline);

        /* Invert the colours of the texture so that there is some
         * obvious feedback when the button is pressed. */
        /* What we want is 1-colour. However we want this to remain
         * pre-multiplied so what we actually want is alpha×(1-colour) =
         * alpha-alpha×colour. The texture is already premultiplied so
         * the colour values are already alpha×colour and we just need
         * to subtract it from the alpha value. */
        snippet = cg_snippet_new(CG_SNIPPET_FIRST_LAYER_FRAGMENT_HOOK, NULL, NULL);
        cg_snippet_set_replace(snippet, "frag.rgb = vec3(frag.a, frag.a, frag.a) - frag.rgb;\n");

        cg_pipeline_add_snippet(pipeline, 1 /* layer number */, snippet);
        cg_object_unref(snippet);

        /* When the last drop down is destroyed the pipeline will be
         * destroyed and we'll set context->highlighted_bg_pipeline to NULL
         * so that it will be recreated for the next drop down */
        cg_object_set_user_data(
            CG_OBJECT(pipeline),
            &pipeline_destroy_key,
            &context_data->highlighted_bg_pipeline,
            (cg_user_data_destroy_callback_t)g_nullify_pointer);

        context_data->highlighted_bg_pipeline = pipeline;

        return pipeline;
    }
}

static void
rut_drop_down_clear_layouts(rut_drop_down_t *drop)
{
    if (drop->layouts) {
        int i;

        for (i = 0; i < drop->n_values; i++)
            g_object_unref(drop->layouts[i].layout);

        c_free(drop->layouts);
        drop->layouts = NULL;
    }
}

static void
rut_drop_down_free_values(rut_drop_down_t *drop)
{
    if (drop->values) {
        int i = 0;

        for (i = 0; i < drop->n_values; i++)
            c_free((char *)drop->values[i].name);

        c_free(drop->values);
        drop->values = NULL;
    }
}

static void
_rut_drop_down_free(void *object)
{
    rut_drop_down_t *drop = object;

    rut_object_unref(drop->shell);
    cg_object_unref(drop->bg_pipeline);
    cg_object_unref(drop->highlighted_bg_pipeline);

    rut_drop_down_free_values(drop);

    rut_drop_down_clear_layouts(drop);

    rut_graphable_remove_child(drop->input_region);
    rut_object_unref(drop->input_region);

    rig_introspectable_destroy(drop);

    rut_shell_remove_pre_paint_callback_by_graphable(drop->shell,
                                                     drop);
    rut_graphable_destroy(drop);

    pango_font_description_free(drop->font_description);

    rut_drop_down_hide_selector(drop);
    if (drop->selector_outline_pipeline)
        cg_object_unref(drop->selector_outline_pipeline);

    rut_object_free(rut_drop_down_t, drop);
}

typedef struct {
    float x1, y1, x2, y2;
    float s1, t1, s2, t2;
} rut_drop_down_rectangle_t;

static PangoFontDescription *
rut_drop_down_create_font_description(void)
{
    PangoFontDescription *font_description = pango_font_description_new();

    pango_font_description_set_family(font_description, "Sans");
    pango_font_description_set_absolute_size(
        font_description, RUT_DROP_DOWN_FONT_SIZE * PANGO_SCALE);

    return font_description;
}

static void
rut_drop_down_ensure_layouts(rut_drop_down_t *drop)
{
    if (drop->layouts == NULL) {
        int i;

        drop->layouts = c_new(rut_drop_down_layout_t, drop->n_values);

        for (i = 0; i < drop->n_values; i++) {
            rut_drop_down_layout_t *layout = drop->layouts + i;

            layout->layout = pango_layout_new(drop->shell->pango_context);

            pango_layout_set_text(layout->layout, drop->values[i].name, -1);

            pango_layout_set_font_description(layout->layout,
                                              drop->font_description);

            pango_layout_get_pixel_extents(
                layout->layout, &layout->ink_rect, &layout->logical_rect);

            cg_pango_ensure_glyph_cache_for_layout(layout->layout);
        }
    }
}

static void
rut_drop_down_paint_selector(rut_drop_down_t *drop,
                             rut_paint_context_t *paint_ctx)
{
    rut_object_t *camera = paint_ctx->camera;
    cg_framebuffer_t *fb = rut_camera_get_framebuffer(camera);
    int i, y_pos = drop->selector_y + 3;

    cg_framebuffer_draw_textured_rectangle(
        fb,
        drop->bg_pipeline,
        drop->selector_x,
        drop->selector_y,
        drop->selector_x + drop->selector_width,
        drop->selector_y + drop->selector_height,
        /* Stretch centre pixel of
           bg texture to entire
           rectangle */
        0.5f,
        0.5f,
        0.5f,
        0.5f);

    cg_path_stroke(
        drop->selector_outline_path, fb, drop->selector_outline_pipeline);

    rut_drop_down_ensure_layouts(drop);

    for (i = 0; i < drop->n_values; i++) {
        rut_drop_down_layout_t *layout = drop->layouts + i;
        int x_pos = (drop->selector_x + drop->selector_width / 2 -
                     layout->logical_rect.width / 2);
        cg_color_t font_color;

        if (i == drop->selector_value) {
            cg_pipeline_t *pipeline = drop->highlighted_bg_pipeline;

            cg_framebuffer_draw_textured_rectangle(
                fb,
                pipeline,
                drop->selector_x,
                y_pos,
                drop->selector_x + drop->selector_width - 1,
                y_pos + layout->logical_rect.height,
                /* Stretch centre pixel of
                   bg texture to entire
                   rectangle */
                0.5f,
                0.5f,
                0.5f,
                0.5f);
            cg_color_init_from_4ub(&font_color, 255, 255, 255, 255);
        } else
            cg_color_init_from_4ub(&font_color, 0, 0, 0, 255);

        cg_pango_show_layout(fb, layout->layout, x_pos, y_pos, &font_color);

        y_pos += layout->logical_rect.height;
    }
}

static void
rut_drop_down_paint_button(rut_drop_down_t *drop,
                           rut_paint_context_t *paint_ctx)
{
    rut_object_t *camera = paint_ctx->camera;
    cg_framebuffer_t *fb = rut_camera_get_framebuffer(camera);
    rut_drop_down_rectangle_t coords[7];
    int translation = drop->width - RUT_DROP_DOWN_EDGE_WIDTH;
    cg_color_t font_color;
    rut_drop_down_layout_t *layout;
    int i;

    /* Top left rounded corner */
    coords[0].x1 = 0.0f;
    coords[0].y1 = 0.0f;
    coords[0].x2 = RUT_DROP_DOWN_EDGE_WIDTH;
    coords[0].y2 = RUT_DROP_DOWN_EDGE_HEIGHT / 2;
    coords[0].s1 = 0.0f;
    coords[0].t1 = 0.0f;
    coords[0].s2 = 0.5f;
    coords[0].t2 = 0.5f;

    /* Centre gap */
    coords[1].x1 = 0.0f;
    coords[1].y1 = coords[0].y2;
    coords[1].x2 = RUT_DROP_DOWN_EDGE_WIDTH;
    coords[1].y2 = drop->height - RUT_DROP_DOWN_EDGE_HEIGHT / 2;
    coords[1].s1 = 0.0f;
    coords[1].t1 = 0.5f;
    coords[1].s2 = 0.5f;
    coords[1].t2 = 0.5f;

    /* Bottom left rounded corner */
    coords[2].x1 = 0.0f;
    coords[2].y1 = coords[1].y2;
    coords[2].x2 = RUT_DROP_DOWN_EDGE_WIDTH;
    coords[2].y2 = drop->height;
    coords[2].s1 = 0.0f;
    coords[2].t1 = 0.5f;
    coords[2].s2 = 0.5f;
    coords[2].t2 = 1.0f;

    /* Centre rectangle */
    coords[3].x1 = RUT_DROP_DOWN_EDGE_WIDTH;
    coords[3].y1 = 0.0f;
    coords[3].x2 = drop->width - RUT_DROP_DOWN_EDGE_WIDTH;
    coords[3].y2 = drop->height;
    /* Stetch the centre pixel to cover the entire rectangle */
    coords[3].s1 = 0.5f;
    coords[3].t1 = 0.5f;
    coords[3].s2 = 0.5f;
    coords[3].t2 = 0.5f;

    /* The right hand side rectangles are just translated copies of the
     * left hand side rectangles with the texture coordinates shifted
     * over to the other half */
    for (i = 0; i < 3; i++) {
        rut_drop_down_rectangle_t *out = coords + i + 4;
        const rut_drop_down_rectangle_t *in = coords + i;

        out->x1 = in->x1 + translation;
        out->y1 = in->y1;
        out->x2 = in->x2 + translation;
        out->y2 = in->y2;
        out->s1 = in->s1 + 0.5f;
        out->t1 = in->t1;
        out->s2 = in->s2 + 0.5f;
        out->t2 = in->t2;
    }

    cg_framebuffer_draw_textured_rectangles(
        fb,
        drop->highlighted ? drop->highlighted_bg_pipeline : drop->bg_pipeline,
        (float *)coords,
        C_N_ELEMENTS(coords));

    rut_drop_down_ensure_layouts(drop);

    cg_color_init_from_4ub(&font_color, 0, 0, 0, 255);

    if (drop->n_values) {
        layout = drop->layouts + drop->value_index;
        cg_pango_show_layout(fb,
                             layout->layout,
                             drop->width / 2 - layout->logical_rect.width / 2,
                             drop->height / 2 - layout->logical_rect.height / 2,
                             &font_color);
    }
}

static void
_rut_drop_down_paint(rut_object_t *object,
                     rut_paint_context_t *paint_ctx)
{
    rut_drop_down_t *drop = object;

    switch (paint_ctx->layer_number) {
    case 0:
        rut_drop_down_paint_button(drop, paint_ctx);

        /* If the selector is visible then we'll queue it to be painted
         * in the next layer so that it won't appear under the
         * subsequent controls */
        if (drop->selector_shown)
            rut_paint_context_queue_paint(paint_ctx, object);
        break;

    case 1:
        rut_drop_down_paint_selector(drop, paint_ctx);
        break;
    }
}

static int
rut_drop_down_find_value_at_position(rut_drop_down_t *drop, float x, float y)
{
    int i, y_pos = drop->selector_y + 3;

    if (x < drop->selector_x || x >= drop->selector_x + drop->selector_width)
        return -1;

    for (i = 0; i < drop->n_values; i++) {
        const rut_drop_down_layout_t *layout = drop->layouts + i;

        if (y >= y_pos && y < y_pos + layout->logical_rect.height)
            return i;

        y_pos += layout->logical_rect.height;
    }

    return -1;
}

static rut_input_event_status_t
rut_drop_down_selector_grab_cb(rut_input_event_t *event, void *user_data)
{
    rut_drop_down_t *drop = user_data;

    switch (rut_input_event_get_type(event)) {
    case RUT_INPUT_EVENT_TYPE_MOTION: {
        float x, y;
        int selector_value;

        if (rut_motion_event_unproject(event, drop, &x, &y))
            selector_value = rut_drop_down_find_value_at_position(drop, x, y);
        else
            selector_value = -1;

        if (selector_value != drop->selector_value) {
            drop->selector_value = selector_value;
            rut_shell_queue_redraw(drop->shell);
        }

        /* If this is a click then commit the chosen value */
        if (rut_motion_event_get_action(event) ==
            RUT_MOTION_EVENT_ACTION_DOWN) {
            rut_drop_down_hide_selector(drop);

            if (selector_value != -1)
                rut_drop_down_set_value(drop, selector_value);

            return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    } break;

    case RUT_INPUT_EVENT_TYPE_KEY:
        /* The escape key cancels the selector */
        if (rut_key_event_get_action(event) == RUT_KEY_EVENT_ACTION_DOWN &&
            rut_key_event_get_keysym(event) == RUT_KEY_Escape)
            rut_drop_down_hide_selector(drop);
        break;

    default:
        break;
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
rut_drop_down_handle_click(rut_drop_down_t *drop,
                           rut_input_event_t *event)
{
    c_matrix_t modelview;
    const c_matrix_t *projection;
    rut_object_t *camera = rut_input_event_get_camera(event);
    float top_point[4];
    int i;

    drop->selector_width = MAX(drop->width - 6, 0);
    drop->selector_height = 0;

    /* Calculate the size of the selector */
    for (i = 0; i < drop->n_values; i++) {
        rut_drop_down_layout_t *layout = drop->layouts + i;

        if (layout->logical_rect.width > drop->selector_width)
            drop->selector_width = layout->logical_rect.width;

        drop->selector_height += layout->logical_rect.height;
    }

    /* Add three pixels all sides for a 1-pixel border and a two pixel
     * gap */
    drop->selector_width += 6;
    drop->selector_height += 6;

    drop->selector_x = drop->width / 2 - drop->selector_width / 2;

    /* Check whether putting the selector below the control would make
     * it go off the screen */
    rut_graphable_get_modelview(drop, camera, &modelview);
    projection = rut_camera_get_projection(camera);
    top_point[0] = drop->selector_x;
    top_point[1] = drop->selector_height + drop->height;
    c_matrix_transform_points(&modelview,
                               2, /* n_components */
                               sizeof(float) * 4, /* stride_in */
                               top_point, /* points_in */
                               sizeof(float) * 4, /* stride_out */
                               top_point, /* points_out */
                               1 /* n_points */);
    c_matrix_project_points(projection,
                             3, /* n_components */
                             sizeof(float) * 4, /* stride_in */
                             top_point, /* points_in */
                             sizeof(float) * 4, /* stride_out */
                             top_point, /* points_out */
                             1 /* n_points */);
    top_point[1] /= top_point[3];

    if (top_point[1] >= -1.0f)
        drop->selector_y = drop->height;
    else
        drop->selector_y = -drop->selector_height;

    if (drop->selector_outline_pipeline == NULL) {
        drop->selector_outline_pipeline =
            cg_pipeline_new(drop->shell->cg_device);
        cg_pipeline_set_color4ub(drop->selector_outline_pipeline, 0, 0, 0, 255);
    }

    drop->selector_outline_path = cg_path_new(drop->shell->cg_device);
    cg_path_rectangle(drop->selector_outline_path,
                      drop->selector_x,
                      drop->selector_y,
                      drop->selector_x + drop->selector_width,
                      drop->selector_y + drop->selector_height);

    rut_shell_grab_input(drop->shell,
                         rut_input_event_get_camera(event),
                         rut_drop_down_selector_grab_cb,
                         drop);

    drop->selector_shown = true;
    drop->selector_value = -1;

    rut_shell_queue_redraw(drop->shell);
}

static rut_input_event_status_t
rut_drop_down_input_cb(rut_input_event_t *event,
                       void *user_data)
{
    rut_drop_down_t *drop = user_data;
    bool highlighted;
    float x, y;

    if (rut_input_event_get_type(event) != RUT_INPUT_EVENT_TYPE_MOTION)
        return RUT_INPUT_EVENT_STATUS_UNHANDLED;

    x = rut_motion_event_get_x(event);
    y = rut_motion_event_get_y(event);

    if ((rut_motion_event_get_button_state(event) & RUT_BUTTON_STATE_1) == 0) {
        drop->button_down = false;
        rut_shell_ungrab_input(
            drop->shell, rut_drop_down_input_cb, user_data);

        /* If we the pointer is still over the widget then treat it as a
         * click */
        if (drop->highlighted)
            rut_drop_down_handle_click(drop, event);

        highlighted = false;
    } else {
        rut_object_t *camera = rut_input_event_get_camera(event);

        highlighted = rut_pickable_pick(drop->input_region,
                                        camera,
                                        NULL, /* pre-computed modelview */
                                        x,
                                        y);
    }

    if (highlighted != drop->highlighted) {
        drop->highlighted = highlighted;
        rut_shell_queue_redraw(drop->shell);
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static rut_input_event_status_t
rut_drop_down_input_region_cb(
    rut_input_region_t *region, rut_input_event_t *event, void *user_data)
{
    rut_drop_down_t *drop = user_data;
    rut_object_t *camera;

    if (!drop->button_down && !drop->selector_shown &&
        rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION &&
        rut_motion_event_get_action(event) == RUT_MOTION_EVENT_ACTION_DOWN &&
        (rut_motion_event_get_button_state(event) &RUT_BUTTON_STATE_1) &&
        (camera = rut_input_event_get_camera(event))) {
        drop->button_down = true;
        drop->highlighted = true;

        rut_shell_grab_input(
            drop->shell, camera, rut_drop_down_input_cb, drop);

        rut_shell_queue_redraw(drop->shell);

        return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
rut_drop_down_hide_selector(rut_drop_down_t *drop)
{
    if (drop->selector_shown) {
        cg_object_unref(drop->selector_outline_path);
        drop->selector_shown = false;
        rut_shell_queue_redraw(drop->shell);

        rut_shell_ungrab_input(
            drop->shell, rut_drop_down_selector_grab_cb, drop);
    }
}

static void
rut_drop_down_set_size(rut_object_t *object, float width, float height)
{
    rut_drop_down_t *drop = object;

    rut_shell_queue_redraw(drop->shell);
    drop->width = width;
    drop->height = height;
    rut_input_region_set_rectangle(drop->input_region,
                                   0.0f,
                                   0.0f, /* x0 / y0 */
                                   drop->width,
                                   drop->height /* x1 / y1 */);
}

static void
rut_drop_down_get_size(rut_object_t *object, float *width, float *height)
{
    rut_drop_down_t *drop = object;

    *width = drop->width;
    *height = drop->height;
}

static void
rut_drop_down_get_preferred_width(rut_object_t *object,
                                  float for_height,
                                  float *min_width_p,
                                  float *natural_width_p)
{
    rut_drop_down_t *drop = object;
    int max_width = 0;
    int i;

    rut_drop_down_ensure_layouts(drop);

    /* Get the narrowest layout */
    for (i = 0; i < drop->n_values; i++) {
        rut_drop_down_layout_t *layout = drop->layouts + i;

        if (layout->logical_rect.width > max_width)
            max_width = layout->logical_rect.width;
    }

    /* Add space for the edges */
    max_width += RUT_DROP_DOWN_EDGE_WIDTH * 2;

    if (min_width_p)
        *min_width_p = max_width;
    if (natural_width_p)
        /* Leave two pixels either side of the label */
        *natural_width_p = max_width + 4;
}

static void
rut_drop_down_get_preferred_height(rut_object_t *object,
                                   float for_width,
                                   float *min_height_p,
                                   float *natural_height_p)
{
    rut_drop_down_t *drop = object;
    int max_height = G_MININT;
    int i;

    rut_drop_down_ensure_layouts(drop);

    /* Get the tallest layout */
    for (i = 0; i < drop->n_values; i++) {
        rut_drop_down_layout_t *layout = drop->layouts + i;

        if (layout->logical_rect.height > max_height)
            max_height = layout->logical_rect.height;
    }

    if (min_height_p)
        *min_height_p = MAX(max_height, RUT_DROP_DOWN_EDGE_HEIGHT);
    if (natural_height_p)
        *natural_height_p = MAX(max_height + 4, RUT_DROP_DOWN_EDGE_HEIGHT);
}

static void
_rut_drop_down_init_type(void)
{
    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child removed */
                                                       NULL, /* child addded */
                                                       NULL /* parent changed */
    };

    static rut_paintable_vtable_t paintable_vtable = { _rut_drop_down_paint };

    static rut_sizable_vtable_t sizable_vtable = {
        rut_drop_down_set_size,            rut_drop_down_get_size,
        rut_drop_down_get_preferred_width, rut_drop_down_get_preferred_height,
        NULL /* add_preferred_size_callback */
    };

    rut_type_t *type = &rut_drop_down_type;
#define TYPE rut_drop_down_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rut_drop_down_free);
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

rut_drop_down_t *
rut_drop_down_new(rut_shell_t *shell)
{
    rut_drop_down_t *drop = rut_object_alloc0(
        rut_drop_down_t, &rut_drop_down_type, _rut_drop_down_init_type);

    drop->shell = rut_object_ref(shell);

    /* Set a dummy value so we can assume that value_index is always a
     * valid index */
    drop->values = c_new(rut_drop_down_value_t, 1);
    drop->values->name = c_strdup("");
    drop->values->value = 0;

    drop->font_description = rut_drop_down_create_font_description();

    rut_paintable_init(drop);
    rut_graphable_init(drop);

    rig_introspectable_init(drop, _rut_drop_down_prop_specs, drop->properties);

    drop->bg_pipeline = rut_drop_down_create_bg_pipeline(shell);
    drop->highlighted_bg_pipeline =
        rut_drop_down_create_highlighted_bg_pipeline(shell);

    drop->input_region = rut_input_region_new_rectangle(
        0, 0, 0, 0, rut_drop_down_input_region_cb, drop);
    rut_graphable_add_child(drop, drop->input_region);

    rut_sizable_set_size(drop, 60, 30);

    return drop;
}

void
rut_drop_down_set_value(rut_object_t *obj, int value)
{
    rut_drop_down_t *drop = obj;

    int i;

    if (value == drop->values[drop->value_index].value)
        return;

    for (i = 0; i < drop->n_values; i++)
        if (drop->values[i].value == value) {
            drop->value_index = i;

            rig_property_dirty(&drop->shell->property_ctx,
                               &drop->properties[RUT_DROP_DOWN_PROP_VALUE]);

            rut_shell_queue_redraw(drop->shell);
            return;
        }

    c_warn_if_reached();
}

int
rut_drop_down_get_value(rut_object_t *obj)
{
    rut_drop_down_t *drop = obj;

    return drop->values[drop->value_index].value;
}

void
rut_drop_down_set_values(rut_drop_down_t *drop, ...)
{
    va_list ap1, ap2;
    const char *name;
    int i = 0, n_values = 0;
    rut_drop_down_value_t *values;

    va_start(ap1, drop);
    G_VA_COPY(ap2, ap1);

    while ((name = va_arg(ap1, const char *))) {
        /* Skip the value */
        va_arg(ap1, int);
        n_values++;
    }

    values = c_alloca(sizeof(rut_drop_down_value_t) * n_values);

    while ((name = va_arg(ap2, const char *))) {
        values[i].name = name;
        values[i].value = va_arg(ap2, int);
        i++;
    }

    va_end(ap1);

    rut_drop_down_set_values_array(drop, values, n_values);
}

void
rut_drop_down_set_values_array(rut_drop_down_t *drop,
                               const rut_drop_down_value_t *values,
                               int n_values)
{
    int old_value;
    int old_value_index = 0;
    int i;

    c_return_if_fail(n_values >= 0);

    old_value = rut_drop_down_get_value(drop);

    rut_drop_down_free_values(drop);
    rut_drop_down_clear_layouts(drop);

    drop->values = c_malloc(sizeof(rut_drop_down_value_t) * n_values);
    for (i = 0; i < n_values; i++) {
        drop->values[i].name = c_strdup(values[i].name);
        drop->values[i].value = values[i].value;

        if (values[i].value == old_value)
            old_value_index = i;
    }

    drop->n_values = n_values;

    drop->value_index = old_value_index;

    rut_shell_queue_redraw(drop->shell);
}
