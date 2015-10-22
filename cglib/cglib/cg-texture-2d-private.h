/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2009 Intel Corporation.
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
 */

#ifndef __CG_TEXTURE_2D_PRIVATE_H
#define __CG_TEXTURE_2D_PRIVATE_H

#include "cg-object-private.h"
#include "cg-pipeline-private.h"
#include "cg-texture-private.h"
#include "cg-texture-2d.h"

#ifdef CG_HAS_EGL_SUPPORT
#include "cg-egl-defines.h"
#endif

struct _cg_texture_2d_t {
    cg_texture_t _parent;

    /* The internal format of the GL texture represented as a
       cg_pixel_format_t */
    cg_pixel_format_t internal_format;

    bool auto_mipmap;
    bool mipmaps_dirty;
    bool is_foreign;

    /* TODO: factor out these OpenGL specific members into some form
     * of driver private state. */

    /* The internal format of the GL texture represented as a GL enum */
    GLenum gl_internal_format;
    /* The texture object number */
    GLuint gl_texture;
    GLenum gl_legacy_texobj_min_filter;
    GLenum gl_legacy_texobj_mag_filter;
    GLint gl_legacy_texobj_wrap_mode_s;
    GLint gl_legacy_texobj_wrap_mode_t;
};

#if defined(CG_HAS_EGL_SUPPORT) && defined(EGL_KHR_image_base)
/* NB: The reason we require the width, height and format to be passed
 * even though they may seem redundant is because GLES 1/2 don't
 * provide a way to query these properties. */
cg_texture_2d_t *_cg_egl_texture_2d_new_from_image(cg_device_t *dev,
                                                   int width,
                                                   int height,
                                                   cg_pixel_format_t format,
                                                   EGLImageKHR image,
                                                   cg_error_t **error);
#endif

cg_texture_2d_t *_cg_texture_2d_create_base(cg_device_t *dev,
                                            int width,
                                            int height,
                                            cg_pixel_format_t internal_format,
                                            cg_texture_loader_t *loader);

void _cg_texture_2d_set_auto_mipmap(cg_texture_t *tex, bool value);

/*
 * _cg_texture_2d_externally_modified:
 * @texture: A #cg_texture_2d_t object
 *
 * This should be called whenever the texture is modified other than
 * by using cg_texture_set_region. It will cause the mipmaps to be
 * invalidated
 */
void _cg_texture_2d_externally_modified(cg_texture_t *texture);

/*
 * _cg_texture_2d_copy_from_framebuffer:
 * @texture: A #cg_texture_2d_t pointer
 * @src_x: X-position to within the framebuffer to read from
 * @src_y: Y-position to within the framebuffer to read from
 * @width: width of the rectangle to copy
 * @height: height of the rectangle to copy
 * @src_fb: A source #cg_framebuffer_t to copy from
 * @dst_x: X-position to store the image within the texture
 * @dst_y: Y-position to store the image within the texture
 * @level: The mipmap level of @texture to copy too
 *
 * This copies a portion of the given @src_fb into the
 * texture.
 */
void _cg_texture_2d_copy_from_framebuffer(cg_texture_2d_t *texture,
                                          int src_x,
                                          int src_y,
                                          int width,
                                          int height,
                                          cg_framebuffer_t *src_fb,
                                          int dst_x,
                                          int dst_y,
                                          int level);

#endif /* __CG_TEXTURE_2D_PRIVATE_H */
