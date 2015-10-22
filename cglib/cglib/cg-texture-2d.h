/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011,2013 Intel Corporation.
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

#ifndef __CG_TEXTURE_2D_H
#define __CG_TEXTURE_2D_H

#include "cg-device.h"
#include "cg-bitmap.h"

CG_BEGIN_DECLS

/**
 * SECTION:cg-texture-2d
 * @short_description: Functions for creating and manipulating 2D textures
 *
 * These functions allow low-level 2D textures to be allocated. These
 * differ from sliced textures for example which may internally be
 * made up of multiple 2D textures, or atlas textures where CGlib must
 * internally modify user texture coordinates before they can be used
 * by the GPU.
 *
 * You should be aware that many GPUs only support power of two sizes
 * for #cg_texture_2d_t textures. You can check support for non power of
 * two textures by checking for the %CG_FEATURE_ID_TEXTURE_NPOT feature
 * via cg_has_feature().
 */

typedef struct _cg_texture_2d_t cg_texture_2d_t;
#define CG_TEXTURE_2D(X) ((cg_texture_2d_t *)X)

/**
 * cg_is_texture_2d:
 * @object: A #cg_object_t
 *
 * Gets whether the given object references an existing #cg_texture_2d_t
 * object.
 *
 * Return value: %true if the object references a #cg_texture_2d_t,
 *   %false otherwise
 */
bool cg_is_texture_2d(void *object);

/**
 * cg_texture_2d_new_with_size:
 * @dev: A #cg_device_t
 * @width: Width of the texture to allocate
 * @height: Height of the texture to allocate
 *
 * Creates a low-level #cg_texture_2d_t texture with a given @width and
 * @height that your GPU can texture from directly.
 *
 * The storage for the texture is not allocated before this function
 * returns. You can call cg_texture_allocate() to explicitly
 * allocate the underlying storage or preferably let CGlib
 * automatically allocate storage lazily when it may know more about
 * how the texture is being used and can optimize how it is allocated.
 *
 * The texture is still configurable until it has been allocated so
 * for example you can influence the internal format of the texture
 * using cg_texture_set_components() and
 * cg_texture_set_premultiplied().
 *
 * <note>Many GPUs only support power of two sizes for #cg_texture_2d_t
 * textures. You can check support for non power of two textures by
 * checking for the %CG_FEATURE_ID_TEXTURE_NPOT feature via
 * cg_has_feature().</note>
 *
 * Returns: (transfer full): A new #cg_texture_2d_t object with no storage yet
 * allocated.
 *
 */
cg_texture_2d_t *
cg_texture_2d_new_with_size(cg_device_t *dev, int width, int height);

/**
 * cg_texture_2d_new_from_file:
 * @dev: A #cg_device_t
 * @filename: the file to load
 * @error: A #cg_error_t to catch exceptional errors or %NULL
 *
 * Creates a low-level #cg_texture_2d_t texture from an image file.
 *
 * The storage for the texture is not allocated before this function
 * returns. You can call cg_texture_allocate() to explicitly
 * allocate the underlying storage or preferably let CGlib
 * automatically allocate storage lazily when it may know more about
 * how the texture is being used and can optimize how it is allocated.
 *
 * The texture is still configurable until it has been allocated so
 * for example you can influence the internal format of the texture
 * using cg_texture_set_components() and
 * cg_texture_set_premultiplied().
 *
 * <note>Many GPUs only support power of two sizes for #cg_texture_2d_t
 * textures. You can check support for non power of two textures by
 * checking for the %CG_FEATURE_ID_TEXTURE_NPOT feature via
 * cg_has_feature().</note>
 *
 * Return value: (transfer full): A newly created #cg_texture_2d_t or %NULL on
 * failure
 *               and @error will be updated.
 *
 */
cg_texture_2d_t *cg_texture_2d_new_from_file(cg_device_t *dev,
                                             const char *filename,
                                             cg_error_t **error);

/**
 * cg_texture_2d_new_from_data:
 * @dev: A #cg_device_t
 * @width: width of texture in pixels
 * @height: height of texture in pixels
 * @format: the #cg_pixel_format_t the buffer is stored in in RAM
 * @rowstride: the memory offset in bytes between the starts of
 *    scanlines in @data. A value of 0 will make CGlib automatically
 *    calculate @rowstride from @width and @format.
 * @data: pointer the memory region where the source buffer resides
 * @error: A #cg_error_t for exceptions
 *
 * Creates a low-level #cg_texture_2d_t texture based on data residing
 * in memory.
 *
 * <note>This api will always immediately allocate GPU memory for the
 * texture and upload the given data so that the @data pointer does
 * not need to remain valid once this function returns. This means it
 * is not possible to configure the texture before it is allocated. If
 * you do need to configure the texture before allocation (to specify
 * constraints on the internal format for example) then you can
 * instead create a #cg_bitmap_t for your data and use
 * cg_texture_2d_new_from_bitmap() or use
 * cg_texture_2d_new_with_size() and then upload data using
 * cg_texture_set_data()</note>
 *
 * <note>Many GPUs only support power of two sizes for #cg_texture_2d_t
 * textures. You can check support for non power of two textures by
 * checking for the %CG_FEATURE_ID_TEXTURE_NPOT feature via
 * cg_has_feature().</note>
 *
 * Returns: (transfer full): A newly allocated #cg_texture_2d_t, or if
 *          the size is not supported (because it is too large or a
 *          non-power-of-two size that the hardware doesn't support)
 *          it will return %NULL and set @error.
 *
 */
cg_texture_2d_t *cg_texture_2d_new_from_data(cg_device_t *dev,
                                             int width,
                                             int height,
                                             cg_pixel_format_t format,
                                             int rowstride,
                                             const uint8_t *data,
                                             cg_error_t **error);

/**
 * cg_texture_2d_new_from_bitmap:
 * @bitmap: A #cg_bitmap_t
 *
 * Creates a low-level #cg_texture_2d_t texture based on data residing
 * in a #cg_bitmap_t.
 *
 * The storage for the texture is not allocated before this function
 * returns. You can call cg_texture_allocate() to explicitly
 * allocate the underlying storage or preferably let CGlib
 * automatically allocate storage lazily when it may know more about
 * how the texture is being used and can optimize how it is allocated.
 *
 * The texture is still configurable until it has been allocated so
 * for example you can influence the internal format of the texture
 * using cg_texture_set_components() and
 * cg_texture_set_premultiplied().
 *
 * <note>Many GPUs only support power of two sizes for #cg_texture_2d_t
 * textures. You can check support for non power of two textures by
 * checking for the %CG_FEATURE_ID_TEXTURE_NPOT feature via
 * cg_has_feature().</note>
 *
 * Returns: (transfer full): A newly allocated #cg_texture_2d_t
 *
 * Stability: unstable
 */
cg_texture_2d_t *cg_texture_2d_new_from_bitmap(cg_bitmap_t *bitmap);

CG_END_DECLS

#endif /* __CG_TEXTURE_2D_H */
