/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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

#include "cg-private.h"
#include "cg-util.h"
#include "cg-bitmap.h"
#include "cg-bitmap-private.h"
#include "cg-texture-private.h"
#include "cg-pipeline.h"
#include "cg-pipeline-opengl-private.h"
#include "cg-device-private.h"
#include "cg-object-private.h"
#include "cg-util-gl-private.h"
#include "cg-error-private.h"
#include "cg-texture-gl-private.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifndef GL_TEXTURE_3D
#define GL_TEXTURE_3D 0x806F
#endif
#ifndef GL_MAX_3D_TEXTURE_SIZE_OES
#define GL_MAX_3D_TEXTURE_SIZE_OES 0x8073
#endif

/* These may not be defined for drivers without GL_EXT_unpack_subimage */
#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#endif
#ifndef GL_UNPACK_SKIP_ROWS
#define GL_UNPACK_SKIP_ROWS 0x0CF3
#endif
#ifndef GL_UNPACK_SKIP_PIXELS
#define GL_UNPACK_SKIP_PIXELS 0x0CF4
#endif

static GLuint
_cg_texture_driver_gen(cg_device_t *dev,
                       GLenum gl_target,
                       cg_pixel_format_t internal_format)
{
    GLuint tex;

    GE(dev, glGenTextures(1, &tex));

    _cg_bind_gl_texture_transient(gl_target, tex, false);

    switch (gl_target) {
    case GL_TEXTURE_2D:
    case GL_TEXTURE_3D:
        /* GL_TEXTURE_MAG_FILTER defaults to GL_LINEAR, no need to set it */
        GE(dev, glTexParameteri(gl_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        break;

    default:
        c_assert_not_reached();
    }

    return tex;
}

static void
prep_gl_for_pixels_upload_full(cg_device_t *dev,
                               int pixels_rowstride,
                               int pixels_src_x,
                               int pixels_src_y,
                               int pixels_bpp)
{
    if (_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_UNPACK_SUBIMAGE)) {
        GE(dev,
           glPixelStorei(GL_UNPACK_ROW_LENGTH, pixels_rowstride / pixels_bpp));

        GE(dev, glPixelStorei(GL_UNPACK_SKIP_PIXELS, pixels_src_x));
        GE(dev, glPixelStorei(GL_UNPACK_SKIP_ROWS, pixels_src_y));
    } else {
        c_assert(pixels_src_x == 0);
        c_assert(pixels_src_y == 0);
    }

    _cg_texture_gl_prep_alignment_for_pixels_upload(dev, pixels_rowstride);
}

static void
_cg_texture_driver_prep_gl_for_pixels_upload(cg_device_t *dev,
                                             int pixels_rowstride,
                                             int pixels_bpp)
{
    prep_gl_for_pixels_upload_full(dev,
                                   pixels_rowstride,
                                   0,
                                   0, /* src_x/y */
                                   pixels_bpp);
}

static void
_cg_texture_driver_prep_gl_for_pixels_download(cg_device_t *dev,
                                               int pixels_rowstride,
                                               int image_width,
                                               int pixels_bpp)
{
    _cg_texture_gl_prep_alignment_for_pixels_download(dev, pixels_bpp,
                                                      image_width,
                                                      pixels_rowstride);
}

static cg_bitmap_t *
prepare_bitmap_alignment_for_upload(cg_device_t *dev,
                                    cg_bitmap_t *src_bmp,
                                    cg_error_t **error)
{
    cg_pixel_format_t format = cg_bitmap_get_format(src_bmp);
    int bpp = _cg_pixel_format_get_bytes_per_pixel(format);
    int src_rowstride = cg_bitmap_get_rowstride(src_bmp);
    int width = cg_bitmap_get_width(src_bmp);
    int alignment = 1;

    if (_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_UNPACK_SUBIMAGE) ||
        src_rowstride == 0)
        return cg_object_ref(src_bmp);

    /* Work out the alignment of the source rowstride */
    alignment = 1 << (_cg_util_ffs(src_rowstride) - 1);
    alignment = MIN(alignment, 8);

    /* If the aligned data equals the rowstride then we can upload from
       the bitmap directly using GL_UNPACK_ALIGNMENT */
    if (((width * bpp + alignment - 1) & ~(alignment - 1)) == src_rowstride)
        return cg_object_ref(src_bmp);
    /* Otherwise we need to copy the bitmap to pack the alignment
       because GLES has no GL_ROW_LENGTH */
    else
        return _cg_bitmap_copy(src_bmp, error);
}

static bool
_cg_texture_driver_upload_subregion_to_gl(cg_device_t *dev,
                                          cg_texture_t *texture,
                                          bool is_foreign,
                                          int src_x,
                                          int src_y,
                                          int dst_x,
                                          int dst_y,
                                          int width,
                                          int height,
                                          int level,
                                          cg_bitmap_t *source_bmp,
                                          GLuint source_gl_format,
                                          GLuint source_gl_type,
                                          cg_error_t **error)
{
    GLenum gl_target;
    GLuint gl_handle;
    uint8_t *data;
    cg_pixel_format_t source_format = cg_bitmap_get_format(source_bmp);
    int bpp = _cg_pixel_format_get_bytes_per_pixel(source_format);
    cg_bitmap_t *slice_bmp;
    int rowstride;
    GLenum gl_error;
    bool status = true;
    cg_error_t *internal_error = NULL;
    int level_width;
    int level_height;

    cg_texture_get_gl_texture(texture, &gl_handle, &gl_target);

    /* If we have the GL_EXT_unpack_subimage extension then we can
       upload from subregions directly. Otherwise we may need to copy
       the bitmap */
    if (!_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_UNPACK_SUBIMAGE) &&
        (src_x != 0 || src_y != 0 || width != cg_bitmap_get_width(source_bmp) ||
         height != cg_bitmap_get_height(source_bmp))) {
        slice_bmp = _cg_bitmap_new_with_malloc_buffer(dev, width, height,
                                                      source_format, error);
        if (!slice_bmp)
            return false;

        if (!_cg_bitmap_copy_subregion(source_bmp,
                                       slice_bmp,
                                       src_x,
                                       src_y,
                                       0,
                                       0, /* dst_x/y */
                                       width,
                                       height,
                                       error)) {
            cg_object_unref(slice_bmp);
            return false;
        }

        src_x = src_y = 0;
    } else {
        slice_bmp = prepare_bitmap_alignment_for_upload(dev, source_bmp,
                                                        error);
        if (!slice_bmp)
            return false;
    }

    rowstride = cg_bitmap_get_rowstride(slice_bmp);

    /* Setup gl alignment to match rowstride and top-left corner */
    prep_gl_for_pixels_upload_full(dev, rowstride, src_x, src_y, bpp);

    data = _cg_bitmap_gl_bind(
        slice_bmp, CG_BUFFER_ACCESS_READ, 0, &internal_error);

    /* NB: _cg_bitmap_gl_bind() may return NULL when successfull so we
     * have to explicitly check the cg error pointer to catch
     * problems... */
    if (internal_error) {
        _cg_propagate_error(error, internal_error);
        cg_object_unref(slice_bmp);
        return false;
    }

    _cg_bind_gl_texture_transient(gl_target, gl_handle, is_foreign);

    /* Clear any GL errors */
    while ((gl_error = dev->glGetError()) != GL_NO_ERROR)
        ;

    _cg_texture_get_level_size(
        texture, level, &level_width, &level_height, NULL);

    if (level_width == width && level_height == height) {
        /* GL gets upset if you use glTexSubImage2D to define the
         * contents of a mipmap level so we make sure to use
         * glTexImage2D if we are uploading a full mipmap level.
         */
        dev->glTexImage2D(gl_target,
                          level,
                          _cg_texture_gl_get_format(texture),
                          width,
                          height,
                          0,
                          source_gl_format,
                          source_gl_type,
                          data);
    } else {
        /* GL gets upset if you use glTexSubImage2D to initialize the
         * contents of a mipmap level so if this is the first time
         * we've seen a request to upload to this level we call
         * glTexImage2D first to assert that the storage for this
         * level exists.
         */
        if (texture->max_level < level) {
            dev->glTexImage2D(gl_target,
                              level,
                              _cg_texture_gl_get_format(texture),
                              level_width,
                              level_height,
                              0,
                              source_gl_format,
                              source_gl_type,
                              NULL);
        }

        dev->glTexSubImage2D(gl_target,
                             level,
                             dst_x,
                             dst_y,
                             width,
                             height,
                             source_gl_format,
                             source_gl_type,
                             data);
    }

    if (_cg_gl_util_catch_out_of_memory(dev, error))
        status = false;

    _cg_bitmap_gl_unbind(slice_bmp);

    cg_object_unref(slice_bmp);

    return status;
}

static bool
_cg_texture_driver_upload_to_gl(cg_device_t *dev,
                                GLenum gl_target,
                                GLuint gl_handle,
                                bool is_foreign,
                                cg_bitmap_t *source_bmp,
                                GLint internal_gl_format,
                                GLuint source_gl_format,
                                GLuint source_gl_type,
                                cg_error_t **error)
{
    cg_pixel_format_t source_format = cg_bitmap_get_format(source_bmp);
    int bpp = _cg_pixel_format_get_bytes_per_pixel(source_format);
    int rowstride;
    int bmp_width = cg_bitmap_get_width(source_bmp);
    int bmp_height = cg_bitmap_get_height(source_bmp);
    cg_bitmap_t *bmp;
    uint8_t *data;
    GLenum gl_error;
    cg_error_t *internal_error = NULL;
    bool status = true;

    bmp = prepare_bitmap_alignment_for_upload(dev, source_bmp, error);
    if (!bmp)
        return false;

    rowstride = cg_bitmap_get_rowstride(bmp);

    /* Setup gl alignment to match rowstride and top-left corner */
    _cg_texture_driver_prep_gl_for_pixels_upload(dev, rowstride, bpp);

    _cg_bind_gl_texture_transient(gl_target, gl_handle, is_foreign);

    data = _cg_bitmap_gl_bind(bmp,
                              CG_BUFFER_ACCESS_READ,
                              0, /* hints */
                              &internal_error);

    /* NB: _cg_bitmap_gl_bind() may return NULL when successful so we
     * have to explicitly check the cg error pointer to catch
     * problems... */
    if (internal_error) {
        cg_object_unref(bmp);
        _cg_propagate_error(error, internal_error);
        return false;
    }

    /* Clear any GL errors */
    while ((gl_error = dev->glGetError()) != GL_NO_ERROR)
        ;

    dev->glTexImage2D(gl_target,
                      0,
                      internal_gl_format,
                      bmp_width,
                      bmp_height,
                      0,
                      source_gl_format,
                      source_gl_type,
                      data);

    if (_cg_gl_util_catch_out_of_memory(dev, error))
        status = false;

    _cg_bitmap_gl_unbind(bmp);

    cg_object_unref(bmp);

    return status;
}

static bool
_cg_texture_driver_upload_to_gl_3d(cg_device_t *dev,
                                   GLenum gl_target,
                                   GLuint gl_handle,
                                   bool is_foreign,
                                   GLint height,
                                   GLint depth,
                                   cg_bitmap_t *source_bmp,
                                   GLint internal_gl_format,
                                   GLuint source_gl_format,
                                   GLuint source_gl_type,
                                   cg_error_t **error)
{
    cg_pixel_format_t source_format = cg_bitmap_get_format(source_bmp);
    int bpp = _cg_pixel_format_get_bytes_per_pixel(source_format);
    int rowstride = cg_bitmap_get_rowstride(source_bmp);
    int bmp_width = cg_bitmap_get_width(source_bmp);
    int bmp_height = cg_bitmap_get_height(source_bmp);
    uint8_t *data;
    GLenum gl_error;

    _cg_bind_gl_texture_transient(gl_target, gl_handle, is_foreign);

    /* If the rowstride or image height can't be specified with just
       GL_ALIGNMENT alone then we need to copy the bitmap because there
       is no GL_ROW_LENGTH */
    if (rowstride / bpp != bmp_width || height != bmp_height / depth) {
        cg_bitmap_t *bmp;
        int image_height = bmp_height / depth;
        cg_pixel_format_t source_bmp_format = cg_bitmap_get_format(source_bmp);
        int i;

        _cg_texture_driver_prep_gl_for_pixels_upload(dev, bmp_width * bpp,
                                                     bpp);

        /* Initialize the texture with empty data and then upload each
           image with a sub-region update */

        /* Clear any GL errors */
        while ((gl_error = dev->glGetError()) != GL_NO_ERROR)
            ;

        dev->glTexImage3D(gl_target,
                          0, /* level */
                          internal_gl_format,
                          bmp_width,
                          height,
                          depth,
                          0,
                          source_gl_format,
                          source_gl_type,
                          NULL);

        if (_cg_gl_util_catch_out_of_memory(dev, error))
            return false;

        bmp = _cg_bitmap_new_with_malloc_buffer(dev, bmp_width, height,
                                                source_bmp_format, error);
        if (!bmp)
            return false;

        for (i = 0; i < depth; i++) {
            if (!_cg_bitmap_copy_subregion(source_bmp,
                                           bmp,
                                           0,
                                           image_height * i,
                                           0,
                                           0,
                                           bmp_width,
                                           height,
                                           error)) {
                cg_object_unref(bmp);
                return false;
            }

            data = _cg_bitmap_gl_bind(bmp, CG_BUFFER_ACCESS_READ, 0, error);
            if (!data) {
                cg_object_unref(bmp);
                return false;
            }

            /* Clear any GL errors */
            while ((gl_error = dev->glGetError()) != GL_NO_ERROR)
                ;

            dev->glTexSubImage3D(gl_target,
                                 0, /* level */
                                 0, /* xoffset */
                                 0, /* yoffset */
                                 i, /* zoffset */
                                 bmp_width, /* width */
                                 height, /* height */
                                 1, /* depth */
                                 source_gl_format,
                                 source_gl_type,
                                 data);

            if (_cg_gl_util_catch_out_of_memory(dev, error)) {
                cg_object_unref(bmp);
                _cg_bitmap_gl_unbind(bmp);
                return false;
            }

            _cg_bitmap_gl_unbind(bmp);
        }

        cg_object_unref(bmp);
    } else {
        data = _cg_bitmap_gl_bind(source_bmp, CG_BUFFER_ACCESS_READ, 0, error);
        if (!data)
            return false;

        _cg_texture_driver_prep_gl_for_pixels_upload(dev, rowstride, bpp);

        /* Clear any GL errors */
        while ((gl_error = dev->glGetError()) != GL_NO_ERROR)
            ;

        dev->glTexImage3D(gl_target,
                          0, /* level */
                          internal_gl_format,
                          bmp_width,
                          height,
                          depth,
                          0,
                          source_gl_format,
                          source_gl_type,
                          data);

        if (_cg_gl_util_catch_out_of_memory(dev, error)) {
            _cg_bitmap_gl_unbind(source_bmp);
            return false;
        }

        _cg_bitmap_gl_unbind(source_bmp);
    }

    return true;
}

/* NB: GLES doesn't support glGetTexImage2D, so cg-texture will instead
 * fallback to a generic render + readpixels approach to downloading
 * texture data. (See _cg_texture_draw_and_read() ) */
static bool
_cg_texture_driver_gl_get_tex_image(cg_device_t *dev,
                                    GLenum gl_target,
                                    GLenum dest_gl_format,
                                    GLenum dest_gl_type,
                                    uint8_t *dest)
{
    return false;
}

static bool
_cg_texture_driver_size_supported_3d(cg_device_t *dev,
                                     GLenum gl_target,
                                     GLenum gl_format,
                                     GLenum gl_type,
                                     int width,
                                     int height,
                                     int depth)
{
    GLint max_size;

    /* GLES doesn't support a proxy texture target so let's at least
       check whether the size is greater than
       GL_MAX_3D_TEXTURE_SIZE_OES */
    GE(dev, glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE_OES, &max_size));

    return width <= max_size && height <= max_size && depth <= max_size;
}

static bool
_cg_texture_driver_size_supported(cg_device_t *dev,
                                  GLenum gl_target,
                                  GLenum gl_intformat,
                                  GLenum gl_format,
                                  GLenum gl_type,
                                  int width,
                                  int height)
{
    GLint max_size;

    /* GLES doesn't support a proxy texture target so let's at least
       check whether the size is greater than GL_MAX_TEXTURE_SIZE */
    GE(dev, glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size));

    return width <= max_size && height <= max_size;
}

static void
_cg_texture_driver_try_setting_gl_border_color(cg_device_t *dev,
					       GLuint gl_target,
					       const GLfloat *transparent_color)
{
    /* FAIL! */
}

static cg_pixel_format_t
_cg_texture_driver_find_best_gl_get_data_format(cg_device_t *dev,
                                                cg_pixel_format_t format,
                                                GLenum *closest_gl_format,
                                                GLenum *closest_gl_type)
{
    /* Find closest format that's supported by GL
       (Can't use _cg_pixel_format_to_gl since available formats
        when reading pixels on GLES are severely limited) */
    *closest_gl_format = GL_RGBA;
    *closest_gl_type = GL_UNSIGNED_BYTE;
    return CG_PIXEL_FORMAT_RGBA_8888;
}

const cg_texture_driver_t _cg_texture_driver_gles = {
    _cg_texture_driver_gen,
    _cg_texture_driver_prep_gl_for_pixels_upload,
    _cg_texture_driver_upload_subregion_to_gl,
    _cg_texture_driver_upload_to_gl,
    _cg_texture_driver_upload_to_gl_3d,
    _cg_texture_driver_prep_gl_for_pixels_download,
    _cg_texture_driver_gl_get_tex_image,
    _cg_texture_driver_size_supported,
    _cg_texture_driver_size_supported_3d,
    _cg_texture_driver_try_setting_gl_border_color,
    _cg_texture_driver_find_best_gl_get_data_format
};
