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

#include <cglib/cglib.h>
#include <string.h>
#include <math.h>

#include "rut-interfaces.h"
#include "rut-paintable.h"
#include "rut-image.h"
#include "rut-camera.h"
#include "rut-introspectable.h"

enum {
    RUT_IMAGE_PROP_DRAW_MODE,
    RUT_IMAGE_N_PROPS
};

struct _rut_image_t {
    rut_object_base_t _base;

    float width, height;
    int tex_width, tex_height;

    /* Cached rectangle to use when the draw mode is
     * SCALE_WITH_ASPECT_RATIO */
    float fit_x1, fit_y1;
    float fit_x2, fit_y2;

    rut_shell_t *shell;

    rut_paintable_props_t paintable;
    rut_graphable_props_t graphable;

    c_list_t preferred_size_cb_list;

    rut_introspectable_props_t introspectable;
    rut_property_t properties[RUT_IMAGE_N_PROPS];

    cg_pipeline_t *pipeline;

    rut_image_draw_mode_t draw_mode;
};

rut_type_t rut_image_type;

static rut_ui_enum_t _rut_image_draw_mode_ui_enum = {
    .nick = "Draw mode",
    .values = { { RUT_IMAGE_DRAW_MODE_1_TO_1, "1 to 1",
                  "Show the full image at a 1:1 ratio" },
                { RUT_IMAGE_DRAW_MODE_REPEAT, "Repeat",
                  "Fill the widget with repeats of the image" },
                { RUT_IMAGE_DRAW_MODE_SCALE, "Scale",
                  "Scale the image to fill the size of the widget" },
                { RUT_IMAGE_DRAW_MODE_SCALE_WITH_ASPECT_RATIO,
                  "Scale with aspect ratio", "Scale the image to fill the size "
                  "of the widget but maintain the "
                  "aspect"
                  "ration" },
                { 0 } }
};

static rut_property_spec_t _rut_image_prop_specs[] = {
    { .name = "draw_mode",
      .type = RUT_PROPERTY_TYPE_ENUM,
      .data_offset = offsetof(rut_image_t, draw_mode),
      .setter.any_type = rut_image_set_draw_mode,
      .flags = (RUT_PROPERTY_FLAG_READWRITE | RUT_PROPERTY_FLAG_VALIDATE),
      .validation = { .ui_enum = &_rut_image_draw_mode_ui_enum } },
    { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rut_image_free(void *object)
{
    rut_image_t *image = object;

    rut_closure_list_disconnect_all_FIXME(&image->preferred_size_cb_list);

    rut_graphable_destroy(image);

    cg_object_unref(image->pipeline);

    rut_object_free(rut_image_t, image);
}

static void
_rut_image_paint(rut_object_t *object,
                 rut_paint_context_t *paint_ctx)
{
    rut_image_t *image = object;
    cg_framebuffer_t *fb = rut_camera_get_framebuffer(paint_ctx->camera);

    switch (image->draw_mode) {
    case RUT_IMAGE_DRAW_MODE_1_TO_1:
        cg_framebuffer_draw_rectangle(fb,
                                      image->pipeline,
                                      0.0f,
                                      0.0f,
                                      image->tex_width,
                                      image->tex_height);
        break;

    case RUT_IMAGE_DRAW_MODE_SCALE:
        cg_framebuffer_draw_rectangle(
            fb, image->pipeline, 0.0f, 0.0f, image->width, image->height);
        break;

    case RUT_IMAGE_DRAW_MODE_REPEAT:
        cg_framebuffer_draw_textured_rectangle(fb,
                                               image->pipeline,
                                               0.0f,
                                               0.0f,
                                               image->width,
                                               image->height,
                                               0.0f,
                                               0.0f,
                                               image->width / image->tex_width,
                                               image->height /
                                               image->tex_height);
        break;

    case RUT_IMAGE_DRAW_MODE_SCALE_WITH_ASPECT_RATIO:
        cg_framebuffer_draw_rectangle(fb,
                                      image->pipeline,
                                      image->fit_x1,
                                      image->fit_y1,
                                      image->fit_x2,
                                      image->fit_y2);
        break;
    }
}

static void
rut_image_set_size(void *object, float width, float height)
{
    rut_image_t *image = object;

    image->width = width;
    image->height = height;

    if (height == 0.0f) {
        image->fit_x1 = 0.0f;
        image->fit_y1 = 0.0f;
        image->fit_x2 = 0.0f;
        image->fit_y2 = 0.0f;
    } else {
        float widget_aspect = width / height;
        float tex_aspect = image->tex_width / (float)image->tex_height;

        if (tex_aspect > widget_aspect) {
            /* Fit the width */
            float draw_height = width / tex_aspect;
            image->fit_x1 = 0.0f;
            image->fit_x2 = width;
            image->fit_y1 = height / 2.0f - draw_height / 2.0f;
            image->fit_y2 = image->fit_y1 + draw_height;
        } else {
            /* Fit the height */
            float draw_width = height * tex_aspect;
            image->fit_y1 = 0.0f;
            image->fit_y2 = height;
            image->fit_x1 = width / 2.0f - draw_width / 2.0f;
            image->fit_x2 = image->fit_x1 + draw_width;
        }
    }

    rut_shell_queue_redraw(image->shell);
}

static void
rut_image_get_preferred_width(void *object,
                              float for_height,
                              float *min_width_p,
                              float *natural_width_p)
{
    rut_image_t *image = object;

    if (image->draw_mode == RUT_IMAGE_DRAW_MODE_1_TO_1) {
        if (min_width_p)
            *min_width_p = image->tex_width;
        if (natural_width_p)
            *natural_width_p = image->tex_width;
    } else if (image->draw_mode ==
               RUT_IMAGE_DRAW_MODE_SCALE_WITH_ASPECT_RATIO) {
        if (min_width_p)
            *min_width_p = 0.0f;

        if (natural_width_p) {
            if (for_height != -1.0f) {
                float aspect = (float)image->tex_width / image->tex_height;

                /* Our preferrence is to have just enough space to be
                 * able to show the image at 1:1, not to necessarily
                 * fill the for_height space...
                 */

                if (for_height > image->tex_height)
                    *natural_width_p = image->tex_width;
                else
                    *natural_width_p = for_height * aspect;
            } else
                *natural_width_p = image->tex_width;
        }
    } else {
        if (min_width_p)
            *min_width_p = 0.0f;

        if (natural_width_p) {
            if (for_height != -1.0f) {
                float aspect = image->tex_width / image->tex_height;

                *natural_width_p = for_height * aspect;
            } else
                *natural_width_p = image->tex_width;
        }
    }
}

static void
rut_image_get_preferred_height(void *object,
                               float for_width,
                               float *min_height_p,
                               float *natural_height_p)
{
    rut_image_t *image = object;

    if (image->draw_mode == RUT_IMAGE_DRAW_MODE_1_TO_1) {
        if (min_height_p)
            *min_height_p = image->tex_height;
        if (natural_height_p)
            *natural_height_p = image->tex_height;
    } else if (image->draw_mode ==
               RUT_IMAGE_DRAW_MODE_SCALE_WITH_ASPECT_RATIO) {
        if (min_height_p)
            *min_height_p = 0.0f;

        if (natural_height_p) {
            if (for_width != -1.0f) {
                float aspect = (float)image->tex_height / image->tex_width;

                /* Our preferrence is to have just enough space to be
                 * able to show the image at 1:1, not to necessarily
                 * fill the for_width space...
                 */

                if (for_width > image->tex_width)
                    *natural_height_p = image->tex_height;
                else
                    *natural_height_p = for_width * aspect;
            } else
                *natural_height_p = image->tex_height;
        }
    } else {
        if (min_height_p)
            *min_height_p = 0.0f;

        if (natural_height_p) {
            if (for_width != -1.0f) {
                float aspect = image->tex_height / image->tex_width;

                *natural_height_p = for_width * aspect;
            } else
                *natural_height_p = image->tex_height;
        }
    }
}

static rut_closure_t *
rut_image_add_preferred_size_callback(void *object,
                                      rut_sizeable_preferred_size_callback_t cb,
                                      void *user_data,
                                      rut_closure_destroy_callback_t destroy)
{
    rut_image_t *image = object;

    return rut_closure_list_add_FIXME(
        &image->preferred_size_cb_list, cb, user_data, destroy);
}

static void
rut_image_get_size(void *object, float *width, float *height)
{
    rut_image_t *image = object;

    *width = image->width;
    *height = image->height;
}

static void
_rut_image_init_type(void)
{
    static rut_paintable_vtable_t paintable_vtable = { _rut_image_paint };
    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child removed */
                                                       NULL, /* child addded */
                                                       NULL /* parent changed */
    };
    static rut_sizable_vtable_t sizable_vtable = {
        rut_image_set_size,                   rut_image_get_size,
        rut_image_get_preferred_width,        rut_image_get_preferred_height,
        rut_image_add_preferred_size_callback
    };

    rut_type_t *type = &rut_image_type;
#define TYPE rut_image_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rut_image_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_PAINTABLE,
                       offsetof(TYPE, paintable),
                       &paintable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_GRAPHABLE,
                       offsetof(TYPE, graphable),
                       &graphable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_SIZABLE,
                       0, /* no implied properties */
                       &sizable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INTROSPECTABLE,
                       offsetof(TYPE, introspectable),
                       NULL); /* no implied vtable */

#undef TYPE
}

rut_image_t *
rut_image_new(rut_shell_t *shell, cg_texture_t *texture)
{
    rut_image_t *image =
        rut_object_alloc0(rut_image_t, &rut_image_type, _rut_image_init_type);

    image->shell = shell;

    c_list_init(&image->preferred_size_cb_list);

    image->pipeline = cg_pipeline_new(shell->cg_device);
    cg_pipeline_set_layer_texture(image->pipeline,
                                  0, /* layer_num */
                                  texture);

    image->tex_width = cg_texture_get_width(texture);
    image->tex_height = cg_texture_get_height(texture);

    rut_paintable_init(image);
    rut_graphable_init(image);

    rut_introspectable_init(image, _rut_image_prop_specs, image->properties);

    rut_image_set_draw_mode(image, RUT_IMAGE_DRAW_MODE_SCALE_WITH_ASPECT_RATIO);

    rut_image_set_size(image, image->tex_width, image->tex_height);

    return image;
}

static void
preferred_size_changed(rut_image_t *image)
{
    rut_closure_list_invoke(&image->preferred_size_cb_list,
                            rut_sizeable_preferred_size_callback_t,
                            image);
}

void
rut_image_set_draw_mode(rut_image_t *image,
                        rut_image_draw_mode_t draw_mode)
{
    if (draw_mode != image->draw_mode) {
        cg_pipeline_wrap_mode_t wrap_mode;
        cg_pipeline_filter_t min_filter, mag_filter;

        if (draw_mode == RUT_IMAGE_DRAW_MODE_1_TO_1 ||
            image->draw_mode == RUT_IMAGE_DRAW_MODE_1_TO_1)
            preferred_size_changed(image);

        image->draw_mode = draw_mode;

        switch (draw_mode) {
        case RUT_IMAGE_DRAW_MODE_1_TO_1:
        case RUT_IMAGE_DRAW_MODE_REPEAT:
            wrap_mode = CG_PIPELINE_WRAP_MODE_REPEAT;
            min_filter = CG_PIPELINE_FILTER_NEAREST;
            mag_filter = CG_PIPELINE_FILTER_NEAREST;
            break;

        case RUT_IMAGE_DRAW_MODE_SCALE:
        case RUT_IMAGE_DRAW_MODE_SCALE_WITH_ASPECT_RATIO:
            wrap_mode = CG_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE;
            min_filter = CG_PIPELINE_FILTER_LINEAR_MIPMAP_NEAREST;
            mag_filter = CG_PIPELINE_FILTER_LINEAR;
            break;
        }

        cg_pipeline_set_layer_wrap_mode(image->pipeline,
                                        0, /* layer_num */
                                        wrap_mode);
        cg_pipeline_set_layer_filters(image->pipeline,
                                      0, /* layer_num */
                                      min_filter,
                                      mag_filter);

        rut_property_dirty(&image->shell->property_ctx,
                           &image->properties[RUT_IMAGE_PROP_DRAW_MODE]);
    }
}
