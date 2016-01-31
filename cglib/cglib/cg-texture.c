/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
 * Copyright (C) 2010 Red Hat, Inc.
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
 *
 *
 * Authors:
 *  Matthew Allum  <mallum@openedhand.com>
 *  Neil Roberts   <neil@linux.intel.com>
 *  Robert Bragg   <robert@linux.intel.com>
 */

#include <cglib-config.h>

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "cg-util.h"
#include "cg-bitmap.h"
#include "cg-bitmap-private.h"
#include "cg-buffer-private.h"
#include "cg-pixel-buffer-private.h"
#include "cg-private.h"
#include "cg-texture-private.h"
#include "cg-texture-driver.h"
#include "cg-texture-2d-sliced-private.h"
#include "cg-texture-2d-private.h"
#include "cg-texture-2d-gl.h"
#include "cg-texture-3d-private.h"
#include "cg-sub-texture-private.h"
#include "cg-atlas-texture-private.h"
#include "cg-pipeline.h"
#include "cg-device-private.h"
#include "cg-object-private.h"
#include "cg-object-private.h"
#include "cg-framebuffer-private.h"
#include "cg-sub-texture.h"
#include "cg-primitive-texture.h"
#include "cg-error-private.h"
#include "cg-pixel-format-private.h"

/* This isn't defined in the GLES headers */
#ifndef GL_RED
#define GL_RED 0x1903
#endif

uint32_t
cg_texture_error_domain(void)
{
    return c_quark_from_static_string("cg-texture-error-quark");
}

/* XXX:
 * The cg_object_t macros don't support any form of inheritance, so for
 * now we implement the cg_object_t support for the cg_texture_t
 * abstract class manually.
 */

static c_sllist_t *_cg_texture_types;

void
_cg_texture_register_texture_type(const cg_object_class_t *klass)
{
    _cg_texture_types = c_sllist_prepend(_cg_texture_types, (void *)klass);
}

bool
cg_is_texture(void *object)
{
    cg_object_t *obj = (cg_object_t *)object;
    c_sllist_t *l;

    if (object == NULL)
        return false;

    for (l = _cg_texture_types; l; l = l->next)
        if (l->data == obj->klass)
            return true;

    return false;
}

void
_cg_texture_init(cg_texture_t *texture,
                 cg_device_t *dev,
                 int width,
                 int height,
                 cg_pixel_format_t src_format,
                 cg_texture_loader_t *loader,
                 const cg_texture_vtable_t *vtable)
{
    texture->dev = dev;
    texture->max_level = 0;
    texture->width = width;
    texture->height = height;
    texture->allocated = false;
    texture->vtable = vtable;
    texture->framebuffers = NULL;

    texture->loader = loader;

    _cg_texture_set_internal_format(texture, src_format);

    /* Although we want to initialize texture::components according
     * to the source format, we always want the internal layout to
     * be considered premultiplied by default.
     *
     * NB: this ->premultiplied state is user configurable so to avoid
     * awkward documentation, setting this to 'true' does not depend on
     * ->components having an alpha component (we will simply ignore the
     * premultiplied status later if there is no alpha component).
     * This way we don't have to worry about updating the
     * ->premultiplied state in _set_components().  Similarly we don't
     * have to worry about updating the ->components state in
     * _set_premultiplied().
     */
    texture->premultiplied = true;
}

static void
_cg_texture_free_loader(cg_texture_t *texture)
{
    if (texture->loader) {
        cg_texture_loader_t *loader = texture->loader;
        switch (loader->src_type) {
        case CG_TEXTURE_SOURCE_TYPE_SIZED:
        case CG_TEXTURE_SOURCE_TYPE_EGL_IMAGE:
        case CG_TEXTURE_SOURCE_TYPE_GL_FOREIGN:
            break;
        case CG_TEXTURE_SOURCE_TYPE_BITMAP:
            cg_object_unref(loader->src.bitmap.bitmap);
            break;
        case CG_TEXTURE_SOURCE_TYPE_WEBGL_IMAGE:
#ifdef CG_HAS_WEBGL_SUPPORT
            cg_object_unref(loader->src.webgl_image.image);
#else
            c_assert_not_reached();
#endif
            break;
        }
        c_slice_free(cg_texture_loader_t, loader);
        texture->loader = NULL;
    }
}

cg_texture_loader_t *
_cg_texture_create_loader(cg_device_t *dev)
{
    /* lazily assert that device has been connected */
    cg_device_connect(dev, NULL);
    return c_slice_new0(cg_texture_loader_t);
}

void
_cg_texture_free(cg_texture_t *texture)
{
    _cg_texture_free_loader(texture);

    c_free(texture);
}

bool
_cg_texture_needs_premult_conversion(cg_pixel_format_t src_format,
                                     cg_pixel_format_t dst_format)
{
    return (src_format != CG_PIXEL_FORMAT_A_8 &&
            dst_format != CG_PIXEL_FORMAT_A_8 &&
            _cg_pixel_format_has_alpha(src_format) &&
            _cg_pixel_format_has_alpha(dst_format) &&
            (_cg_pixel_format_is_premultiplied(src_format) !=
             _cg_pixel_format_is_premultiplied(dst_format)));
}

bool
_cg_texture_is_foreign(cg_texture_t *texture)
{
    if (texture->vtable->is_foreign)
        return texture->vtable->is_foreign(texture);
    else
        return false;
}

int
cg_texture_get_width(cg_texture_t *texture)
{
    return texture->width;
}

int
cg_texture_get_height(cg_texture_t *texture)
{
    return texture->height;
}

cg_pixel_format_t
_cg_texture_get_format(cg_texture_t *texture)
{
    if (!texture->allocated)
        cg_texture_allocate(texture, NULL);
    return texture->vtable->get_format(texture);
}

int
_cg_texture_get_n_levels(cg_texture_t *texture)
{
    int width = cg_texture_get_width(texture);
    int height = cg_texture_get_height(texture);
    int max_dimension = MAX(width, height);

    if (cg_is_texture_3d(texture)) {
        cg_texture_3d_t *tex_3d = CG_TEXTURE_3D(texture);
        max_dimension = MAX(max_dimension, tex_3d->depth);
    }

    return _cg_util_fls(max_dimension);
}

void
_cg_texture_get_level_size(
    cg_texture_t *texture, int level, int *width, int *height, int *depth)
{
    int current_width = cg_texture_get_width(texture);
    int current_height = cg_texture_get_height(texture);
    int current_depth;
    int i;

    if (cg_is_texture_3d(texture)) {
        cg_texture_3d_t *tex_3d = CG_TEXTURE_3D(texture);
        current_depth = tex_3d->depth;
    } else
        current_depth = 0;

    /* NB: The OpenGL spec (like D3D) uses a floor() convention to
     * round down the size of a mipmap level when dividing the size
     * of the previous level results in a fraction...
     */
    for (i = 0; i < level; i++) {
        current_width = MAX(1, current_width >> 1);
        current_height = MAX(1, current_height >> 1);
        current_depth = MAX(1, current_depth >> 1);
    }

    if (width)
        *width = current_width;
    if (height)
        *height = current_height;
    if (depth)
        *depth = current_depth;
}

bool
cg_texture_is_sliced(cg_texture_t *texture)
{
    if (!texture->allocated)
        cg_texture_allocate(texture, NULL);

    if (texture->vtable->is_sliced)
        return texture->vtable->is_sliced(texture);
    else
        return false;
}

/* If this returns false, that implies _foreach_sub_texture_in_region
 * will be needed to iterate over multiple sub textures for regions whos
 * texture coordinates extend out of the range [0,1]
 */
bool
_cg_texture_can_hardware_repeat(cg_texture_t *texture)
{
    if (!texture->allocated)
        cg_texture_allocate(texture, NULL);
    if (texture->vtable->can_hardware_repeat)
        return texture->vtable->can_hardware_repeat(texture);
    else
        return true;
}

bool
cg_texture_get_gl_texture(cg_texture_t *texture,
                          GLuint *out_gl_handle,
                          GLenum *out_gl_target)
{
    if (!texture->allocated)
        cg_texture_allocate(texture, NULL);

    return texture->vtable->get_gl_texture(
        texture, out_gl_handle, out_gl_target);
}

cg_texture_type_t
_cg_texture_get_type(cg_texture_t *texture)
{
    return texture->vtable->get_type(texture);
}

void
_cg_texture_pre_paint(cg_texture_t *texture,
                      cg_texture_pre_paint_flags_t flags)
{
    /* Assert that the storage for the texture exists already if we're
     * about to reference it for painting.
     *
     * Note: we abort on error here since it's a bit late to do anything
     * about it if we fail to allocate the texture and the app could
     * have explicitly allocated the texture earlier to handle problems
     * gracefully.
     *
     * XXX: Maybe it could even be considered a programmer error if the
     * texture hasn't been allocated by this point since it implies we
     * are abount to paint with undefined texture contents?
     */
    cg_texture_allocate(texture, NULL);

    texture->vtable->pre_paint(texture, flags);
}

bool
cg_texture_set_region_from_bitmap(cg_texture_t *texture,
                                  int src_x,
                                  int src_y,
                                  int width,
                                  int height,
                                  cg_bitmap_t *bmp,
                                  int dst_x,
                                  int dst_y,
                                  int level,
                                  cg_error_t **error)
{
    c_return_val_if_fail((cg_bitmap_get_width(bmp) - src_x) >= width, false);
    c_return_val_if_fail((cg_bitmap_get_height(bmp) - src_y) >= height,
                           false);
    c_return_val_if_fail(width > 0, false);
    c_return_val_if_fail(height > 0, false);

    /* Assert that the storage for this texture has been allocated */
    if (!cg_texture_allocate(texture, error))
        return false;

    /* Note that we don't prepare the bitmap for upload here because
       some backends may be internally using a different format for the
       actual GL texture than that reported by
       _cg_texture_get_format. For example the atlas textures are
       always stored in an RGBA texture even if the texture format is
       advertised as RGB. */

    return texture->vtable->set_region(
        texture, src_x, src_y, dst_x, dst_y, width, height, level, bmp, error);
}

bool
cg_texture_set_region(cg_texture_t *texture,
                      int width,
                      int height,
                      cg_pixel_format_t format,
                      int rowstride,
                      const uint8_t *data,
                      int dst_x,
                      int dst_y,
                      int level,
                      cg_error_t **error)
{
    cg_device_t *dev = texture->dev;
    cg_bitmap_t *source_bmp;
    bool ret;

    c_return_val_if_fail(format != CG_PIXEL_FORMAT_ANY, false);

    /* Rowstride from width if none specified */
    if (rowstride == 0)
        rowstride = _cg_pixel_format_get_bytes_per_pixel(format) * width;

    /* Init source bitmap */
    source_bmp = cg_bitmap_new_for_data(dev, width, height, format,
                                        rowstride, (uint8_t *)data);

    ret = cg_texture_set_region_from_bitmap(
        texture, 0, 0, width, height, source_bmp, dst_x, dst_y, level, error);

    cg_object_unref(source_bmp);

    return ret;
}

bool
cg_texture_set_data(cg_texture_t *texture,
                    cg_pixel_format_t format,
                    int rowstride,
                    const uint8_t *data,
                    int level,
                    cg_error_t **error)
{
    int level_width;
    int level_height;

    _cg_texture_get_level_size(
        texture, level, &level_width, &level_height, NULL);

    return cg_texture_set_region(texture,
                                 level_width,
                                 level_height,
                                 format,
                                 rowstride,
                                 data,
                                 0,
                                 0, /* dest x, y */
                                 level,
                                 error);
}

/* Reads back the contents of a texture by rendering it to the framebuffer
 * and reading back the resulting pixels.
 *
 * It will perform multiple renders if the texture is larger than the
 * current glViewport.
 *
 * It assumes the projection and modelview have already been setup so
 * that rendering to 0,0 with the same width and height of the viewport
 * will exactly cover the viewport.
 *
 * NB: Normally this approach isn't normally used since we can just use
 * glGetTexImage, but may be used as a fallback in some circumstances.
 */
static bool
do_texture_draw_and_read(cg_framebuffer_t *fb,
                         cg_pipeline_t *pipeline,
                         cg_texture_t *texture,
                         cg_bitmap_t *target_bmp,
                         float *viewport,
                         cg_error_t **error)
{
    float rx1, ry1;
    float rx2, ry2;
    float tx1, ty1;
    float tx2, ty2;
    int bw, bh;
    cg_bitmap_t *rect_bmp;
    unsigned int tex_width, tex_height;
    cg_device_t *dev = fb->dev;

    tex_width = cg_texture_get_width(texture);
    tex_height = cg_texture_get_height(texture);

    ry2 = 0;
    ty2 = 0;

    /* Walk Y axis until whole bitmap height consumed */
    for (bh = tex_height; bh > 0; bh -= viewport[3]) {
        /* Rectangle Y coords */
        ry1 = ry2;
        ry2 += (bh < viewport[3]) ? bh : viewport[3];

        /* Normalized texture Y coords */
        ty1 = ty2;
        ty2 = (ry2 / (float)tex_height);

        rx2 = 0;
        tx2 = 0;

        /* Walk X axis until whole bitmap width consumed */
        for (bw = tex_width; bw > 0; bw -= viewport[2]) {
            int width;
            int height;

            /* Rectangle X coords */
            rx1 = rx2;
            rx2 += (bw < viewport[2]) ? bw : viewport[2];

            width = rx2 - rx1;
            height = ry2 - ry1;

            /* Normalized texture X coords */
            tx1 = tx2;
            tx2 = (rx2 / (float)tex_width);

            /* Draw a portion of texture */
            cg_framebuffer_draw_textured_rectangle(
                fb, pipeline, 0, 0, rx2 - rx1, ry2 - ry1, tx1, ty1, tx2, ty2);

            /* Read into a temporary bitmap */
            rect_bmp = _cg_bitmap_new_with_malloc_buffer(dev, width, height,
                                                         CG_PIXEL_FORMAT_RGBA_8888_PRE,
                                                         error);
            if (!rect_bmp)
                return false;

            if (!cg_framebuffer_read_pixels_into_bitmap(
                    fb,
                    viewport[0],
                    viewport[1],
                    CG_READ_PIXELS_COLOR_BUFFER,
                    rect_bmp,
                    error)) {
                cg_object_unref(rect_bmp);
                return false;
            }

            /* Copy to target bitmap */
            if (!_cg_bitmap_copy_subregion(rect_bmp,
                                           target_bmp,
                                           0,
                                           0,
                                           rx1,
                                           ry1,
                                           width,
                                           height,
                                           error)) {
                cg_object_unref(rect_bmp);
                return false;
            }

            /* Free temp bitmap */
            cg_object_unref(rect_bmp);
        }
    }

    return true;
}

bool
cg_texture_draw_and_read_to_bitmap(cg_texture_t *texture,
                                   cg_framebuffer_t *framebuffer,
                                   cg_bitmap_t *target_bmp,
                                   cg_error_t **error)
{
    cg_device_t *dev = framebuffer->dev;
    float save_viewport[4];
    float viewport[4];
    bool status = false;

    viewport[0] = 0;
    viewport[1] = 0;
    viewport[2] = cg_framebuffer_get_width(framebuffer);
    viewport[3] = cg_framebuffer_get_height(framebuffer);

    cg_framebuffer_get_viewport4fv(framebuffer, save_viewport);
    _cg_framebuffer_push_projection(framebuffer);
    cg_framebuffer_orthographic(framebuffer, 0, 0, viewport[2], viewport[3], 0, 100);

    cg_framebuffer_push_matrix(framebuffer);
    cg_framebuffer_identity_matrix(framebuffer);

    /* Direct copy operation */

    if (dev->texture_download_pipeline == NULL) {
        cg_snippet_t *snippet;

        dev->texture_download_pipeline = cg_pipeline_new(dev);
        cg_pipeline_set_blend(dev->texture_download_pipeline,
                              "RGBA = ADD (SRC_COLOR, 0)", NULL);

        snippet = cg_snippet_new(CG_SNIPPET_HOOK_LAYER_FRAGMENT, NULL, NULL);
        cg_snippet_set_replace(snippet, "frag = texture2D(cg_sampler0, cg_tex_coord0_in.st)");
        cg_pipeline_add_layer_snippet(dev->texture_download_pipeline, 0, snippet);
        cg_object_unref(snippet);

        cg_pipeline_set_layer_filters(dev->texture_download_pipeline,
                                      0,
                                      CG_PIPELINE_FILTER_NEAREST,
                                      CG_PIPELINE_FILTER_NEAREST);
    }

    cg_pipeline_set_layer_texture(dev->texture_download_pipeline, 0, texture);

    if (!do_texture_draw_and_read(framebuffer,
                                  dev->texture_download_pipeline,
                                  texture,
                                  target_bmp,
                                  viewport,
                                  error))
        return false;

    status = true;

    /* Restore old state */
    cg_framebuffer_pop_matrix(framebuffer);
    _cg_framebuffer_pop_projection(framebuffer);
    cg_framebuffer_set_viewport(framebuffer,
                                save_viewport[0],
                                save_viewport[1],
                                save_viewport[2],
                                save_viewport[3]);

    return status;
}

static bool
get_texture_bits_via_offscreen(cg_texture_t *meta_texture,
                               cg_texture_t *sub_texture,
                               int x,
                               int y,
                               int width,
                               int height,
                               uint8_t *dst_bits,
                               unsigned int dst_rowstride,
                               cg_pixel_format_t closest_format)
{
    cg_device_t *dev = sub_texture->dev;
    cg_offscreen_t *offscreen;
    cg_framebuffer_t *framebuffer;
    cg_bitmap_t *bitmap;
    bool ret;
    cg_error_t *ignore_error = NULL;
    cg_pixel_format_t real_format;

    offscreen = _cg_offscreen_new_with_texture_full(
        sub_texture, CG_OFFSCREEN_DISABLE_AUTO_DEPTH_AND_STENCIL, 0);

    framebuffer = CG_FRAMEBUFFER(offscreen);
    if (!cg_framebuffer_allocate(framebuffer, &ignore_error)) {
        cg_error_free(ignore_error);
        return false;
    }

    /* Currently the framebuffer's internal format corresponds to the
     * internal format of @sub_texture but in the case of atlas textures
     * it's possible that this format doesn't reflect the correct
     * premultiplied alpha status or what components are valid since
     * atlas textures are always stored in a shared texture with a
     * format of _RGBA_8888.
     *
     * Here we override the internal format to make sure the
     * framebuffer's internal format matches the internal format of the
     * parent meta_texture instead.
     */
    real_format = _cg_texture_get_format(meta_texture);
    _cg_framebuffer_set_internal_format(framebuffer, real_format);

    bitmap = cg_bitmap_new_for_data(dev, width, height, closest_format,
                                    dst_rowstride, dst_bits);
    ret = cg_framebuffer_read_pixels_into_bitmap(
        framebuffer, x, y, CG_READ_PIXELS_COLOR_BUFFER, bitmap, &ignore_error);

    if (!ret)
        cg_error_free(ignore_error);

    cg_object_unref(bitmap);

    cg_object_unref(framebuffer);

    return ret;
}

static bool
get_texture_bits_via_copy(cg_texture_t *texture,
                          int x,
                          int y,
                          int width,
                          int height,
                          uint8_t *dst_bits,
                          unsigned int dst_rowstride,
                          cg_pixel_format_t dst_format)
{
    unsigned int full_rowstride;
    uint8_t *full_bits;
    bool ret = true;
    int bpp;
    int full_tex_width, full_tex_height;

    full_tex_width = cg_texture_get_width(texture);
    full_tex_height = cg_texture_get_height(texture);

    bpp = _cg_pixel_format_get_bytes_per_pixel(dst_format);

    full_rowstride = bpp * full_tex_width;
    full_bits = c_malloc(full_rowstride * full_tex_height);

    if (texture->vtable->get_data(
            texture, dst_format, full_rowstride, full_bits)) {
        uint8_t *dst = dst_bits;
        uint8_t *src = full_bits + x * bpp + y * full_rowstride;
        int i;

        for (i = 0; i < height; i++) {
            memcpy(dst, src, bpp * width);
            dst += dst_rowstride;
            src += full_rowstride;
        }
    } else
        ret = false;

    c_free(full_bits);

    return ret;
}

typedef struct {
    cg_texture_t *meta_texture;
    int orig_width;
    int orig_height;
    cg_bitmap_t *target_bmp;
    uint8_t *target_bits;
    bool success;
    cg_error_t *error;
} cg_texture_get_data_t;

static void
texture_get_cb(cg_texture_t *subtexture,
               const float *subtexture_coords,
               const float *virtual_coords,
               void *user_data)
{
    cg_texture_get_data_t *tg_data = user_data;
    cg_texture_t *meta_texture = tg_data->meta_texture;
    cg_pixel_format_t closest_format =
        cg_bitmap_get_format(tg_data->target_bmp);
    int bpp = _cg_pixel_format_get_bytes_per_pixel(closest_format);
    unsigned int rowstride = cg_bitmap_get_rowstride(tg_data->target_bmp);
    int subtexture_width = cg_texture_get_width(subtexture);
    int subtexture_height = cg_texture_get_height(subtexture);

    int x_in_subtexture = (int)(0.5 + subtexture_width * subtexture_coords[0]);
    int y_in_subtexture = (int)(0.5 + subtexture_height * subtexture_coords[1]);
    int width = ((int)(0.5 + subtexture_width * subtexture_coords[2]) -
                 x_in_subtexture);
    int height = ((int)(0.5 + subtexture_height * subtexture_coords[3]) -
                  y_in_subtexture);
    int x_in_bitmap = (int)(0.5 + tg_data->orig_width * virtual_coords[0]);
    int y_in_bitmap = (int)(0.5 + tg_data->orig_height * virtual_coords[1]);

    uint8_t *dst_bits;

    if (!tg_data->success)
        return;

    dst_bits =
        tg_data->target_bits + x_in_bitmap * bpp + y_in_bitmap * rowstride;

    /* If we can read everything as a single slice, then go ahead and do that
     * to avoid allocating an FBO. We'll leave it up to the GL implementation to
     * do glGetTexImage as efficiently as possible. (GLES doesn't have that,
     * so we'll fall through)
     */
    if (x_in_subtexture == 0 && y_in_subtexture == 0 &&
        width == subtexture_width && height == subtexture_height) {
        if (subtexture->vtable->get_data(
                subtexture, closest_format, rowstride, dst_bits))
            return;
    }

    /* Next best option is a FBO and glReadPixels */
    if (get_texture_bits_via_offscreen(meta_texture,
                                       subtexture,
                                       x_in_subtexture,
                                       y_in_subtexture,
                                       width,
                                       height,
                                       dst_bits,
                                       rowstride,
                                       closest_format))
        return;

    /* Getting ugly: read the entire texture, copy out the part we want */
    if (get_texture_bits_via_copy(subtexture,
                                  x_in_subtexture,
                                  y_in_subtexture,
                                  width,
                                  height,
                                  dst_bits,
                                  rowstride,
                                  closest_format))
        return;

    /* No luck, the caller will fall back to the draw-to-backbuffer and
     * read implementation */
    tg_data->success = false;
}

int
cg_texture_get_data(cg_texture_t *texture,
                    cg_pixel_format_t format,
                    unsigned int rowstride,
                    uint8_t *data)
{
    cg_device_t *dev = texture->dev;
    int bpp;
    int byte_size;
    cg_pixel_format_t closest_format;
    GLenum closest_gl_format;
    GLenum closest_gl_type;
    cg_bitmap_t *target_bmp;
    int tex_width;
    int tex_height;
    cg_pixel_format_t texture_format;
    cg_error_t *ignore_error = NULL;

    cg_texture_get_data_t tg_data;

    texture_format = _cg_texture_get_format(texture);

    /* Default to internal format if none specified */
    if (format == CG_PIXEL_FORMAT_ANY)
        format = texture_format;

    tex_width = cg_texture_get_width(texture);
    tex_height = cg_texture_get_height(texture);

    /* Rowstride from texture width if none specified */
    bpp = _cg_pixel_format_get_bytes_per_pixel(format);
    if (rowstride == 0)
        rowstride = tex_width * bpp;

    /* Return byte size if only that requested */
    byte_size = tex_height * rowstride;
    if (data == NULL)
        return byte_size;

    closest_format = dev->texture_driver->find_best_gl_get_data_format(dev,
                                                                       format,
                                                                       &closest_gl_format,
                                                                       &closest_gl_type);

    /* We can assume that whatever data GL gives us will have the
       premult status of the original texture */
    if (_cg_pixel_format_can_be_premultiplied(closest_format)) {
        closest_format = _cg_pixel_format_premult_stem(closest_format);

        if (_cg_pixel_format_is_premultiplied(texture_format))
            closest_format = _cg_pixel_format_premultiply(closest_format);
    }

    /* If the application is requesting a conversion from a
     * component-alpha texture and the driver doesn't support them
     * natively then we can only read into an alpha-format buffer. In
     * this case the driver will be faking the alpha textures with a
     * red-component texture and it won't swizzle to the correct format
     * while reading */
    if (!_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_ALPHA_TEXTURES)) {
        if (texture_format == CG_PIXEL_FORMAT_A_8) {
            closest_format = CG_PIXEL_FORMAT_A_8;
            closest_gl_format = GL_RED;
            closest_gl_type = GL_UNSIGNED_BYTE;
        } else if (format == CG_PIXEL_FORMAT_A_8) {
            /* If we are converting to a component-alpha texture then we
             * need to read all of the components to a temporary buffer
             * because there is no way to get just the 4th component.
             * Note: it doesn't matter whether the texture is
             * pre-multiplied here because we're only going to look at
             * the alpha component */
            closest_format = CG_PIXEL_FORMAT_RGBA_8888;
            closest_gl_format = GL_RGBA;
            closest_gl_type = GL_UNSIGNED_BYTE;
        }
    }

    /* Is the requested format supported? */
    if (closest_format == format)
        /* Target user data directly */
        target_bmp = cg_bitmap_new_for_data(dev, tex_width, tex_height,
                                            format, rowstride, data);
    else {
        target_bmp = _cg_bitmap_new_with_malloc_buffer(dev, tex_width,
                                                       tex_height,
                                                       closest_format,
                                                       &ignore_error);
        if (!target_bmp) {
            cg_error_free(ignore_error);
            return 0;
        }
    }

    tg_data.target_bits = _cg_bitmap_map(target_bmp,
                                         CG_BUFFER_ACCESS_WRITE,
                                         CG_BUFFER_MAP_HINT_DISCARD,
                                         &ignore_error);
    if (tg_data.target_bits) {
        tg_data.meta_texture = texture;
        tg_data.orig_width = tex_width;
        tg_data.orig_height = tex_height;
        tg_data.target_bmp = target_bmp;
        tg_data.error = NULL;
        tg_data.success = true;

        /* If there are any dependent framebuffers on the texture then
         * we need to flush them so the texture contents will be
         * up-to-date */
        _cg_texture_flush_batched_rendering(texture);

        /* Iterating through the subtextures allows piecing together
         * the data for a sliced texture, and allows us to do the
         * read-from-framebuffer logic here in a simple fashion rather than
         * passing offsets down through the code. */
        cg_meta_texture_foreach_in_region(CG_META_TEXTURE(texture),
                                          0,
                                          0,
                                          1,
                                          1,
                                          CG_PIPELINE_WRAP_MODE_REPEAT,
                                          CG_PIPELINE_WRAP_MODE_REPEAT,
                                          texture_get_cb,
                                          &tg_data);

        _cg_bitmap_unmap(target_bmp);
    } else {
        cg_error_free(ignore_error);
        tg_data.success = false;
    }

    /* XXX: In some cases this api may fail to read back the texture
     * data; such as for GLES which doesn't support glGetTexImage
     */
    if (!tg_data.success) {
        cg_object_unref(target_bmp);
        return 0;
    }

    /* Was intermediate used? */
    if (closest_format != format) {
        cg_bitmap_t *new_bmp;
        bool result;
        cg_error_t *error = NULL;

        /* Convert to requested format directly into the user's buffer */
        new_bmp = cg_bitmap_new_for_data(dev, tex_width, tex_height, format,
                                         rowstride, data);
        result = _cg_bitmap_convert_into_bitmap(target_bmp, new_bmp, &error);

        if (!result) {
            cg_error_free(error);
            /* Return failure after cleaning up */
            byte_size = 0;
        }

        cg_object_unref(new_bmp);
    }

    cg_object_unref(target_bmp);

    return byte_size;
}

static void
_cg_texture_framebuffer_destroy_cb(void *user_data, void *instance)
{
    cg_texture_t *tex = user_data;
    cg_framebuffer_t *framebuffer = instance;

    tex->framebuffers = c_llist_remove(tex->framebuffers, framebuffer);
}

void
_cg_texture_associate_framebuffer(cg_texture_t *texture,
                                  cg_framebuffer_t *framebuffer)
{
    static cg_user_data_key_t framebuffer_destroy_notify_key;

    /* Note: we don't take a reference on the framebuffer here because
     * that would introduce a circular reference. */
    texture->framebuffers = c_llist_prepend(texture->framebuffers, framebuffer);

    /* Since we haven't taken a reference on the framebuffer we setup
     * some private data so we will be notified if it is destroyed... */
    _cg_object_set_user_data(CG_OBJECT(framebuffer),
                             &framebuffer_destroy_notify_key,
                             texture,
                             _cg_texture_framebuffer_destroy_cb);
}

const c_llist_t *
_cg_texture_get_associated_framebuffers(cg_texture_t *texture)
{
    return texture->framebuffers;
}

void
_cg_texture_flush_batched_rendering(cg_texture_t *texture)
{
    c_llist_t *l;

    for (l = texture->framebuffers; l; l = l->next)
        _cg_framebuffer_flush(l->data);
}

/* This function lets you define a meta texture as a grid of textures
 * whereby the x and y grid-lines are defined by an array of
 * cg_span_ts. With that grid based description this function can then
 * iterate all the cells of the grid that lye within a region
 * specified as virtual, meta-texture, coordinates.  This function can
 * also cope with regions that extend beyond the original meta-texture
 * grid by iterating cells repeatedly according to the wrap_x/y
 * arguments.
 *
 * To differentiate between texture coordinates of a specific, real,
 * slice texture and the texture coordinates of a composite, meta
 * texture, the coordinates of the meta texture are called "virtual"
 * coordinates and the coordinates of spans are called "slice"
 * coordinates.
 *
 * Note: no guarantee is given about the order in which the slices
 * will be visited.
 *
 * Note: The slice coordinates passed to @callback are always
 * normalized coordinates even if the span coordinates aren't
 * normalized.
 */
void
_cg_texture_spans_foreach_in_region(cg_span_t *x_spans,
                                    int n_x_spans,
                                    cg_span_t *y_spans,
                                    int n_y_spans,
                                    cg_texture_t **textures,
                                    float *virtual_coords,
                                    float x_normalize_factor,
                                    float y_normalize_factor,
                                    cg_pipeline_wrap_mode_t wrap_x,
                                    cg_pipeline_wrap_mode_t wrap_y,
                                    cg_meta_texture_callback_t callback,
                                    void *user_data)
{
    cg_span_iter_t iter_x;
    cg_span_iter_t iter_y;
    float slice_coords[4];
    float span_virtual_coords[4];

    /* Iterate the y axis of the virtual rectangle */
    for (_cg_span_iter_begin(&iter_y,
                             y_spans,
                             n_y_spans,
                             y_normalize_factor,
                             virtual_coords[1],
                             virtual_coords[3],
                             wrap_y);
         !_cg_span_iter_end(&iter_y);
         _cg_span_iter_next(&iter_y)) {
        if (iter_y.flipped) {
            slice_coords[1] = iter_y.intersect_end;
            slice_coords[3] = iter_y.intersect_start;
            span_virtual_coords[1] = iter_y.intersect_end;
            span_virtual_coords[3] = iter_y.intersect_start;
        } else {
            slice_coords[1] = iter_y.intersect_start;
            slice_coords[3] = iter_y.intersect_end;
            span_virtual_coords[1] = iter_y.intersect_start;
            span_virtual_coords[3] = iter_y.intersect_end;
        }

        /* Map the current intersection to normalized slice coordinates */
        slice_coords[1] = (slice_coords[1] - iter_y.pos) / iter_y.span->size;
        slice_coords[3] = (slice_coords[3] - iter_y.pos) / iter_y.span->size;

        /* Iterate the x axis of the virtual rectangle */
        for (_cg_span_iter_begin(&iter_x,
                                 x_spans,
                                 n_x_spans,
                                 x_normalize_factor,
                                 virtual_coords[0],
                                 virtual_coords[2],
                                 wrap_x);
             !_cg_span_iter_end(&iter_x);
             _cg_span_iter_next(&iter_x)) {
            cg_texture_t *span_tex;

            if (iter_x.flipped) {
                slice_coords[0] = iter_x.intersect_end;
                slice_coords[2] = iter_x.intersect_start;
                span_virtual_coords[0] = iter_x.intersect_end;
                span_virtual_coords[2] = iter_x.intersect_start;
            } else {
                slice_coords[0] = iter_x.intersect_start;
                slice_coords[2] = iter_x.intersect_end;
                span_virtual_coords[0] = iter_x.intersect_start;
                span_virtual_coords[2] = iter_x.intersect_end;
            }

            /* Map the current intersection to normalized slice coordinates */
            slice_coords[0] =
                (slice_coords[0] - iter_x.pos) / iter_x.span->size;
            slice_coords[2] =
                (slice_coords[2] - iter_x.pos) / iter_x.span->size;

            /* Pluck out the cg texture for this span */
            span_tex = textures[iter_y.index * n_x_spans + iter_x.index];

            callback(CG_TEXTURE(span_tex),
                     slice_coords,
                     span_virtual_coords,
                     user_data);
        }
    }
}

void
_cg_texture_set_allocated(cg_texture_t *texture,
                          cg_pixel_format_t internal_format,
                          int width,
                          int height)
{
    _cg_texture_set_internal_format(texture, internal_format);

    texture->width = width;
    texture->height = height;
    texture->allocated = true;

    _cg_texture_free_loader(texture);
}

bool
cg_texture_allocate(cg_texture_t *texture, cg_error_t **error)
{
    if (texture->allocated)
        return true;

    if (texture->components == CG_TEXTURE_COMPONENTS_RG &&
        !cg_has_feature(texture->dev, CG_FEATURE_ID_TEXTURE_RG))
        _cg_set_error(error,
                      CG_TEXTURE_ERROR,
                      CG_TEXTURE_ERROR_FORMAT,
                      "A red-green texture was requested but the driver "
                      "does not support them");

    texture->allocated = texture->vtable->allocate(texture, error);

    return texture->allocated;
}

void
_cg_texture_set_internal_format(cg_texture_t *texture,
                                cg_pixel_format_t internal_format)
{
    texture->premultiplied = false;

    if (internal_format == CG_PIXEL_FORMAT_ANY)
        internal_format = CG_PIXEL_FORMAT_RGBA_8888_PRE;

    switch (internal_format) {
    case CG_PIXEL_FORMAT_A_8:
    case CG_PIXEL_FORMAT_A_8SN:
    case CG_PIXEL_FORMAT_A_16U:
    case CG_PIXEL_FORMAT_A_16F:
    case CG_PIXEL_FORMAT_A_32U:
    case CG_PIXEL_FORMAT_A_32F:
        texture->components = CG_TEXTURE_COMPONENTS_A;
        break;

    case CG_PIXEL_FORMAT_RG_88:
    case CG_PIXEL_FORMAT_RG_88SN:
    case CG_PIXEL_FORMAT_RG_1616U:
    case CG_PIXEL_FORMAT_RG_1616F:
    case CG_PIXEL_FORMAT_RG_3232U:
    case CG_PIXEL_FORMAT_RG_3232F:
        texture->components = CG_TEXTURE_COMPONENTS_RG;
        break;

    case CG_PIXEL_FORMAT_RGB_565:
    case CG_PIXEL_FORMAT_RGB_888:
    case CG_PIXEL_FORMAT_BGR_888:
    case CG_PIXEL_FORMAT_RGB_888SN:
    case CG_PIXEL_FORMAT_BGR_888SN:
    case CG_PIXEL_FORMAT_RGB_161616U:
    case CG_PIXEL_FORMAT_BGR_161616U:
    case CG_PIXEL_FORMAT_RGB_161616F:
    case CG_PIXEL_FORMAT_BGR_161616F:
    case CG_PIXEL_FORMAT_RGB_323232U:
    case CG_PIXEL_FORMAT_BGR_323232U:
    case CG_PIXEL_FORMAT_BGR_323232F:
    case CG_PIXEL_FORMAT_RGB_323232F:
        texture->components = CG_TEXTURE_COMPONENTS_RGB;
        break;

    case CG_PIXEL_FORMAT_RGBA_4444:
    case CG_PIXEL_FORMAT_RGBA_5551:
    case CG_PIXEL_FORMAT_RGBA_8888:
    case CG_PIXEL_FORMAT_BGRA_8888:
    case CG_PIXEL_FORMAT_ARGB_8888:
    case CG_PIXEL_FORMAT_ABGR_8888:
    case CG_PIXEL_FORMAT_RGBA_8888SN:
    case CG_PIXEL_FORMAT_BGRA_8888SN:
    case CG_PIXEL_FORMAT_RGBA_1010102:
    case CG_PIXEL_FORMAT_BGRA_1010102:
    case CG_PIXEL_FORMAT_ARGB_2101010:
    case CG_PIXEL_FORMAT_ABGR_2101010:
    case CG_PIXEL_FORMAT_RGBA_16161616U:
    case CG_PIXEL_FORMAT_BGRA_16161616U:
    case CG_PIXEL_FORMAT_RGBA_16161616F:
    case CG_PIXEL_FORMAT_BGRA_16161616F:
    case CG_PIXEL_FORMAT_RGBA_32323232U:
    case CG_PIXEL_FORMAT_BGRA_32323232U:
    case CG_PIXEL_FORMAT_RGBA_32323232F:
    case CG_PIXEL_FORMAT_BGRA_32323232F:
        texture->components = CG_TEXTURE_COMPONENTS_RGBA;
        break;

    case CG_PIXEL_FORMAT_RGBA_4444_PRE:
    case CG_PIXEL_FORMAT_RGBA_5551_PRE:
    case CG_PIXEL_FORMAT_RGBA_8888_PRE:
    case CG_PIXEL_FORMAT_BGRA_8888_PRE:
    case CG_PIXEL_FORMAT_ARGB_8888_PRE:
    case CG_PIXEL_FORMAT_ABGR_8888_PRE:
    case CG_PIXEL_FORMAT_RGBA_1010102_PRE:
    case CG_PIXEL_FORMAT_BGRA_1010102_PRE:
    case CG_PIXEL_FORMAT_ARGB_2101010_PRE:
    case CG_PIXEL_FORMAT_ABGR_2101010_PRE:
    case CG_PIXEL_FORMAT_RGBA_16161616F_PRE:
    case CG_PIXEL_FORMAT_BGRA_16161616F_PRE:
    case CG_PIXEL_FORMAT_RGBA_32323232F_PRE:
    case CG_PIXEL_FORMAT_BGRA_32323232F_PRE:
        texture->components = CG_TEXTURE_COMPONENTS_RGBA;
        texture->premultiplied = true;
        break;

    case CG_PIXEL_FORMAT_DEPTH_16:
    case CG_PIXEL_FORMAT_DEPTH_32:
        texture->components = CG_TEXTURE_COMPONENTS_DEPTH;
        break;

    case CG_PIXEL_FORMAT_DEPTH_24_STENCIL_8:
        texture->components = CG_TEXTURE_COMPONENTS_DEPTH_STENCIL;
        break;

    case CG_PIXEL_FORMAT_ANY:
        c_assert_not_reached();
    }
}

cg_pixel_format_t
_cg_texture_derive_format(cg_device_t *dev,
                          cg_pixel_format_t src_format,
                          cg_texture_components_t components,
                          bool premultiplied)
{
    switch (components) {
    case CG_TEXTURE_COMPONENTS_DEPTH:
        if (src_format != CG_PIXEL_FORMAT_ANY &&
            _cg_pixel_format_has_depth(src_format))
        {
            return src_format;
        } else {
            if (_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_EXT_PACKED_DEPTH_STENCIL) ||
                _cg_has_private_feature(dev, CG_PRIVATE_FEATURE_OES_PACKED_DEPTH_STENCIL)) {
                return CG_PIXEL_FORMAT_DEPTH_24_STENCIL_8;
            } else
                return CG_PIXEL_FORMAT_DEPTH_16;
        }
    case CG_TEXTURE_COMPONENTS_DEPTH_STENCIL:
        return CG_PIXEL_FORMAT_DEPTH_24_STENCIL_8;
    case CG_TEXTURE_COMPONENTS_A:
    case CG_TEXTURE_COMPONENTS_A8:
        return CG_PIXEL_FORMAT_A_8;
    case CG_TEXTURE_COMPONENTS_A8_SNORM:
        return CG_PIXEL_FORMAT_A_8SN;
    case CG_TEXTURE_COMPONENTS_A16U:
        return CG_PIXEL_FORMAT_A_16U;
    case CG_TEXTURE_COMPONENTS_A16F:
        return CG_PIXEL_FORMAT_A_16F;
    case CG_TEXTURE_COMPONENTS_A32U:
        return CG_PIXEL_FORMAT_A_32U;
    case CG_TEXTURE_COMPONENTS_A32F:
        return CG_PIXEL_FORMAT_A_32F;
    case CG_TEXTURE_COMPONENTS_RG:
    case CG_TEXTURE_COMPONENTS_RG8:
        return CG_PIXEL_FORMAT_RG_88;
    case CG_TEXTURE_COMPONENTS_RG8_SNORM:
        return CG_PIXEL_FORMAT_RG_88SN;
    case CG_TEXTURE_COMPONENTS_RG16U:
        return CG_PIXEL_FORMAT_RG_1616U;
    case CG_TEXTURE_COMPONENTS_RG16F:
        return CG_PIXEL_FORMAT_RG_1616F;
    case CG_TEXTURE_COMPONENTS_RG32U:
        return CG_PIXEL_FORMAT_RG_3232U;
    case CG_TEXTURE_COMPONENTS_RG32F:
        return CG_PIXEL_FORMAT_RG_3232F;
    case CG_TEXTURE_COMPONENTS_RGB:
        if (src_format != CG_PIXEL_FORMAT_ANY && !_cg_pixel_format_has_alpha(src_format) &&
            !_cg_pixel_format_has_depth(src_format))
            return src_format;
        else
            return CG_PIXEL_FORMAT_RGB_888;
    case CG_TEXTURE_COMPONENTS_RGB8:
        return CG_PIXEL_FORMAT_RGB_888;
    case CG_TEXTURE_COMPONENTS_RGB8_SNORM:
        return CG_PIXEL_FORMAT_RGB_888SN;
    case CG_TEXTURE_COMPONENTS_RGB16U:
        return CG_PIXEL_FORMAT_RGB_161616U;
    case CG_TEXTURE_COMPONENTS_RGB16F:
        return CG_PIXEL_FORMAT_RGB_161616F;
    case CG_TEXTURE_COMPONENTS_RGB32U:
        return CG_PIXEL_FORMAT_RGB_323232U;
    case CG_TEXTURE_COMPONENTS_RGB32F:
        return CG_PIXEL_FORMAT_RGB_323232F;
    case CG_TEXTURE_COMPONENTS_RGBA: {
        cg_pixel_format_t format;

        if (src_format != CG_PIXEL_FORMAT_ANY && _cg_pixel_format_has_alpha(src_format) &&
            src_format != CG_PIXEL_FORMAT_A_8)
            format = src_format;
        else
            format = CG_PIXEL_FORMAT_RGBA_8888;

        if (premultiplied) {
            if (_cg_pixel_format_can_be_premultiplied(format))
                return _cg_pixel_format_premultiply(format);
            else
                return CG_PIXEL_FORMAT_RGBA_8888_PRE;
        } else
            return _cg_pixel_format_premult_stem(format);
    }
    case CG_TEXTURE_COMPONENTS_RGBA8:
        return premultiplied ?
            CG_PIXEL_FORMAT_RGBA_8888_PRE : CG_PIXEL_FORMAT_RGBA_8888;
    case CG_TEXTURE_COMPONENTS_RGBA8_SNORM:
        c_return_val_if_fail(!premultiplied, CG_PIXEL_FORMAT_RGBA_8888_PRE);
        return CG_PIXEL_FORMAT_RGBA_8888SN;
    case CG_TEXTURE_COMPONENTS_RGBA16U:
        c_return_val_if_fail(!premultiplied, CG_PIXEL_FORMAT_RGBA_8888_PRE);
        return CG_PIXEL_FORMAT_RGBA_16161616U;
    case CG_TEXTURE_COMPONENTS_RGBA16F:
        return premultiplied ?
            CG_PIXEL_FORMAT_RGBA_16161616F_PRE : CG_PIXEL_FORMAT_RGBA_16161616F;
    case CG_TEXTURE_COMPONENTS_RGBA32U:
        c_return_val_if_fail(!premultiplied, CG_PIXEL_FORMAT_RGBA_8888_PRE);
        return CG_PIXEL_FORMAT_RGBA_32323232U;
    case CG_TEXTURE_COMPONENTS_RGBA32F:
        return premultiplied ?
            CG_PIXEL_FORMAT_RGBA_32323232F_PRE : CG_PIXEL_FORMAT_RGBA_32323232F;
    }

    c_return_val_if_reached(CG_PIXEL_FORMAT_RGBA_8888_PRE);
}

cg_pixel_format_t
_cg_texture_determine_internal_format(cg_texture_t *texture,
                                      cg_pixel_format_t src_format)
{
    return _cg_texture_derive_format(texture->dev,
                                     src_format,
                                     texture->components,
                                     texture->premultiplied);
}

void
cg_texture_set_components(cg_texture_t *texture,
                          cg_texture_components_t components)
{
    c_return_if_fail(!texture->allocated);

    if (texture->components == components)
        return;

    texture->components = components;
}

cg_texture_components_t
cg_texture_get_components(cg_texture_t *texture)
{
    return texture->components;
}

void
cg_texture_set_premultiplied(cg_texture_t *texture, bool premultiplied)
{
    c_return_if_fail(!texture->allocated);

    premultiplied = !!premultiplied;

    if (texture->premultiplied == premultiplied)
        return;

    texture->premultiplied = premultiplied;
}

bool
cg_texture_get_premultiplied(cg_texture_t *texture)
{
    return texture->premultiplied;
}

void
_cg_texture_copy_internal_format(cg_texture_t *src, cg_texture_t *dest)
{
    cg_texture_set_components(dest, src->components);
    cg_texture_set_premultiplied(dest, src->premultiplied);
}
