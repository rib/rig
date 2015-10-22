/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009,2010,2011,2012,2013 Intel Corporation.
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

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef __CG_TEXTURE_H__
#define __CG_TEXTURE_H__

/* We forward declare the cg_texture_t type here to avoid some circular
 * dependency issues with the following headers.
 */
#ifdef __CG_H_INSIDE__
/* For the public C api we typedef interface types as void to avoid needing
 * lots of casting in code and instead we will rely on runtime type checking
 * for these objects. */
typedef void cg_texture_t;
#else
typedef struct _cg_texture_t cg_texture_t;
#define CG_TEXTURE(X) ((cg_texture_t *)X)
#endif

#include <cglib/cg-types.h>
#include <cglib/cg-defines.h>
#include <cglib/cg-pixel-buffer.h>
#include <cglib/cg-bitmap.h>
#include <cglib/cg-framebuffer.h>

CG_BEGIN_DECLS

/**
 * SECTION:cg-texture
 * @short_description: Common interface for manipulating textures
 *
 * CGlib provides several different types of textures such as
 * #cg_texture_2d_t, #cg_texture_3d_t, #cg_texture_2d_sliced_t,
 * #cg_atlas_texture_t, #cg_sub_texture_t and #cg_texture_pixmap_x11_t that
 * each have specific apis for creating and manipulating them, but
 * there are a number of common operations that can be applied to any
 * of these texture types which are handled via this #cg_texture_t
 * interface.
 */

#define CG_TEXTURE_MAX_WASTE 127

/**
 * CG_TEXTURE_ERROR:
 *
 * #cg_error_t domain for texture errors.
 *
 * Stability: Unstable
 */
#define CG_TEXTURE_ERROR (cg_texture_error_domain())

/**
 * cg_texture_error_t:
 * @CG_TEXTURE_ERROR_SIZE: Unsupported size
 * @CG_TEXTURE_ERROR_FORMAT: Unsupported format
 * @CG_TEXTURE_ERROR_TYPE: A primitive texture type that is
 *   unsupported by the driver was used
 *
 * Error codes that can be thrown when allocating textures.
 *
 * Stability: Unstable
 */
typedef enum {
    CG_TEXTURE_ERROR_SIZE,
    CG_TEXTURE_ERROR_FORMAT,
    CG_TEXTURE_ERROR_BAD_PARAMETER,
    CG_TEXTURE_ERROR_TYPE
} cg_texture_error_t;

/**
 * cg_texture_type_t:
 * @CG_TEXTURE_TYPE_2D: A #cg_texture_2d_t
 * @CG_TEXTURE_TYPE_3D: A #cg_texture_3d_t
 *
 * Constants representing the underlying hardware texture type of a
 * #cg_texture_t.
 *
 * Stability: unstable
 */
typedef enum {
    CG_TEXTURE_TYPE_2D,
    CG_TEXTURE_TYPE_3D
} cg_texture_type_t;

uint32_t cg_texture_error_domain(void);

/**
 * cg_is_texture:
 * @object: A #cg_object_t pointer
 *
 * Gets whether the given object references a texture object.
 *
 * Return value: %true if the @object references a texture, and
 *   %false otherwise
 */
bool cg_is_texture(void *object);

/**
 * cg_texture_components_t:
 * @CG_TEXTURE_COMPONENTS_A: Only the alpha component
 * @CG_TEXTURE_COMPONENTS_RG: Red and green components. Note that
 *   this can only be used if the %CG_FEATURE_ID_TEXTURE_RG feature
 *   is advertised.
 * @CG_TEXTURE_COMPONENTS_RGB: Red, green and blue components
 * @CG_TEXTURE_COMPONENTS_RGBA: Red, green, blue and alpha components
 * @CG_TEXTURE_COMPONENTS_DEPTH: Only a depth component
 * @CG_TEXTURE_COMPONENTS_DEPTH_STENCIL: Depth and stencil components
 *
 * See cg_texture_set_components().
 *
 */
typedef enum _cg_texture_components_t {
    CG_TEXTURE_COMPONENTS_A = 1,
    CG_TEXTURE_COMPONENTS_A8,
    CG_TEXTURE_COMPONENTS_A8_SNORM,
    CG_TEXTURE_COMPONENTS_A16U,
    CG_TEXTURE_COMPONENTS_A16F,
    CG_TEXTURE_COMPONENTS_A32U,
    CG_TEXTURE_COMPONENTS_A32F,
    CG_TEXTURE_COMPONENTS_RG,
    CG_TEXTURE_COMPONENTS_RG8,
    CG_TEXTURE_COMPONENTS_RG8_SNORM,
    CG_TEXTURE_COMPONENTS_RG16U,
    CG_TEXTURE_COMPONENTS_RG16F,
    CG_TEXTURE_COMPONENTS_RG32U,
    CG_TEXTURE_COMPONENTS_RG32F,
    CG_TEXTURE_COMPONENTS_RGB,
    CG_TEXTURE_COMPONENTS_RGB8,
    CG_TEXTURE_COMPONENTS_RGB8_SNORM,
    CG_TEXTURE_COMPONENTS_RGB16U,
    CG_TEXTURE_COMPONENTS_RGB16F,
    CG_TEXTURE_COMPONENTS_RGB32U,
    CG_TEXTURE_COMPONENTS_RGB32F,
    CG_TEXTURE_COMPONENTS_RGBA,
    CG_TEXTURE_COMPONENTS_RGBA8,
    CG_TEXTURE_COMPONENTS_RGBA8_SNORM,
    CG_TEXTURE_COMPONENTS_RGBA16U,
    CG_TEXTURE_COMPONENTS_RGBA16F,
    CG_TEXTURE_COMPONENTS_RGBA32U,
    CG_TEXTURE_COMPONENTS_RGBA32F,
    CG_TEXTURE_COMPONENTS_DEPTH,
    CG_TEXTURE_COMPONENTS_DEPTH_STENCIL
} cg_texture_components_t;

/**
 * cg_texture_set_components:
 * @texture: a #cg_texture_t pointer.
 *
 * Affects the internal storage format for this texture by specifying
 * what components will be required for sampling later.
 *
 * This api affects how data is uploaded to the GPU since unused
 * components can potentially be discarded from source data.
 *
 * For textures created by the ‘_with_size’ constructors the default
 * is %CG_TEXTURE_COMPONENTS_RGBA. The other constructors which take
 * a %cg_bitmap_t or a data pointer default to the same components as
 * the pixel format of the data.
 *
 * Note that the %CG_TEXTURE_COMPONENTS_RG format is not available
 * on all drivers. The availability can be determined by checking for
 * the %CG_FEATURE_ID_TEXTURE_RG feature. If this format is used on
 * a driver where it is not available then %CG_TEXTURE_ERROR_FORMAT
 * will be raised when the texture is allocated. Even if the feature
 * is not available then %CG_PIXEL_FORMAT_RG_88 can still be used as
 * an image format as long as %CG_TEXTURE_COMPONENTS_RG isn't used
 * as the texture's components.
 *
 */
void cg_texture_set_components(cg_texture_t *texture,
                               cg_texture_components_t components);

/**
 * cg_texture_get_components:
 * @texture: a #cg_texture_t pointer.
 *
 * Queries what components the given @texture stores internally as set
 * via cg_texture_set_components().
 *
 * For textures created by the ‘_with_size’ constructors the default
 * is %CG_TEXTURE_COMPONENTS_RGBA. The other constructors which take
 * a %cg_bitmap_t or a data pointer default to the same components as
 * the pixel format of the data.
 *
 */
cg_texture_components_t cg_texture_get_components(cg_texture_t *texture);

/**
 * cg_texture_set_premultiplied:
 * @texture: a #cg_texture_t pointer.
 * @premultiplied: Whether any internally stored red, green or blue
 *                 components are pre-multiplied by an alpha
 *                 component.
 *
 * Affects the internal storage format for this texture by specifying
 * whether red, green and blue color components should be stored as
 * pre-multiplied alpha values.
 *
 * This api affects how data is uploaded to the GPU since CGlib will
 * convert source data to have premultiplied or unpremultiplied
 * components according to this state.
 *
 * For example if you create a texture via
 * cg_texture_2d_new_with_size() and then upload data via
 * cg_texture_set_data() passing a source format of
 * %CG_PIXEL_FORMAT_RGBA_8888 then CGlib will internally multiply the
 * red, green and blue components of the source data by the alpha
 * component, for each pixel so that the internally stored data has
 * pre-multiplied alpha components. If you instead upload data that
 * already has pre-multiplied components by passing
 * %CG_PIXEL_FORMAT_RGBA_8888_PRE as the source format to
 * cg_texture_set_data() then the data can be uploaded without being
 * converted.
 *
 * By default the @premultipled state is @true.
 *
 */
void cg_texture_set_premultiplied(cg_texture_t *texture, bool premultiplied);

/**
 * cg_texture_get_premultiplied:
 * @texture: a #cg_texture_t pointer.
 *
 * Queries the pre-multiplied alpha status for internally stored red,
 * green and blue components for the given @texture as set by
 * cg_texture_set_premultiplied().
 *
 * By default the pre-multipled state is @true.
 *
 * Return value: %true if red, green and blue components are
 *               internally stored pre-multiplied by the alpha
 *               value or %false if not.
 */
bool cg_texture_get_premultiplied(cg_texture_t *texture);

/**
 * cg_texture_get_width:
 * @texture: a #cg_texture_t pointer.
 *
 * Queries the width of a cg texture.
 *
 * Return value: the width of the GPU side texture in pixels
 */
int cg_texture_get_width(cg_texture_t *texture);

/**
 * cg_texture_get_height:
 * @texture: a #cg_texture_t pointer.
 *
 * Queries the height of a cg texture.
 *
 * Return value: the height of the GPU side texture in pixels
 */
int cg_texture_get_height(cg_texture_t *texture);

/**
 * cg_texture_is_sliced:
 * @texture: a #cg_texture_t pointer.
 *
 * Queries if a texture is sliced (stored as multiple GPU side tecture
 * objects).
 *
 * Return value: %true if the texture is sliced, %false if the texture
 *   is stored as a single GPU texture
 */
bool cg_texture_is_sliced(cg_texture_t *texture);

/**
 * cg_texture_get_gl_texture:
 * @texture: a #cg_texture_t pointer.
 * @out_gl_handle: (out) (allow-none): pointer to return location for the
 *   textures GL handle, or %NULL.
 * @out_gl_target: (out) (allow-none): pointer to return location for the
 *   GL target type, or %NULL.
 *
 * Queries the GL handles for a GPU side texture through its #cg_texture_t.
 *
 * If the texture is spliced the data for the first sub texture will be
 * queried.
 *
 * Return value: %true if the handle was successfully retrieved, %false
 *   if the handle was invalid
 */
bool cg_texture_get_gl_texture(cg_texture_t *texture,
                               unsigned int *out_gl_handle,
                               unsigned int *out_gl_target);

/**
 * cg_texture_get_data:
 * @texture: a #cg_texture_t pointer.
 * @format: the #cg_pixel_format_t to store the texture as.
 * @rowstride: the rowstride of @data in bytes or pass 0 to calculate
 *             from the bytes-per-pixel of @format multiplied by the
 *             @texture width.
 * @data: memory location to write the @texture's contents, or %NULL
 * to only query the data size through the return value.
 *
 * Copies the pixel data from a cg texture to system memory.
 *
 * <note>The rowstride should be the rowstride you want for the
 * destination @data buffer you don't need to try and calculate the
 * rowstride of the source texture</note>
 *
 * Return value: the size of the texture data in bytes
 */
int cg_texture_get_data(cg_texture_t *texture,
                        cg_pixel_format_t format,
                        unsigned int rowstride,
                        uint8_t *data);

/**
 * cg_texture_draw_and_read_to_bitmap:
 * @texture: The #cg_texture_t to read
 * @framebuffer: The intermediate framebuffer needed to render the
 *               texture
 * @target_bmp: The destination bitmap
 * @error: A #cg_error_t to catch any exceptional errors
 *
 * Only to be used in exceptional circumstances, this api reads back
 * the contents of a @texture by rendering it to the given
 * @framebuffer and reading back the resulting pixels to be stored in
 * @bitmap. If the @texture is larger than the given @framebuffer then
 * multiple renders will be done to read the texture back in chunks.
 *
 * Any viewport, projection or modelview matrix state associated with
 * @framebuffer will be saved and restored, but other state such as
 * the color mask state is ignored and may affect the result of
 * reading back the texture.
 *
 * This API should only be used in exceptional circumstances when
 * alternative apis such as cg_texture_get_data() have failed. For
 * example compressed textures can not be read back directly and so
 * a render is required if you want read back the image data. Ideally
 * applications should aim to avoid needing to read back textures in
 * the first place and perhaps only use this api for debugging
 * purposes.
 *
 * Stability: unstable
 */
bool cg_texture_draw_and_read_to_bitmap(cg_texture_t *texture,
                                        cg_framebuffer_t *framebuffer,
                                        cg_bitmap_t *target_bmp,
                                        cg_error_t **error);

/**
 * cg_texture_set_region:
 * @texture: a #cg_texture_t.
 * @width: width of the region to set.
 * @height: height of the region to set.
 * @format: the #cg_pixel_format_t used in the source @data buffer.
 * @rowstride: rowstride in bytes of the source @data buffer (computed
 *             from @width and @format if it equals 0)
 * @data: the source data, pointing to the first top-left pixel to set
 * @dst_x: upper left destination x coordinate.
 * @dst_y: upper left destination y coordinate.
 * @level: The mipmap level to update (Normally 0 for the largest,
 *         base image)
 * @error: A #cg_error_t to return exceptional errors
 *
 * Sets the pixels in a rectangular subregion of @texture from an
 * in-memory buffer containing pixel @data.
 *
 * @data should point to the first pixel to copy corresponding
 * to the top left of the region being set.
 *
 * The rowstride determines how many bytes between the first pixel of
 * a row of @data and the first pixel of the next row. If @rowstride
 * equals 0 then it will be automatically calculated from @width and
 * the bytes-per-pixel for the given @format.
 *
 * A mipmap @level of 0 corresponds to the largest, base image of a
 * texture and @level 1 is half the width and height of level 0. The
 * size of any level can be calculated from the size of the base
 * level as follows:
 *
 * |[
 *  width = MAX (1, floor (base_width / 2 ^ level));
 *  height = MAX (1, floor (base_height / 2 ^ level));
 * ]|
 *
 * Or more succinctly put using C:
 *
 * |[
 *  width = MAX (1, base_width >> level);
 *  height = MAX (1, base_height >> level);
 * ]|
 *
 * You can get the size of the base level using
 * cg_texture_get_width() and cg_texture_get_height().
 *
 * You can determine the number of mipmap levels for a given texture
 * like this:
 *
 * |[
 *  n_levels = 1 + floor (log2 (max_dimension));
 * ]|
 *
 * Or more succinctly in C using the fls() - "Find Last Set" - function:
 *
 * |[
 *  n_levels = fls (max_dimension);
 * ]|

 * Where %max_dimension is the larger of cg_texture_get_width() and
 * cg_texture_get_height().
 *
 * It is an error to pass a @level number >= the number of levels that
 * @texture can have according to the above calculation.
 *
 * <note>Since the storage for a #cg_texture_t is allocated lazily then
 * if the given @texture has not previously been allocated then this
 * api can return %false and throw an exceptional @error if there is
 * not enough memory to allocate storage for @texture.</note>
 *
 * Return value: %true if the subregion upload was successful, and
 *   %false otherwise
 */
bool cg_texture_set_region(cg_texture_t *texture,
                           int width,
                           int height,
                           cg_pixel_format_t format,
                           int rowstride,
                           const uint8_t *data,
                           int dst_x,
                           int dst_y,
                           int level,
                           cg_error_t **error);

/**
 * cg_texture_set_data:
 * @texture a #cg_texture_t.
 * @format: the #cg_pixel_format_t used in the source @data buffer.
 * @rowstride: rowstride of the source @data buffer (computed from
 *             the texture width and @format if it equals 0)
 * @data: the source data, pointing to the first top-left pixel to set
 * @level: The mipmap level to update (Normally 0 for the largest,
 *         base texture)
 * @error: A #cg_error_t to return exceptional errors
 *
 * Sets all the pixels for a given mipmap @level by copying the pixel
 * data pointed to by the @data argument into the given @texture.
 *
 * @data should point to the first pixel to copy corresponding
 * to the top left of the mipmap @level being set.
 *
 * If @rowstride equals 0 then it will be automatically calculated
 * from the width of the mipmap level and the bytes-per-pixel for the
 * given @format.
 *
 * A mipmap @level of 0 corresponds to the largest, base image of a
 * texture and @level 1 is half the width and height of level 0. If
 * dividing any dimension of the previous level by two results in a
 * fraction then round the number down (floor()), but clamp to 1
 * something like this:
 *
 * |[
 *  next_width = MAX (1, floor (prev_width));
 * ]|
 *
 * You can determine the number of mipmap levels for a given texture
 * like this:
 *
 * |[
 *  n_levels = 1 + floor (log2 (max_dimension));
 * ]|
 *
 * Where %max_dimension is the larger of cg_texture_get_width() and
 * cg_texture_get_height().
 *
 * It is an error to pass a @level number >= the number of levels that
 * @texture can have according to the above calculation.
 *
 * <note>Since the storage for a #cg_texture_t is allocated lazily then
 * if the given @texture has not previously been allocated then this
 * api can return %false and throw an exceptional @error if there is
 * not enough memory to allocate storage for @texture.</note>
 *
 * Return value: %true if the data upload was successful, and
 *               %false otherwise
 */
bool cg_texture_set_data(cg_texture_t *texture,
                         cg_pixel_format_t format,
                         int rowstride,
                         const uint8_t *data,
                         int level,
                         cg_error_t **error);

/**
 * cg_texture_set_region_from_bitmap:
 * @texture: a #cg_texture_t pointer
 * @src_x: upper left x coordinate of the region in the source bitmap.
 * @src_y: upper left y coordinate of the region in the source bitmap
 * @width: width of the region to copy. (Must be less than or equal to
 *         the bitmap width)
 * @height: height of the region to copy. (Must be less than or equal
 *          to the bitmap height)
 * @bitmap: The source bitmap to copy from
 * @dst_x: upper left destination x coordinate to copy to.
 * @dst_y: upper left destination y coordinate to copy to.
 * @error: A #cg_error_t to return exceptional errors or %NULL
 *
 * Copies a rectangular region from @bitmap to the position
 * (@dst_x, @dst_y) of the given destination @texture.
 *
 * The source region's top left coordinate is (@src_x, @src_y) within
 * the source @bitmap and the region is @width pixels wide and @height
 * pixels high.
 *
 * <note>The source region must not extend outside the bounds of the
 * source @bitmap.</note>
 *
 * Return value: %true if the subregion upload was successful, and
 *   %false otherwise
 *
 * Stability: unstable
 */
bool cg_texture_set_region_from_bitmap(cg_texture_t *texture,
                                       int src_x,
                                       int src_y,
                                       int width,
                                       int height,
                                       cg_bitmap_t *bitmap,
                                       int dst_x,
                                       int dst_y,
                                       int level,
                                       cg_error_t **error);

/**
 * cg_texture_allocate:
 * @texture: A #cg_texture_t
 * @error: A #cg_error_t to return exceptional errors or %NULL
 *
 * Explicitly allocates the storage for the given @texture which
 * allows you to be sure that there is enough memory for the
 * texture and if not then the error can be handled gracefully.
 *
 * <note>Normally applications don't need to use this api directly
 * since the texture will be implicitly allocated when data is set on
 * the texture, or if the texture is attached to a #cg_offscreen_t
 * framebuffer and rendered too.</note>
 *
 * Return value: %true if the texture was successfully allocated,
 *               otherwise %false and @error will be updated if it
 *               wasn't %NULL.
 */
bool cg_texture_allocate(cg_texture_t *texture, cg_error_t **error);

CG_END_DECLS

#endif /* __CG_TEXTURE_H__ */
