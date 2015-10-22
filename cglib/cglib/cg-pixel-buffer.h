/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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
 *   Damien Lespiau <damien.lespiau@intel.com>
 *   Robert Bragg <robert@linux.intel.com>
 */

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef __CG_PIXEL_BUFFER_H__
#define __CG_PIXEL_BUFFER_H__

/* XXX: We forward declare cg_pixel_buffer_t here to allow for circular
 * dependencies between some headers */
typedef struct _cg_pixel_buffer_t cg_pixel_buffer_t;

#include <cglib/cg-types.h>
#include <cglib/cg-device.h>

CG_BEGIN_DECLS

#define CG_PIXEL_BUFFER(buffer) ((cg_pixel_buffer_t *)(buffer))

/**
 * cg_pixel_buffer_new:
 * @dev: A #cg_device_t
 * @size: The number of bytes to allocate for the pixel data.
 * @data: An optional pointer to vertex data to upload immediately
 * @error: A #cg_error_t for catching exceptional errors
 *
 * Declares a new #cg_pixel_buffer_t of @size bytes to contain arrays of
 * pixels. Once declared, data can be set using cg_buffer_set_data()
 * or by mapping it into the application's address space using
 * cg_buffer_map().
 *
 * If @data isn't %NULL then @size bytes will be read from @data and
 * immediately copied into the new buffer.
 *
 * Return value: (transfer full): a newly allocated #cg_pixel_buffer_t
 *
 * Stability: unstable
 */
cg_pixel_buffer_t *cg_pixel_buffer_new(cg_device_t *dev,
                                       size_t size,
                                       const void *data,
                                       cg_error_t **error);

/**
 * cg_is_pixel_buffer:
 * @object: a #cg_object_t to test
 *
 * Checks whether @object is a pixel buffer.
 *
 * Return value: %true if the @object is a pixel buffer, and %false
 *   otherwise
 *
 * Stability: Unstable
 */
bool cg_is_pixel_buffer(void *object);

#if 0
/*
 * cg_pixel_buffer_set_region:
 * @buffer: A #cg_pixel_buffer_t object
 * @data: pixel data to upload to @array
 * @src_width: width in pixels of the region to update
 * @src_height: height in pixels of the region to update
 * @src_rowstride: row stride in bytes of the source array
 * @dst_x: upper left destination horizontal coordinate
 * @dst_y: upper left destination vertical coordinate
 *
 * Uploads new data into a pixel array. The source data pointed by @data can
 * have a different stride than @array in which case the function will do the
 * right thing for you. For performance reasons, it is recommended for the
 * source data to have the same stride than @array.
 *
 * Return value: %true if the upload succeeded, %false otherwise
 *
 * Stability: Unstable
 */
bool
cg_pixel_buffer_set_region (cg_pixel_buffer_t *buffer,
                            uint8_t *data,
                            unsigned int src_width,
                            unsigned int src_height,
                            unsigned int src_rowstride,
                            unsigned int dst_x,
                            unsigned int dst_y);
#endif

CG_END_DECLS

#endif /* __CG_PIXEL_BUFFER_H__ */
