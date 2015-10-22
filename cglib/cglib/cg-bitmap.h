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
 */

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef __CG_BITMAP_H__
#define __CG_BITMAP_H__

/* XXX: We forward declare cg_bitmap_t here to allow for circular
 * dependencies between some headers */
typedef struct _cg_bitmap_t cg_bitmap_t;

#include <cglib/cg-types.h>
#include <cglib/cg-buffer.h>
#include <cglib/cg-device.h>
#include <cglib/cg-pixel-buffer.h>

#ifdef CG_HAS_ANDROID_SUPPORT
#include <android/asset_manager.h>
#endif

CG_BEGIN_DECLS

/**
 * SECTION:cg-bitmap
 * @short_description: Functions for loading images
 *
 * CGlib allows loading image data into memory as cg_bitmap_ts without
 * loading them immediately into GPU textures.
 *
 * #cg_bitmap_t is available since CGlib 1.0
 */

/**
 * cg_bitmap_new_from_file:
 * @dev: A #cg_device_t
 * @filename: the file to load.
 * @error: a #cg_error_t or %NULL.
 *
 * Loads an image file from disk. This function can be safely called from
 * within a thread.
 *
 * Return value: (transfer full): a #cg_bitmap_t to the new loaded
 *               image data, or %NULL if loading the image failed.
 *
 */
cg_bitmap_t *cg_bitmap_new_from_file(cg_device_t *dev,
                                     const char *filename,
                                     cg_error_t **error);

#ifdef CG_HAS_ANDROID_SUPPORT
/**
 * cg_android_bitmap_new_from_asset:
 * @dev: A #cg_device_t
 * @manager: An Android Asset Manager.
 * @filename: The file name for the asset
 * @error: A return location for a cg_error_t exception.
 *
 * Loads an Android asset into a newly allocated #cg_bitmap_t.
 *
 * Return value: (transfer full): A newly allocated #cg_bitmap_t
 *               holding the image data of the specified asset.
 *
 */
cg_bitmap_t *cg_android_bitmap_new_from_asset(cg_device_t *dev,
                                              AAssetManager *manager,
                                              const char *filename,
                                              cg_error_t **error);
#endif

/**
 * cg_bitmap_new_from_buffer:
 * @buffer: A #cg_buffer_t containing image data
 * @format: The #cg_pixel_format_t defining the format of the image data
 *          in the given @buffer.
 * @width: The width of the image data in the given @buffer.
 * @height: The height of the image data in the given @buffer.
 * @rowstride: The rowstride in bytes of the image data in the given @buffer.
 * @offset: The offset into the given @buffer to the first pixel that
 *          should be considered part of the #cg_bitmap_t.
 *
 * Wraps some image data that has been uploaded into a #cg_buffer_t as
 * a #cg_bitmap_t. The data is not copied in this process.
 *
 * Return value: (transfer full): a #cg_bitmap_t encapsulating the given
 *@buffer.
 *
 * Stability: unstable
 */
cg_bitmap_t *cg_bitmap_new_from_buffer(cg_buffer_t *buffer,
                                       cg_pixel_format_t format,
                                       int width,
                                       int height,
                                       int rowstride,
                                       int offset);

/**
 * cg_bitmap_new_with_size:
 * @dev: A #cg_device_t
 * @width: width of the bitmap in pixels
 * @height: height of the bitmap in pixels
 * @format: the format of the pixels the array will store
 *
 * Creates a new #cg_bitmap_t with the given width, height and format.
 * The initial contents of the bitmap are undefined.
 *
 * The data for the bitmap will be stored in a newly created
 * #cg_pixel_buffer_t. You can get a pointer to the pixel buffer using
 * cg_bitmap_get_buffer(). The #cg_buffer_t API can then be
 * used to fill the bitmap with data.
 *
 * <note>CGlib will try its best to provide a hardware array you can
 * map, write into and effectively do a zero copy upload when creating
 * a texture from it with cg_texture_new_from_bitmap(). For various
 * reasons, such arrays are likely to have a stride larger than width
 * * bytes_per_pixel. The user must take the stride into account when
 * writing into it. The stride can be retrieved with
 * cg_bitmap_get_rowstride().</note>
 *
 * Return value: (transfer full): a #cg_pixel_buffer_t representing the
 *               newly created array or %NULL on failure
 *
 * Stability: Unstable
 */
cg_bitmap_t *cg_bitmap_new_with_size(cg_device_t *dev,
                                     int width,
                                     int height,
                                     cg_pixel_format_t format);

/**
 * cg_bitmap_new_for_data:
 * @dev: A #cg_device_t
 * @width: The width of the bitmap.
 * @height: The height of the bitmap.
 * @format: The format of the pixel data.
 * @rowstride: The rowstride of the bitmap (the number of bytes from
 *   the start of one row of the bitmap to the next).
 * @data: A pointer to the data. The bitmap will take ownership of this data.
 *
 * Creates a bitmap using some existing data. The data is not copied
 * so the application must keep the buffer alive for the lifetime of
 * the #cg_bitmap_t. This can be used for example with
 * cg_framebuffer_read_pixels_into_bitmap() to read data directly
 * into an application buffer with the specified rowstride.
 *
 * Return value: (transfer full): A new #cg_bitmap_t.
 * Stability: unstable
 */
cg_bitmap_t *cg_bitmap_new_for_data(cg_device_t *dev,
                                    int width,
                                    int height,
                                    cg_pixel_format_t format,
                                    int rowstride,
                                    uint8_t *data);

/**
 * cg_bitmap_get_format:
 * @bitmap: A #cg_bitmap_t
 *
 * Return value: the #cg_pixel_format_t that the data for the bitmap is in.
 * Stability: unstable
 */
cg_pixel_format_t cg_bitmap_get_format(cg_bitmap_t *bitmap);

/**
 * cg_bitmap_get_width:
 * @bitmap: A #cg_bitmap_t
 *
 * Return value: the width of the bitmap
 * Stability: unstable
 */
int cg_bitmap_get_width(cg_bitmap_t *bitmap);

/**
 * cg_bitmap_get_height:
 * @bitmap: A #cg_bitmap_t
 *
 * Return value: the height of the bitmap
 * Stability: unstable
 */
int cg_bitmap_get_height(cg_bitmap_t *bitmap);

/**
 * cg_bitmap_get_rowstride:
 * @bitmap: A #cg_bitmap_t
 *
 * Return value: the rowstride of the bitmap. This is the number of
 *   bytes between the address of start of one row to the address of the
 *   next row in the image.
 * Stability: unstable
 */
int cg_bitmap_get_rowstride(cg_bitmap_t *bitmap);

/**
 * cg_bitmap_get_buffer:
 * @bitmap: A #cg_bitmap_t
 *
 * Return value: (transfer none): the #cg_pixel_buffer_t that this
 *   buffer uses for storage. Note that if the bitmap was created with
 *   cg_bitmap_new_from_file() then it will not actually be using a
 *   pixel buffer and this function will return %NULL.
 * Stability: unstable
 */
cg_pixel_buffer_t *cg_bitmap_get_buffer(cg_bitmap_t *bitmap);

/**
 * cg_bitmap_get_size_from_file:
 * @filename: the file to check
 * @width: (out): return location for the bitmap width, or %NULL
 * @height: (out): return location for the bitmap height, or %NULL
 *
 * Parses an image file enough to extract the width and height
 * of the bitmap.
 *
 * Return value: %true if the image was successfully parsed
 *
 */
bool
cg_bitmap_get_size_from_file(const char *filename, int *width, int *height);

/**
 * cg_is_bitmap:
 * @object: a #cg_object_t pointer
 *
 * Checks whether @object is a #cg_bitmap_t
 *
 * Return value: %true if the passed @object represents a bitmap,
 *   and %false otherwise
 *
 */
bool cg_is_bitmap(void *object);

/**
 * CG_BITMAP_ERROR:
 *
 * #cg_error_t domain for bitmap errors.
 *
 */
#define CG_BITMAP_ERROR (cg_bitmap_error_domain())

/**
 * cg_bitmap_error_t:
 * @CG_BITMAP_ERROR_FAILED: Generic failure code, something went
 *   wrong.
 * @CG_BITMAP_ERROR_UNKNOWN_TYPE: Unknown image type.
 * @CG_BITMAP_ERROR_CORRUPT_IMAGE: An image file was broken somehow.
 *
 * Error codes that can be thrown when performing bitmap
 * operations. Note that gdk_pixbuf_new_from_file() can also throw
 * errors directly from the underlying image loading library. For
 * example, if GdkPixbuf is used then errors #GdkPixbufError<!-- -->s
 * will be used directly.
 *
 */
typedef enum {
    CG_BITMAP_ERROR_FAILED,
    CG_BITMAP_ERROR_UNKNOWN_TYPE,
    CG_BITMAP_ERROR_CORRUPT_IMAGE
} cg_bitmap_error_t;

uint32_t cg_bitmap_error_domain(void);

CG_END_DECLS

#endif /* __CG_BITMAP_H__ */
