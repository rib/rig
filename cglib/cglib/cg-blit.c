/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
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
 * Authors:
 *  Neil Roberts   <neil@linux.intel.com>
 */

#include <cglib-config.h>

#include <string.h>

#include "cg-util.h"
#include "cg-blit.h"
#include "cg-device-private.h"
#include "cg-framebuffer-private.h"
#include "cg-texture-private.h"
#include "cg-texture-2d-private.h"
#include "cg-private.h"

static const cg_blit_mode_t *_cg_blit_default_mode = NULL;

static bool
_cg_blit_texture_render_begin(cg_blit_data_t *data)
{
    cg_device_t *dev = data->src_tex->dev;
    cg_offscreen_t *offscreen;
    cg_framebuffer_t *fb;
    cg_pipeline_t *pipeline;
    unsigned int dst_width, dst_height;
    cg_error_t *ignore_error = NULL;

    offscreen = _cg_offscreen_new_with_texture_full(
        data->dst_tex, CG_OFFSCREEN_DISABLE_AUTO_DEPTH_AND_STENCIL, 0 /* level */);

    fb = CG_FRAMEBUFFER(offscreen);
    if (!cg_framebuffer_allocate(fb, &ignore_error)) {
        cg_error_free(ignore_error);
        cg_object_unref(fb);
        return false;
    }

    data->dest_fb = fb;

    dst_width = cg_texture_get_width(data->dst_tex);
    dst_height = cg_texture_get_height(data->dst_tex);

    /* Set up an orthographic projection so we can use pixel
       coordinates to render to the texture */
    cg_framebuffer_orthographic(
        fb, 0, 0, dst_width, dst_height, -1 /* near */, 1 /* far */);

    /* We cache a pipeline used for migrating on to the context so
       that it doesn't have to continuously regenerate a shader
       program */
    if (dev->blit_texture_pipeline == NULL) {
        dev->blit_texture_pipeline = cg_pipeline_new(dev);

        cg_pipeline_set_layer_filters(dev->blit_texture_pipeline,
                                      0,
                                      CG_PIPELINE_FILTER_NEAREST,
                                      CG_PIPELINE_FILTER_NEAREST);

        /* Disable blending by just directly taking the contents of the
           source texture */
        cg_pipeline_set_blend(dev->blit_texture_pipeline,
                              "RGBA = ADD(SRC_COLOR, 0)", NULL);
    }

    pipeline = dev->blit_texture_pipeline;

    cg_pipeline_set_layer_texture(pipeline, 0, data->src_tex);

    data->pipeline = pipeline;

    return true;
}

static void
_cg_blit_texture_render_blit(cg_blit_data_t *data,
                             int src_x,
                             int src_y,
                             int dst_x,
                             int dst_y,
                             int width,
                             int height)
{
    cg_framebuffer_draw_textured_rectangle(
        data->dest_fb,
        data->pipeline,
        dst_x,
        dst_y,
        dst_x + width,
        dst_y + height,
        src_x / (float)data->src_width,
        src_y / (float)data->src_height,
        (src_x + width) / (float)data->src_width,
        (src_y + height) / (float)data->src_height);
}

static void
_cg_blit_texture_render_end(cg_blit_data_t *data)
{
    cg_device_t *dev = data->src_tex->dev;

    /* Attach the target texture to the texture render pipeline so that
       we don't keep a reference to the source texture forever. This is
       assuming that the destination texture will live for a long time
       which is currently the case when cg_blit_* is used from the
       atlas code. It may be better in future to keep around a set of
       dummy 1x1 textures for each texture target that we could bind
       instead. This would also be useful when using a pipeline as a
       hash table key such as for the GLSL program cache. */
    cg_pipeline_set_layer_texture(dev->blit_texture_pipeline, 0,
                                  data->dst_tex);

    cg_object_unref(data->dest_fb);
}

static bool
_cg_blit_framebuffer_begin(cg_blit_data_t *data)
{
    cg_device_t *dev = data->src_tex->dev;
    cg_offscreen_t *dst_offscreen = NULL, *src_offscreen = NULL;
    cg_framebuffer_t *dst_fb, *src_fb;
    cg_error_t *ignore_error = NULL;

    if (!_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_OFFSCREEN_BLIT))
        return false;

    dst_offscreen = _cg_offscreen_new_with_texture_full(
        data->dst_tex, CG_OFFSCREEN_DISABLE_AUTO_DEPTH_AND_STENCIL, 0 /* level */);

    dst_fb = CG_FRAMEBUFFER(dst_offscreen);
    if (!cg_framebuffer_allocate(dst_fb, &ignore_error)) {
        cg_error_free(ignore_error);
        goto error;
    }

    src_offscreen = _cg_offscreen_new_with_texture_full(
        data->src_tex, CG_OFFSCREEN_DISABLE_AUTO_DEPTH_AND_STENCIL, 0 /* level */);

    src_fb = CG_FRAMEBUFFER(src_offscreen);
    if (!cg_framebuffer_allocate(src_fb, &ignore_error)) {
        cg_error_free(ignore_error);
        goto error;
    }

    data->src_fb = src_fb;
    data->dest_fb = dst_fb;

    return true;

error:

    if (dst_offscreen)
        cg_object_unref(dst_offscreen);
    if (src_offscreen)
        cg_object_unref(src_offscreen);

    return false;
}

static void
_cg_blit_framebuffer_blit(cg_blit_data_t *data,
                          int src_x,
                          int src_y,
                          int dst_x,
                          int dst_y,
                          int width,
                          int height)
{
    _cg_blit_framebuffer(
        data->src_fb, data->dest_fb, src_x, src_y, dst_x, dst_y, width, height);
}

static void
_cg_blit_framebuffer_end(cg_blit_data_t *data)
{
    cg_object_unref(data->src_fb);
    cg_object_unref(data->dest_fb);
}

static bool
_cg_blit_copy_tex_sub_image_begin(cg_blit_data_t *data)
{
    cg_offscreen_t *offscreen;
    cg_framebuffer_t *fb;
    cg_error_t *ignore_error = NULL;

    /* This will only work if the target texture is a cg_texture_2d_t */
    if (!cg_is_texture_2d(data->dst_tex))
        return false;

    offscreen = _cg_offscreen_new_with_texture_full(
        data->src_tex, CG_OFFSCREEN_DISABLE_AUTO_DEPTH_AND_STENCIL, 0 /* level */);

    fb = CG_FRAMEBUFFER(offscreen);
    if (!cg_framebuffer_allocate(fb, &ignore_error)) {
        cg_error_free(ignore_error);
        cg_object_unref(fb);
        return false;
    }

    data->src_fb = fb;

    return true;
}

static void
_cg_blit_copy_tex_sub_image_blit(cg_blit_data_t *data,
                                 int src_x,
                                 int src_y,
                                 int dst_x,
                                 int dst_y,
                                 int width,
                                 int height)
{
    _cg_texture_2d_copy_from_framebuffer(CG_TEXTURE_2D(data->dst_tex),
                                         src_x,
                                         src_y,
                                         width,
                                         height,
                                         data->src_fb,
                                         dst_x,
                                         dst_y,
                                         0); /* level */
}

static void
_cg_blit_copy_tex_sub_image_end(cg_blit_data_t *data)
{
    cg_object_unref(data->src_fb);
}

static bool
_cg_blit_get_tex_data_begin(cg_blit_data_t *data)
{
    data->format = _cg_texture_get_format(data->src_tex);
    data->bpp = _cg_pixel_format_get_bytes_per_pixel(data->format);

    data->image_data = c_malloc(data->bpp * data->src_width * data->src_height);
    cg_texture_get_data(data->src_tex,
                        data->format,
                        data->src_width * data->bpp,
                        data->image_data);

    return true;
}

static void
_cg_blit_get_tex_data_blit(cg_blit_data_t *data,
                           int src_x,
                           int src_y,
                           int dst_x,
                           int dst_y,
                           int width,
                           int height)
{
    cg_error_t *ignore = NULL;
    int rowstride = data->src_width * data->bpp;
    int offset = rowstride * src_y + src_x * data->bpp;

    cg_texture_set_region(data->dst_tex,
                          width,
                          height,
                          data->format,
                          rowstride,
                          data->image_data + offset,
                          dst_x,
                          dst_y,
                          0, /* level */
                          &ignore);
    /* TODO: support chaining up errors during the blit */
}

static void
_cg_blit_get_tex_data_end(cg_blit_data_t *data)
{
    c_free(data->image_data);
}

/* These should be specified in order of preference */
static const cg_blit_mode_t _cg_blit_modes[] = {
    { "texture-render",             _cg_blit_texture_render_begin,
      _cg_blit_texture_render_blit, _cg_blit_texture_render_end },
    { "framebuffer",             _cg_blit_framebuffer_begin,
      _cg_blit_framebuffer_blit, _cg_blit_framebuffer_end },
    { "copy-tex-sub-image",             _cg_blit_copy_tex_sub_image_begin,
      _cg_blit_copy_tex_sub_image_blit, _cg_blit_copy_tex_sub_image_end },
    { "get-tex-data",             _cg_blit_get_tex_data_begin,
      _cg_blit_get_tex_data_blit, _cg_blit_get_tex_data_end }
};

void
_cg_blit_begin(cg_blit_data_t *data,
               cg_texture_t *dst_tex,
               cg_texture_t *src_tex)
{
    int i;

    if (_cg_blit_default_mode == NULL) {
        const char *default_mode_string;

        /* Allow the default to be specified with an environment
           variable. For the time being these functions are only used
           when blitting between atlas textures so the environment
           variable is named to be specific to the atlas code. If we
           want to use the code in other places we should create another
           environment variable for each specific use case */
        if ((default_mode_string = c_getenv("CG_ATLAS_DEFAULT_BLIT_MODE"))) {
            for (i = 0; i < C_N_ELEMENTS(_cg_blit_modes); i++)
                if (!strcmp(_cg_blit_modes[i].name, default_mode_string)) {
                    _cg_blit_default_mode = _cg_blit_modes + i;
                    break;
                }

            if (i >= C_N_ELEMENTS(_cg_blit_modes)) {
                c_warning("Unknown blit mode %s", default_mode_string);
                _cg_blit_default_mode = _cg_blit_modes;
            }
        } else
            /* Default to the first blit mode */
            _cg_blit_default_mode = _cg_blit_modes;
    }

    memset(data, 0, sizeof(cg_blit_data_t));

    data->dst_tex = dst_tex;
    data->src_tex = src_tex;

    data->src_width = cg_texture_get_width(src_tex);
    data->src_height = cg_texture_get_height(src_tex);

    /* Try the default blit mode first */
    if (!_cg_blit_default_mode->begin_func(data)) {
        CG_NOTE(ATLAS,
                "Failed to set up blit mode %s",
                _cg_blit_default_mode->name);

        /* Try all of the other modes in order */
        for (i = 0; i < C_N_ELEMENTS(_cg_blit_modes); i++)
            if (_cg_blit_modes + i != _cg_blit_default_mode &&
                _cg_blit_modes[i].begin_func(data)) {
                /* Use this mode as the default from now on */
                _cg_blit_default_mode = _cg_blit_modes + i;
                break;
            } else
                CG_NOTE(ATLAS,
                        "Failed to set up blit mode %s",
                        _cg_blit_modes[i].name);

        /* The last blit mode can't fail so this should never happen */
        c_return_if_fail(i < C_N_ELEMENTS(_cg_blit_modes));
    }

    data->blit_mode = _cg_blit_default_mode;

    CG_NOTE(ATLAS, "Setup blit using %s", _cg_blit_default_mode->name);
}

void
_cg_blit(cg_blit_data_t *data,
         int src_x,
         int src_y,
         int dst_x,
         int dst_y,
         int width,
         int height)
{
    data->blit_mode->blit_func(data, src_x, src_y, dst_x, dst_y, width, height);
}

void
_cg_blit_end(cg_blit_data_t *data)
{
    data->blit_mode->end_func(data);
}
