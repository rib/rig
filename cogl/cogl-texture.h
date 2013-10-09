/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2010,2011,2012,2013 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_TEXTURE_H__
#define __COGL_TEXTURE_H__

/* We forward declare the CoglTexture type here to avoid some circular
 * dependency issues with the following headers.
 */
#ifdef __COGL_H_INSIDE__
/* For the public C api we typedef interface types as void to avoid needing
 * lots of casting in code and instead we will rely on runtime type checking
 * for these objects. */
typedef void CoglTexture;
#else
typedef struct _CoglTexture CoglTexture;
#define COGL_TEXTURE(X) ((CoglTexture *)X)
#endif

#include <cogl/cogl-types.h>
#include <cogl/cogl-defines.h>
#include <cogl/cogl-pixel-buffer.h>
#include <cogl/cogl-bitmap.h>
#include <cogl/cogl-framebuffer.h>

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-texture
 * @short_description: Common interface for manipulating textures
 *
 * Cogl provides several different types of textures such as
 * #CoglTexture2D, #CoglTexture3D, #CoglTextureRectangle,
 * #CoglTexture2DSliced, #CoglAtlasTexture, #CoglSubTexture and
 * #CoglTexturePixmapX11 that each have specific apis for creating
 * and manipulating them, but there are a number of common operations
 * that can be applied to any of these texture types which are handled
 * via this #CoglTexture interface.
 */

#define COGL_TEXTURE_MAX_WASTE  127

/**
 * COGL_TEXTURE_ERROR:
 *
 * #CoglError domain for texture errors.
 *
 * Since: 2.0
 * Stability: Unstable
 */
#define COGL_TEXTURE_ERROR (cogl_texture_error_domain ())


/**
 * CoglTextureError:
 * @COGL_TEXTURE_ERROR_SIZE: Unsupported size
 *
 * Error codes that can be thrown when allocating textures.
 *
 * Since: 2.0
 * Stability: Unstable
 */
typedef enum {
  COGL_TEXTURE_ERROR_SIZE,
  COGL_TEXTURE_ERROR_FORMAT,
  COGL_TEXTURE_ERROR_BAD_PARAMETER,
  COGL_TEXTURE_ERROR_TYPE
} CoglTextureError;

/**
 * CoglTextureType:
 * @COGL_TEXTURE_TYPE_2D: A #CoglTexture2D
 * @COGL_TEXTURE_TYPE_3D: A #CoglTexture3D
 * @COGL_TEXTURE_TYPE_RECTANGLE: A #CoglTextureRectangle
 *
 * Constants representing the underlying hardware texture type of a
 * #CoglTexture.
 *
 * Stability: unstable
 * Since: 1.10
 */
typedef enum {
  COGL_TEXTURE_TYPE_2D,
  COGL_TEXTURE_TYPE_3D,
  COGL_TEXTURE_TYPE_RECTANGLE
} CoglTextureType;

uint32_t cogl_texture_error_domain (void);

/**
 * cogl_is_texture:
 * @object: A #CoglObject pointer
 *
 * Gets whether the given object references a texture object.
 *
 * Return value: %TRUE if the @object references a texture, and
 *   %FALSE otherwise
 */
CoglBool
cogl_is_texture (void *object);

/**
 * cogl_texture_get_width:
 * @texture: a #CoglTexture pointer.
 *
 * Queries the width of a cogl texture.
 *
 * Return value: the width of the GPU side texture in pixels
 */
unsigned int
cogl_texture_get_width (CoglTexture *texture);

/**
 * cogl_texture_get_height:
 * @texture: a #CoglTexture pointer.
 *
 * Queries the height of a cogl texture.
 *
 * Return value: the height of the GPU side texture in pixels
 */
unsigned int
cogl_texture_get_height (CoglTexture *texture);

/**
 * cogl_texture_get_format:
 * @texture: a #CoglTexture pointer.
 *
 * Queries the #CoglPixelFormat of a cogl texture.
 *
 * Return value: the #CoglPixelFormat of the GPU side texture
 */
CoglPixelFormat
cogl_texture_get_format (CoglTexture *texture);

/**
 * cogl_texture_is_sliced:
 * @texture: a #CoglTexture pointer.
 *
 * Queries if a texture is sliced (stored as multiple GPU side tecture
 * objects).
 *
 * Return value: %TRUE if the texture is sliced, %FALSE if the texture
 *   is stored as a single GPU texture
 */
CoglBool
cogl_texture_is_sliced (CoglTexture *texture);

/**
 * cogl_texture_get_gl_texture:
 * @texture: a #CoglTexture pointer.
 * @out_gl_handle: (out) (allow-none): pointer to return location for the
 *   textures GL handle, or %NULL.
 * @out_gl_target: (out) (allow-none): pointer to return location for the
 *   GL target type, or %NULL.
 *
 * Queries the GL handles for a GPU side texture through its #CoglTexture.
 *
 * If the texture is spliced the data for the first sub texture will be
 * queried.
 *
 * Return value: %TRUE if the handle was successfully retrieved, %FALSE
 *   if the handle was invalid
 */
CoglBool
cogl_texture_get_gl_texture (CoglTexture *texture,
                             unsigned int *out_gl_handle,
                             unsigned int *out_gl_target);

/**
 * cogl_texture_get_data:
 * @texture: a #CoglTexture pointer.
 * @format: the #CoglPixelFormat to store the texture as.
 * @rowstride: the rowstride of @data in bytes or pass 0 to calculate
 *             from the bytes-per-pixel of @format multiplied by the
 *             @texture width.
 * @data: memory location to write the @texture's contents, or %NULL
 * to only query the data size through the return value.
 *
 * Copies the pixel data from a cogl texture to system memory.
 *
 * <note>The rowstride should be the rowstride you want for the
 * destination @data buffer you don't need to try and calculate the
 * rowstride of the source texture</note>
 *
 * Return value: the size of the texture data in bytes
 */
int
cogl_texture_get_data (CoglTexture *texture,
                       CoglPixelFormat format,
                       unsigned int rowstride,
                       uint8_t *data);

/**
 * cogl_texture_draw_and_read_to_bitmap:
 * @texture: The #CoglTexture to read
 * @framebuffer: The intermediate framebuffer needed to render the
 *               texture
 * @target_bmp: The destination bitmap
 * @error: A #CoglError to catch any exceptional errors
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
 * alternative apis such as cogl_texture_get_data() have failed. For
 * example compressed textures can not be read back directly and so
 * a render is required if you want read back the image data. Ideally
 * applications should aim to avoid needing to read back textures in
 * the first place and perhaps only use this api for debugging
 * purposes.
 *
 * Stability: unstable
 */
CoglBool
cogl_texture_draw_and_read_to_bitmap (CoglTexture *texture,
                                      CoglFramebuffer *framebuffer,
                                      CoglBitmap *target_bmp,
                                      CoglError **error);

/**
 * cogl_texture_set_region:
 * @texture: a #CoglTexture.
 * @width: width of the region to set.
 * @height: height of the region to set.
 * @format: the #CoglPixelFormat used in the source @data buffer.
 * @rowstride: rowstride in bytes of the source @data buffer (computed
 *             from @width and @format if it equals 0)
 * @data: the source data, pointing to the first top-left pixel to set
 * @dst_x: upper left destination x coordinate.
 * @dst_y: upper left destination y coordinate.
 * @level: The mipmap level to update (Normally 0 for the largest,
 *         base image)
 * @error: A #CoglError to return exceptional errors
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
 * cogl_texture_get_width() and cogl_texture_get_height().
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

 * Where %max_dimension is the larger of cogl_texture_get_width() and
 * cogl_texture_get_height().
 *
 * It is an error to pass a @level number >= the number of levels that
 * @texture can have according to the above calculation.
 *
 * <note>Since the storage for a #CoglTexture is allocated lazily then
 * if the given @texture has not previously been allocated then this
 * api can return %FALSE and throw an exceptional @error if there is
 * not enough memory to allocate storage for @texture.</note>
 *
 * Return value: %TRUE if the subregion upload was successful, and
 *   %FALSE otherwise
 */
CoglBool
cogl_texture_set_region (CoglTexture *texture,
                         int width,
                         int height,
                         CoglPixelFormat format,
                         int rowstride,
                         const uint8_t *data,
			 int dst_x,
			 int dst_y,
                         int level,
                         CoglError **error);

/**
 * cogl_texture_set_data:
 * @texture a #CoglTexture.
 * @format: the #CoglPixelFormat used in the source @data buffer.
 * @rowstride: rowstride of the source @data buffer (computed from
 *             the texture width and @format if it equals 0)
 * @data: the source data, pointing to the first top-left pixel to set
 * @level: The mipmap level to update (Normally 0 for the largest,
 *         base texture)
 * @error: A #CoglError to return exceptional errors
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
 * Where %max_dimension is the larger of cogl_texture_get_width() and
 * cogl_texture_get_height().
 *
 * It is an error to pass a @level number >= the number of levels that
 * @texture can have according to the above calculation.
 *
 * <note>Since the storage for a #CoglTexture is allocated lazily then
 * if the given @texture has not previously been allocated then this
 * api can return %FALSE and throw an exceptional @error if there is
 * not enough memory to allocate storage for @texture.</note>
 *
 * Return value: %TRUE if the data upload was successful, and
 *               %FALSE otherwise
 */
CoglBool
cogl_texture_set_data (CoglTexture *texture,
                       CoglPixelFormat format,
                       int rowstride,
                       const uint8_t *data,
                       int level,
                       CoglError **error);

/**
 * cogl_texture_set_region_from_bitmap:
 * @texture: a #CoglTexture pointer
 * @src_x: upper left x coordinate of the region in the source bitmap.
 * @src_y: upper left y coordinate of the region in the source bitmap
 * @width: width of the region to copy. (Must be less than or equal to
 *         the bitmap width)
 * @height: height of the region to copy. (Must be less than or equal
 *          to the bitmap height)
 * @bitmap: The source bitmap to copy from
 * @dst_x: upper left destination x coordinate to copy to.
 * @dst_y: upper left destination y coordinate to copy to.
 * @error: A #CoglError to return exceptional errors or %NULL
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
 * Return value: %TRUE if the subregion upload was successful, and
 *   %FALSE otherwise
 *
 * Since: 1.8
 * Stability: unstable
 */
CoglBool
cogl_texture_set_region_from_bitmap (CoglTexture *texture,
                                     int src_x,
                                     int src_y,
                                     int width,
                                     int height,
                                     CoglBitmap *bitmap,
                                     int dst_x,
                                     int dst_y,
                                     int level,
                                     CoglError **error);

/**
 * cogl_texture_allocate:
 * @texture: A #CoglTexture
 * @error: A #CoglError to return exceptional errors or %NULL
 *
 * Explicitly allocates the storage for the given @texture which
 * allows you to be sure that there is enough memory for the
 * texture and if not then the error can be handled gracefully.
 *
 * <note>Normally applications don't need to use this api directly
 * since the texture will be implicitly allocated when data is set on
 * the texture, or if the texture is attached to a #CoglOffscreen
 * framebuffer and rendered too.</note>
 *
 * Return value: %TRUE if the texture was successfully allocated,
 *               otherwise %FALSE and @error will be updated if it
 *               wasn't %NULL.
 */
CoglBool
cogl_texture_allocate (CoglTexture *texture,
                       CoglError **error);

COGL_END_DECLS

#endif /* __COGL_TEXTURE_H__ */
