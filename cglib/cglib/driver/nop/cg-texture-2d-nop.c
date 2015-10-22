/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2009,2010,2011,2012 Intel Corporation.
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
 *  Neil Roberts   <neil@linux.intel.com>
 *  Robert Bragg   <robert@linux.intel.com>
 */

#include <cglib-config.h>

#include <string.h>

#include "cg-private.h"
#include "cg-texture-2d-nop-private.h"
#include "cg-texture-2d-private.h"
#include "cg-error-private.h"

void
_cg_texture_2d_nop_free(cg_texture_2d_t *tex_2d)
{
}

bool
_cg_texture_2d_nop_can_create(cg_device_t *dev,
                              int width,
                              int height,
                              cg_pixel_format_t internal_format)
{
    return true;
}

void
_cg_texture_2d_nop_init(cg_texture_2d_t *tex_2d)
{
}

bool
_cg_texture_2d_nop_allocate(cg_texture_t *tex, cg_error_t **error)
{
    return true;
}

void
_cg_texture_2d_nop_flush_legacy_texobj_filters(cg_texture_t *tex,
                                               GLenum min_filter,
                                               GLenum mag_filter)
{
}

void
_cg_texture_2d_nop_flush_legacy_texobj_wrap_modes(cg_texture_t *tex,
                                                  GLenum wrap_mode_s,
                                                  GLenum wrap_mode_t,
                                                  GLenum wrap_mode_p)
{
}

void
_cg_texture_2d_nop_copy_from_framebuffer(cg_texture_2d_t *tex_2d,
                                         int src_x,
                                         int src_y,
                                         int width,
                                         int height,
                                         cg_framebuffer_t *src_fb,
                                         int dst_x,
                                         int dst_y,
                                         int level)
{
}

unsigned int
_cg_texture_2d_nop_get_gl_handle(cg_texture_2d_t *tex_2d)
{
    return 0;
}

void
_cg_texture_2d_nop_generate_mipmap(cg_texture_2d_t *tex_2d)
{
}

bool
_cg_texture_2d_nop_copy_from_bitmap(cg_texture_2d_t *tex_2d,
                                    int src_x,
                                    int src_y,
                                    int width,
                                    int height,
                                    cg_bitmap_t *bitmap,
                                    int dst_x,
                                    int dst_y,
                                    int level,
                                    cg_error_t **error)
{
    return true;
}

void
_cg_texture_2d_nop_get_data(cg_texture_2d_t *tex_2d,
                            cg_pixel_format_t format,
                            size_t rowstride,
                            uint8_t *data)
{
}
