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
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef _CG_TEXTURE_2D_GL_H_
#define _CG_TEXTURE_2D_GL_H_

#include "cg-device.h"
#include "cg-texture-2d.h"

CG_BEGIN_DECLS

/**
 * cg_texture_2d_gl_new_from_foreign:
 * @dev: A #cg_device_t
 * @gl_handle: A GL handle for a GL_TEXTURE_2D texture object
 * @width: Width of the foreign GL texture
 * @height: Height of the foreign GL texture
 * @format: The format of the texture
 *
 * Wraps an existing GL_TEXTURE_2D texture object as a #cg_texture_2d_t.
 * This can be used for integrating CGlib with software using OpenGL
 * directly.
 *
 * The texture is still configurable until it has been allocated so
 * for example you can declare whether the texture is premultiplied
 * with cg_texture_set_premultiplied().
 *
 * <note>The results are undefined for passing an invalid @gl_handle
 * or if @width or @height don't have the correct texture
 * geometry.</note>
 *
 * Returns: (transfer full): A newly allocated #cg_texture_2d_t
 *
 */
cg_texture_2d_t *cg_texture_2d_gl_new_from_foreign(cg_device_t *dev,
                                                   unsigned int gl_handle,
                                                   int width,
                                                   int height,
                                                   cg_pixel_format_t format);

CG_END_DECLS

#endif /* _CG_TEXTURE_2D_GL_H_ */
