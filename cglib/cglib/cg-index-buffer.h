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

#ifndef __CG_INDEX_BUFFER_H__
#define __CG_INDEX_BUFFER_H__

#include <cglib/cg-device.h>

CG_BEGIN_DECLS

/**
 * SECTION:cg-index-buffer
 * @short_description: Functions for creating and manipulating vertex
 * indices.
 *
 * FIXME
 */

#define CG_INDEX_BUFFER(buffer) ((cg_index_buffer_t *)buffer)

typedef struct _cg_index_buffer_t cg_index_buffer_t;

/**
 * cg_index_buffer_new:
 * @dev: A #cg_device_t
 * @bytes: The number of bytes to allocate for vertex attribute data.
 *
 * Declares a new #cg_index_buffer_t of @size bytes to contain vertex
 * indices. Once declared, data can be set using
 * cg_buffer_set_data() or by mapping it into the application's
 * address space using cg_buffer_map().
 *
 * Return value: (transfer full): A newly allocated #cg_index_buffer_t
 *
 * Stability: Unstable
 */
cg_index_buffer_t *cg_index_buffer_new(cg_device_t *dev, size_t bytes);

/**
 * cg_is_index_buffer:
 * @object: A #cg_object_t
 *
 * Gets whether the given object references a #cg_index_buffer_t.
 *
 * Returns: %true if the @object references a #cg_index_buffer_t,
 *   %false otherwise
 *
 * Stability: Unstable
 */
bool cg_is_index_buffer(void *object);

CG_END_DECLS

#endif /* __CG_INDEX_BUFFER_H__ */
