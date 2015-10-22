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
#include "cg-device-private.h"
#include "cg-object-private.h"
#include "cg-pipeline-opengl-private.h"
#include "cg-util-gl-private.h"
#include "cg-error-private.h"
#include "cg-texture-gl-private.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifndef GL_TEXTURE_SWIZZLE_RGBA
#define GL_TEXTURE_SWIZZLE_RGBA 0x8E46
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
/* In case automatic mipmap generation gets disabled for this
 * texture but a minification filter depending on mipmap
 * interpolation is selected then we initialize the max mipmap
 * level to 0 so OpenGL will consider the texture storage to be
 * "complete".
 */
#ifdef CG_HAS_GL_SUPPORT
        if (_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_TEXTURE_MAX_LEVEL))
            GE(dev, glTexParameteri(gl_target, GL_TEXTURE_MAX_LEVEL, 0));
#endif

        /* GL_TEXTURE_MAG_FILTER defaults to GL_LINEAR, no need to set it */
        GE(dev, glTexParameteri(gl_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        break;

    default:
        c_assert_not_reached();
    }

    /* If the driver doesn't support alpha textures directly then we'll
     * fake them by setting the swizzle parameters */
    if (internal_format == CG_PIXEL_FORMAT_A_8 &&
        !_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_ALPHA_TEXTURES) &&
        _cg_has_private_feature(dev, CG_PRIVATE_FEATURE_TEXTURE_SWIZZLE)) {
        static const GLint red_swizzle[] = {
            GL_ZERO, GL_ZERO, GL_ZERO, GL_RED
        };

        GE(dev,
           glTexParameteriv(gl_target, GL_TEXTURE_SWIZZLE_RGBA, red_swizzle));
    }

    return tex;
}

/* OpenGL - unlike GLES - can upload a sub region of pixel data from a larger
 * source buffer */
static void
prep_gl_for_pixels_upload_full(cg_device_t *dev,
                               int pixels_rowstride,
                               int image_height,
                               int pixels_src_x,
                               int pixels_src_y,
                               int pixels_bpp)
{
    GE(dev,
       glPixelStorei(GL_UNPACK_ROW_LENGTH, pixels_rowstride / pixels_bpp));

    GE(dev, glPixelStorei(GL_UNPACK_SKIP_PIXELS, pixels_src_x));
    GE(dev, glPixelStorei(GL_UNPACK_SKIP_ROWS, pixels_src_y));

    if (cg_has_feature(dev, CG_FEATURE_ID_TEXTURE_3D))
        GE(dev, glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, image_height));

    _cg_texture_gl_prep_alignment_for_pixels_upload(dev, pixels_rowstride);
}

static void
_cg_texture_driver_prep_gl_for_pixels_upload(cg_device_t *dev,
                                             int pixels_rowstride,
                                             int pixels_bpp)
{
    prep_gl_for_pixels_upload_full(dev, pixels_rowstride, 0, 0, 0,
                                   pixels_bpp);
}

/* OpenGL - unlike GLES - can download pixel data into a sub region of
 * a larger destination buffer */
static void
prep_gl_for_pixels_download_full(cg_device_t *dev,
                                 int image_width,
                                 int pixels_rowstride,
                                 int image_height,
                                 int pixels_src_x,
                                 int pixels_src_y,
                                 int pixels_bpp)
{
    GE(dev, glPixelStorei(GL_PACK_ROW_LENGTH, pixels_rowstride / pixels_bpp));

    GE(dev, glPixelStorei(GL_PACK_SKIP_PIXELS, pixels_src_x));
    GE(dev, glPixelStorei(GL_PACK_SKIP_ROWS, pixels_src_y));

    if (cg_has_feature(dev, CG_FEATURE_ID_TEXTURE_3D))
        GE(dev, glPixelStorei(GL_PACK_IMAGE_HEIGHT, image_height));

    _cg_texture_gl_prep_alignment_for_pixels_download(dev, pixels_bpp,
                                                      image_width,
                                                      pixels_rowstride);
}

static void
_cg_texture_driver_prep_gl_for_pixels_download(cg_device_t *dev,
                                               int image_width,
                                               int pixels_rowstride,
                                               int pixels_bpp)
{
    prep_gl_for_pixels_download_full(dev,
                                     pixels_rowstride,
                                     image_width,
                                     0 /* image height */,
                                     0,
                                     0, /* pixels_src_x/y */
                                     pixels_bpp);
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
    GLenum gl_error;
    bool status = true;
    cg_error_t *internal_error = NULL;
    int level_width;
    int level_height;

    cg_texture_get_gl_texture(texture, &gl_handle, &gl_target);

    data = _cg_bitmap_gl_bind(
        source_bmp, CG_BUFFER_ACCESS_READ, 0, &internal_error);

    /* NB: _cg_bitmap_gl_bind() may return NULL when successfull so we
     * have to explicitly check the cg error pointer to catch
     * problems... */
    if (internal_error) {
        _cg_propagate_error(error, internal_error);
        return false;
    }

    /* Setup gl alignment to match rowstride and top-left corner */
    prep_gl_for_pixels_upload_full(dev, cg_bitmap_get_rowstride(source_bmp),
                                   0, src_x, src_y, bpp);

    _cg_bind_gl_texture_transient(gl_target, gl_handle, is_foreign);

    /* Clear any GL errors */
    while ((gl_error = dev->glGetError()) != GL_NO_ERROR)
        ;

    _cg_texture_get_level_size(
        texture, level, &level_width, &level_height, NULL);

    if (level_width == width && level_height == height) {
        /* GL gets upset if you use glTexSubImage2D to initialize the
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

    _cg_bitmap_gl_unbind(source_bmp);

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
    uint8_t *data;
    cg_pixel_format_t source_format = cg_bitmap_get_format(source_bmp);
    int bpp = _cg_pixel_format_get_bytes_per_pixel(source_format);
    GLenum gl_error;
    bool status = true;
    cg_error_t *internal_error = NULL;

    data = _cg_bitmap_gl_bind(source_bmp,
                              CG_BUFFER_ACCESS_READ,
                              0, /* hints */
                              &internal_error);

    /* NB: _cg_bitmap_gl_bind() may return NULL when successful so we
     * have to explicitly check the cg error pointer to catch
     * problems... */
    if (internal_error) {
        _cg_propagate_error(error, internal_error);
        return false;
    }

    /* Setup gl alignment to match rowstride and top-left corner */
    prep_gl_for_pixels_upload_full(dev, cg_bitmap_get_rowstride(source_bmp),
                                   0, 0, 0, bpp);

    _cg_bind_gl_texture_transient(gl_target, gl_handle, is_foreign);

    /* Clear any GL errors */
    while ((gl_error = dev->glGetError()) != GL_NO_ERROR)
        ;

    dev->glTexImage2D(gl_target,
                      0,
                      internal_gl_format,
                      cg_bitmap_get_width(source_bmp),
                      cg_bitmap_get_height(source_bmp),
                      0,
                      source_gl_format,
                      source_gl_type,
                      data);

    if (_cg_gl_util_catch_out_of_memory(dev, error))
        status = false;

    _cg_bitmap_gl_unbind(source_bmp);

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
    uint8_t *data;
    cg_pixel_format_t source_format = cg_bitmap_get_format(source_bmp);
    int bpp = _cg_pixel_format_get_bytes_per_pixel(source_format);
    GLenum gl_error;
    bool status = true;

    data = _cg_bitmap_gl_bind(source_bmp, CG_BUFFER_ACCESS_READ, 0, error);
    if (!data)
        return false;

    /* Setup gl alignment to match rowstride and top-left corner */
    prep_gl_for_pixels_upload_full(dev,
                                   cg_bitmap_get_rowstride(source_bmp),
                                   (cg_bitmap_get_height(source_bmp) / depth),
                                   0,
                                   0,
                                   bpp);

    _cg_bind_gl_texture_transient(gl_target, gl_handle, is_foreign);

    /* Clear any GL errors */
    while ((gl_error = dev->glGetError()) != GL_NO_ERROR)
        ;

    dev->glTexImage3D(gl_target,
                      0, /* level */
                      internal_gl_format,
                      cg_bitmap_get_width(source_bmp),
                      height,
                      depth,
                      0,
                      source_gl_format,
                      source_gl_type,
                      data);

    if (_cg_gl_util_catch_out_of_memory(dev, error))
        status = false;

    _cg_bitmap_gl_unbind(source_bmp);

    return status;
}

static bool
_cg_texture_driver_gl_get_tex_image(cg_device_t *dev,
                                    GLenum gl_target,
                                    GLenum dest_gl_format,
                                    GLenum dest_gl_type,
                                    uint8_t *dest)
{
    GE(dev,
       glGetTexImage(gl_target,
                     0, /* level */
                     dest_gl_format,
                     dest_gl_type,
                     (GLvoid *)dest));
    return true;
}

static bool
_cg_texture_driver_size_supported_3d(cg_device_t *dev,
                                     GLenum gl_target,
                                     GLenum gl_internal_format,
                                     GLenum gl_type,
                                     int width,
                                     int height,
                                     int depth)
{
    GLenum proxy_target;
    GLint new_width = 0;

    if (gl_target == GL_TEXTURE_3D)
        proxy_target = GL_PROXY_TEXTURE_3D;
    else
        /* Unknown target, assume it's not supported */
        return false;

    /* Proxy texture allows for a quick check for supported size */
    GE(dev,
       glTexImage3D(proxy_target,
                    0,
                    gl_internal_format,
                    width,
                    height,
                    depth,
                    0 /* border */,
                    GL_RGBA,
                    gl_type,
                    NULL));

    GE(dev,
       glGetTexLevelParameteriv(proxy_target, 0, GL_TEXTURE_WIDTH, &new_width));

    return new_width != 0;
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
    GLenum proxy_target;
    GLint new_width = 0;

    if (gl_target == GL_TEXTURE_2D)
        proxy_target = GL_PROXY_TEXTURE_2D;
    else
        /* Unknown target, assume it's not supported */
        return false;

    /* Proxy texture allows for a quick check for supported size */
    GE(dev,
       glTexImage2D(proxy_target,
                    0,
                    gl_intformat,
                    width,
                    height,
                    0 /* border */,
                    gl_format,
                    gl_type,
                    NULL));

    GE(dev,
       glGetTexLevelParameteriv(proxy_target, 0, GL_TEXTURE_WIDTH, &new_width));

    return new_width != 0;
}

static void
_cg_texture_driver_try_setting_gl_border_color(cg_device_t *dev,
                                               GLuint gl_target,
                                               const GLfloat *transparent_color)
{
    /* Use a transparent border color so that we can leave the
       color buffer alone when using texture co-ordinates
       outside of the texture */
    GE(dev,
       glTexParameterfv(gl_target, GL_TEXTURE_BORDER_COLOR, transparent_color));
}

static cg_pixel_format_t
_cg_texture_driver_find_best_gl_get_data_format(cg_device_t *dev,
                                                cg_pixel_format_t format,
                                                GLenum *closest_gl_format,
                                                GLenum *closest_gl_type)
{
    return dev->driver_vtable->pixel_format_to_gl(dev,
                                                      format,
                                                      NULL, /* don't need */
                                                      closest_gl_format,
                                                      closest_gl_type);
}

const cg_texture_driver_t _cg_texture_driver_gl = {
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
