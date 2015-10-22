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
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __CG_TEXURE_2D_SLICED_H
#define __CG_TEXURE_2D_SLICED_H

#include "cg-device.h"
#include "cg-types.h"

/**
 * SECTION:cg-texture-2d-sliced
 * @short_description: Functions for creating and manipulating 2D meta
 *                     textures that may internally be comprised of
 *                     multiple 2D textures with power-of-two sizes.
 *
 * These functions allow high-level meta textures (See the
 * #cg_meta_texture_t interface) to be allocated that may internally be
 * comprised of multiple 2D texture "slices" with power-of-two sizes.
 *
 * This API can be useful when working with GPUs that don't have
 * native support for non-power-of-two textures or if you want to load
 * a texture that is larger than the GPUs maximum texture size limits.
 *
 * The algorithm for slicing works by first trying to map a virtual
 * size to the next larger power-of-two size and then seeing how many
 * wasted pixels that would result in. For example if you have a
 * virtual texture that's 259 texels wide, the next pot size = 512 and
 * the amount of waste would be 253 texels. If the amount of waste is
 * above a max-waste threshold then we would next slice that texture
 * into one that's 256 texels and then looking at how many more texels
 * remain unallocated after that we choose the next power-of-two size.
 * For the example of a 259 texel image that would mean having a 256
 * texel wide texture, leaving 3 texels unallocated so we'd then
 * create a 4 texel wide texture - now there is only one texel of
 * waste. The algorithm continues to slice the right most textures
 * until the amount of waste is less than or equal to a specfied
 * max-waste threshold. The same logic for slicing from left to right
 * is also applied from top to bottom.
 */

typedef struct _cg_texture_2d_sliced_t cg_texture_2d_sliced_t;
#define CG_TEXTURE_2D_SLICED(X) ((cg_texture_2d_sliced_t *)X)

/**
 * cg_texture_2d_sliced_new_with_size:
 * @dev: A #cg_device_t
 * @width: The virtual width of your sliced texture.
 * @height: The virtual height of your sliced texture.
 * @max_waste: The threshold of how wide a strip of wasted texels
 *             are allowed along the right and bottom textures before
 *             they must be sliced to reduce the amount of waste. A
 *             negative can be passed to disable slicing.
 *
 * Creates a #cg_texture_2d_sliced_t that may internally be comprised of
 * 1 or more #cg_texture_2d_t textures depending on GPU limitations.
 * For example if the GPU only supports power-of-two sized textures
 * then a sliced texture will turn a non-power-of-two size into a
 * combination of smaller power-of-two sized textures. If the
 * requested texture size is larger than is supported by the hardware
 * then the texture will be sliced into smaller textures that can be
 * accessed by the hardware.
 *
 * @max_waste is used as a threshold for recursively slicing the
 * right-most or bottom-most slices into smaller sizes until the
 * wasted padding at the bottom and right of the textures is less than
 * specified. A negative @max_waste will disable slicing.
 *
 * The storage for the texture is not allocated before this function
 * returns. You can call cg_texture_allocate() to explicitly
 * allocate the underlying storage or let CGlib automatically allocate
 * storage lazily.
 *
 * <note>It's possible for the allocation of a sliced texture to fail
 * later due to impossible slicing constraints if a negative
 * @max_waste value is given. If the given virtual texture size size
 * is larger than is supported by the hardware but slicing is disabled
 * the texture size would be too large to handle.</note>
 *
 * Returns: (transfer full): A new #cg_texture_2d_sliced_t object with no
 * storage
 *          allocated yet.
 *
 * Stability: unstable
 */
cg_texture_2d_sliced_t *cg_texture_2d_sliced_new_with_size(cg_device_t *dev,
                                                           int width,
                                                           int height,
                                                           int max_waste);

/**
 * cg_texture_2d_sliced_new_from_file:
 * @dev: A #cg_device_t
 * @filename: the file to load
 * @max_waste: The threshold of how wide a strip of wasted texels
 *             are allowed along the right and bottom textures before
 *             they must be sliced to reduce the amount of waste. A
 *             negative can be passed to disable slicing.
 * @error: A #cg_error_t to catch exceptional errors or %NULL
 *
 * Creates a #cg_texture_2d_sliced_t from an image file.
 *
 * A #cg_texture_2d_sliced_t may internally be comprised of 1 or more
 * #cg_texture_2d_t textures depending on GPU limitations.  For example
 * if the GPU only supports power-of-two sized textures then a sliced
 * texture will turn a non-power-of-two size into a combination of
 * smaller power-of-two sized textures. If the requested texture size
 * is larger than is supported by the hardware then the texture will
 * be sliced into smaller textures that can be accessed by the
 * hardware.
 *
 * @max_waste is used as a threshold for recursively slicing the
 * right-most or bottom-most slices into smaller sizes until the
 * wasted padding at the bottom and right of the textures is less than
 * specified. A negative @max_waste will disable slicing.
 *
 * The storage for the texture is not allocated before this function
 * returns. You can call cg_texture_allocate() to explicitly
 * allocate the underlying storage or let CGlib automatically allocate
 * storage lazily.
 *
 * <note>It's possible for the allocation of a sliced texture to fail
 * later due to impossible slicing constraints if a negative
 * @max_waste value is given. If the given virtual texture size is
 * larger than is supported by the hardware but slicing is disabled
 * the texture size would be too large to handle.</note>
 *
 * Return value: (transfer full): A newly created #cg_texture_2d_sliced_t
 *               or %NULL on failure and @error will be updated.
 *
 */
cg_texture_2d_sliced_t *cg_texture_2d_sliced_new_from_file(cg_device_t *dev,
                                                           const char *filename,
                                                           int max_waste,
                                                           cg_error_t **error);

/**
 * cg_texture_2d_sliced_new_from_data:
 * @dev: A #cg_device_t
 * @width: width of texture in pixels
 * @height: height of texture in pixels
 * @format: the #cg_pixel_format_t the buffer is stored in in RAM
 * @max_waste: The threshold of how wide a strip of wasted texels
 *             are allowed along the right and bottom textures before
 *             they must be sliced to reduce the amount of waste. A
 *             negative can be passed to disable slicing.
 * @rowstride: the memory offset in bytes between the start of each
 *    row in @data. A value of 0 will make CGlib automatically
 *    calculate @rowstride from @width and @format.
 * @data: pointer the memory region where the source buffer resides
 * @error: A #cg_error_t to catch exceptional errors or %NULL
 *
 * Creates a new #cg_texture_2d_sliced_t texture based on data residing
 * in memory.
 *
 * A #cg_texture_2d_sliced_t may internally be comprised of 1 or more
 * #cg_texture_2d_t textures depending on GPU limitations.  For example
 * if the GPU only supports power-of-two sized textures then a sliced
 * texture will turn a non-power-of-two size into a combination of
 * smaller power-of-two sized textures. If the requested texture size
 * is larger than is supported by the hardware then the texture will
 * be sliced into smaller textures that can be accessed by the
 * hardware.
 *
 * @max_waste is used as a threshold for recursively slicing the
 * right-most or bottom-most slices into smaller sizes until the
 * wasted padding at the bottom and right of the textures is less than
 * specified. A negative @max_waste will disable slicing.
 *
 * <note>This api will always immediately allocate GPU memory for all
 * the required texture slices and upload the given data so that the
 * @data pointer does not need to remain valid once this function
 * returns. This means it is not possible to configure the texture
 * before it is allocated. If you do need to configure the texture
 * before allocation (to specify constraints on the internal format
 * for example) then you can instead create a #cg_bitmap_t for your
 * data and use cg_texture_2d_sliced_new_from_bitmap() or use
 * cg_texture_2d_sliced_new_with_size() and then upload data using
 * cg_texture_set_data()</note>
 *
 * <note>It's possible for the allocation of a sliced texture to fail
 * due to impossible slicing constraints if a negative @max_waste
 * value is given. If the given virtual texture size is larger than is
 * supported by the hardware but slicing is disabled the texture size
 * would be too large to handle.</note>
 *
 * Return value: (transfer full): A newly created #cg_texture_2d_sliced_t
 *               or %NULL on failure and @error will be updated.
 *
 */
cg_texture_2d_sliced_t *
cg_texture_2d_sliced_new_from_data(cg_device_t *dev,
                                   int width,
                                   int height,
                                   int max_waste,
                                   cg_pixel_format_t format,
                                   int rowstride,
                                   const uint8_t *data,
                                   cg_error_t **error);

/**
 * cg_texture_2d_sliced_new_from_bitmap:
 * @bmp: A #cg_bitmap_t
 * @max_waste: The threshold of how wide a strip of wasted texels
 *             are allowed along the right and bottom textures before
 *             they must be sliced to reduce the amount of waste. A
 *             negative can be passed to disable slicing.
 *
 * Creates a new #cg_texture_2d_sliced_t texture based on data residing
 * in a bitmap.
 *
 * A #cg_texture_2d_sliced_t may internally be comprised of 1 or more
 * #cg_texture_2d_t textures depending on GPU limitations.  For example
 * if the GPU only supports power-of-two sized textures then a sliced
 * texture will turn a non-power-of-two size into a combination of
 * smaller power-of-two sized textures. If the requested texture size
 * is larger than is supported by the hardware then the texture will
 * be sliced into smaller textures that can be accessed by the
 * hardware.
 *
 * @max_waste is used as a threshold for recursively slicing the
 * right-most or bottom-most slices into smaller sizes until the
 * wasted padding at the bottom and right of the textures is less than
 * specified. A negative @max_waste will disable slicing.
 *
 * The storage for the texture is not allocated before this function
 * returns. You can call cg_texture_allocate() to explicitly
 * allocate the underlying storage or let CGlib automatically allocate
 * storage lazily.
 *
 * <note>It's possible for the allocation of a sliced texture to fail
 * later due to impossible slicing constraints if a negative
 * @max_waste value is given. If the given virtual texture size is
 * larger than is supported by the hardware but slicing is disabled
 * the texture size would be too large to handle.</note>
 *
 * Return value: (transfer full): A newly created #cg_texture_2d_sliced_t
 *               or %NULL on failure and @error will be updated.
 *
 */
cg_texture_2d_sliced_t *cg_texture_2d_sliced_new_from_bitmap(cg_bitmap_t *bmp,
                                                             int max_waste);

/**
 * cg_is_texture_2d_sliced:
 * @object: A #cg_object_t pointer
 *
 * Gets whether the given object references a #cg_texture_2d_sliced_t.
 *
 * Return value: %true if the object references a #cg_texture_2d_sliced_t
 *   and %false otherwise.
 * Stability: unstable
 */
bool cg_is_texture_2d_sliced(void *object);

#endif /* __CG_TEXURE_2D_SLICED_H */
