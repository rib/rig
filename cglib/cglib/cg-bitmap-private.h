/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007 OpenedHand
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

#ifndef __CG_BITMAP_H
#define __CG_BITMAP_H

#include <clib.h>

#include "cg-object-private.h"
#include "cg-buffer.h"
#include "cg-bitmap.h"

#ifdef CG_HAS_ANDROID_SUPPORT
#include <android/asset_manager.h>
#endif

struct _cg_bitmap_t {
    cg_object_t _parent;

    /* Pointer back to the context that this bitmap was created with */
    cg_device_t *dev;

    cg_pixel_format_t format;
    int width;
    int height;
    int rowstride;

    uint8_t *data;

    bool mapped;
    bool bound;

    /* If this is non-null then 'data' is ignored and instead it is
       fetched from this shared bitmap. */
    cg_bitmap_t *shared_bmp;

    /* If this is non-null then 'data' is treated as an offset into the
       buffer and map will divert to mapping the buffer */
    cg_buffer_t *buffer;
};

/*
 * _cg_bitmap_new_with_malloc_buffer:
 * @dev: A #cg_device_t
 * @width: width of the bitmap in pixels
 * @height: height of the bitmap in pixels
 * @format: the format of the pixels the array will store
 * @error: A #cg_error_t for catching exceptional errors or %NULL
 *
 * This is equivalent to cg_bitmap_new_with_size() except that it
 * allocated the buffer using c_malloc() instead of creating a
 * #cg_pixel_buffer_t. The buffer will be automatically destroyed when
 * the bitmap is freed.
 *
 * Return value: a #cg_pixel_buffer_t representing the newly created array
 *
 * Stability: Unstable
 */
cg_bitmap_t *_cg_bitmap_new_with_malloc_buffer(cg_device_t *dev,
                                               int width,
                                               int height,
                                               cg_pixel_format_t format,
                                               cg_error_t **error);

/* The idea of this function is that it will create a bitmap that
   shares the actual data with another bitmap. This is needed for the
   atlas texture backend because it needs upload a bitmap to a sub
   texture but override the format so that it ignores the premult
   flag. */
cg_bitmap_t *_cg_bitmap_new_shared(cg_bitmap_t *shared_bmp,
                                   cg_pixel_format_t format,
                                   int width,
                                   int height,
                                   int rowstride);

cg_bitmap_t *_cg_bitmap_convert(cg_bitmap_t *bmp,
                                cg_pixel_format_t dst_format,
                                cg_error_t **error);

cg_bitmap_t *_cg_bitmap_convert_for_upload(cg_bitmap_t *src_bmp,
                                           cg_pixel_format_t internal_format,
                                           bool can_convert_in_place,
                                           cg_error_t **error);

bool _cg_bitmap_convert_into_bitmap(cg_bitmap_t *src_bmp,
                                    cg_bitmap_t *dst_bmp,
                                    cg_error_t **error);

cg_bitmap_t *_cg_bitmap_from_file(cg_device_t *dev,
                                  const char *filename,
                                  cg_error_t **error);

#ifdef CG_HAS_ANDROID_SUPPORT
cg_bitmap_t *_cg_android_bitmap_new_from_asset(cg_device_t *dev,
                                               AAssetManager *manager,
                                               const char *filename,
                                               cg_error_t **error);
#endif

bool _cg_bitmap_unpremult(cg_bitmap_t *dst_bmp, cg_error_t **error);

bool _cg_bitmap_premult(cg_bitmap_t *dst_bmp, cg_error_t **error);

bool _cg_bitmap_convert_premult_status(cg_bitmap_t *bmp,
                                       cg_pixel_format_t dst_format,
                                       cg_error_t **error);

bool _cg_bitmap_copy_subregion(cg_bitmap_t *src,
                               cg_bitmap_t *dst,
                               int src_x,
                               int src_y,
                               int dst_x,
                               int dst_y,
                               int width,
                               int height,
                               cg_error_t **error);

/* Creates a deep copy of the source bitmap */
cg_bitmap_t *_cg_bitmap_copy(cg_bitmap_t *src_bmp, cg_error_t **error);

bool
_cg_bitmap_get_size_from_file(const char *filename, int *width, int *height);

void _cg_bitmap_set_format(cg_bitmap_t *bitmap, cg_pixel_format_t format);

/* Maps the bitmap so that the pixels can be accessed directly or if
   the bitmap is just a memory bitmap then it just returns the pointer
   to memory. Note that the bitmap isn't guaranteed to allocated to
   the full size of rowstride*height so it is not safe to read up to
   the rowstride of the last row. This will be the case if the user
   uploads data using gdk_pixbuf_new_subpixbuf with a sub region
   containing the last row of the pixbuf because in that case the
   rowstride can be much larger than the width of the image */
uint8_t *_cg_bitmap_map(cg_bitmap_t *bitmap,
                        cg_buffer_access_t access,
                        cg_buffer_map_hint_t hints,
                        cg_error_t **error);

void _cg_bitmap_unmap(cg_bitmap_t *bitmap);

/* These two are replacements for map and unmap that should used when
 * the pointer is going to be passed to GL for pixel packing or
 * unpacking. The address might not be valid for reading if the bitmap
 * was created with new_from_buffer but it will however be good to
 * pass to glTexImage2D for example. The access should be READ for
 * unpacking and WRITE for packing. It can not be both
 *
 * TODO: split this bind/unbind functions out into a GL specific file
 */
uint8_t *_cg_bitmap_gl_bind(cg_bitmap_t *bitmap,
                            cg_buffer_access_t access,
                            cg_buffer_map_hint_t hints,
                            cg_error_t **error);

void _cg_bitmap_gl_unbind(cg_bitmap_t *bitmap);

cg_device_t *_cg_bitmap_get_context(cg_bitmap_t *bitmap);

#endif /* __CG_BITMAP_H */
