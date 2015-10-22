/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010 Intel Corporation.
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
 *   Robert Bragg <robert@linux.intel.com>
 */

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef __CG_ATTRIBUTE_BUFFER_H__
#define __CG_ATTRIBUTE_BUFFER_H__

/* We forward declare the cg_attribute_buffer_t type here to avoid some circular
 * dependency issues with the following headers.
 */
typedef struct _cg_attribute_buffer_t cg_attribute_buffer_t;

#include <cglib/cg-device.h>

CG_BEGIN_DECLS

/**
 * SECTION:cg-attribute-buffer
 * @short_description: Functions for creating and manipulating attribute
 *   buffers
 *
 * FIXME
 */

#define CG_ATTRIBUTE_BUFFER(buffer) ((cg_attribute_buffer_t *)(buffer))

/**
 * cg_attribute_buffer_new_with_size:
 * @dev: A #cg_device_t
 * @bytes: The number of bytes to allocate for vertex attribute data.
 *
 * Describes a new #cg_attribute_buffer_t of @size bytes to contain
 * arrays of vertex attribute data. Afterwards data can be set using
 * cg_buffer_set_data() or by mapping it into the application's
 * address space using cg_buffer_map().
 *
 * The underlying storage of this buffer isn't allocated by this
 * function so that you have an opportunity to use the
 * cg_buffer_set_update_hint() and cg_buffer_set_usage_hint()
 * functions which may influence how the storage is allocated. The
 * storage will be allocated once you upload data to the buffer.
 *
 * Note: You can assume this function always succeeds and won't return
 * %NULL
 *
 * Return value: (transfer full): A newly allocated #cg_attribute_buffer_t.
 * Never %NULL.
 *
 * Stability: Unstable
 */
cg_attribute_buffer_t *cg_attribute_buffer_new_with_size(cg_device_t *dev,
                                                         size_t bytes);

/**
 * cg_attribute_buffer_new:
 * @dev: A #cg_device_t
 * @bytes: The number of bytes to allocate for vertex attribute data.
 * @data: (array length=bytes): An optional pointer to vertex data to
 *        upload immediately.
 *
 * Describes a new #cg_attribute_buffer_t of @size bytes to contain
 * arrays of vertex attribute data and also uploads @size bytes read
 * from @data to the new buffer.
 *
 * You should never pass a %NULL data pointer.
 *
 * <note>This function does not report out-of-memory errors back to
 * the caller by returning %NULL and so you can assume this function
 * always succeeds.</note>
 *
 * <note>In the unlikely case that there is an out of memory problem
 * then CGlib will abort the application with a message. If your
 * application needs to gracefully handle out-of-memory errors then
 * you can use cg_attribute_buffer_new_with_size() and then
 * explicitly catch errors with cg_buffer_set_data() or
 * cg_buffer_map().</note>
 *
 * Return value: (transfer full): A newly allocated #cg_attribute_buffer_t
 *(never %NULL)
 *
 * Stability: Unstable
 */
cg_attribute_buffer_t *
cg_attribute_buffer_new(cg_device_t *dev, size_t bytes, const void *data);

/**
 * cg_is_attribute_buffer:
 * @object: A #cg_object_t
 *
 * Gets whether the given object references a #cg_attribute_buffer_t.
 *
 * Returns: %true if @object references a #cg_attribute_buffer_t,
 *   %false otherwise
 *
 * Stability: Unstable
 */
bool cg_is_attribute_buffer(void *object);

CG_END_DECLS

#endif /* __CG_ATTRIBUTE_BUFFER_H__ */
