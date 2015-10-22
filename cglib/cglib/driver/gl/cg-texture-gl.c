/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
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

#include <cglib-config.h>

#include <strings.h>

#include "cg-device-private.h"
#include "cg-util-gl-private.h"
#include "cg-texture-gl-private.h"
#include "cg-texture-3d-private.h"
#include "cg-util.h"
#include "cg-pipeline-opengl-private.h"

static inline int
calculate_alignment(int rowstride)
{
    int alignment = 1 << (_cg_util_ffs(rowstride) - 1);

    return MIN(alignment, 8);
}

void
_cg_texture_gl_prep_alignment_for_pixels_upload(cg_device_t *dev,
                                                int pixels_rowstride)
{
    GE(dev,
       glPixelStorei(GL_UNPACK_ALIGNMENT,
                     calculate_alignment(pixels_rowstride)));
}

void
_cg_texture_gl_prep_alignment_for_pixels_download(cg_device_t *dev,
                                                  int bpp,
                                                  int width,
                                                  int rowstride)
{
    int alignment;

    /* If no padding is needed then we can always use an alignment of 1.
     * We want to do this even though it is equivalent to the alignment
     * of the rowstride because the Intel driver in Mesa currently has
     * an optimisation when reading data into a PBO that only works if
     * the alignment is exactly 1.
     *
     * https://bugs.freedesktop.org/show_bug.cgi?id=46632
     */

    if (rowstride == bpp * width)
        alignment = 1;
    else
        alignment = calculate_alignment(rowstride);

    GE(dev, glPixelStorei(GL_PACK_ALIGNMENT, alignment));
}

void
_cg_texture_gl_flush_legacy_texobj_wrap_modes(cg_texture_t *texture,
                                              unsigned int wrap_mode_s,
                                              unsigned int wrap_mode_t,
                                              unsigned int wrap_mode_p)
{
    texture->vtable->gl_flush_legacy_texobj_wrap_modes(
        texture, wrap_mode_s, wrap_mode_t, wrap_mode_p);
}

void
_cg_texture_gl_flush_legacy_texobj_filters(cg_texture_t *texture,
                                           unsigned int min_filter,
                                           unsigned int mag_filter)
{
    texture->vtable->gl_flush_legacy_texobj_filters(
        texture, min_filter, mag_filter);
}

void
_cg_texture_gl_maybe_update_max_level(cg_texture_t *texture, int max_level)
{
/* This isn't supported on GLES */
#ifdef CG_HAS_GL_SUPPORT
    cg_device_t *dev = texture->dev;

    if (_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_TEXTURE_MAX_LEVEL) &&
        texture->max_level < max_level) {
        cg_device_t *dev = texture->dev;
        GLuint gl_handle;
        GLenum gl_target;

        cg_texture_get_gl_texture(texture, &gl_handle, &gl_target);

        texture->max_level = max_level;

        _cg_bind_gl_texture_transient(
            gl_target, gl_handle, _cg_texture_is_foreign(texture));

        GE(dev,
           glTexParameteri(
               gl_target, GL_TEXTURE_MAX_LEVEL, texture->max_level));
    }
#endif /* CG_HAS_GL_SUPPORT */
}

void
_cg_texture_gl_generate_mipmaps(cg_texture_t *texture)
{
    cg_device_t *dev = texture->dev;
    int n_levels = _cg_texture_get_n_levels(texture);
    GLuint gl_handle;
    GLenum gl_target;

    _cg_texture_gl_maybe_update_max_level(texture, n_levels - 1);

    cg_texture_get_gl_texture(texture, &gl_handle, &gl_target);

    _cg_bind_gl_texture_transient(
        gl_target, gl_handle, _cg_texture_is_foreign(texture));
    GE(dev, glGenerateMipmap(gl_target));
}

GLenum
_cg_texture_gl_get_format(cg_texture_t *texture)
{
    return texture->vtable->get_gl_format(texture);
}
