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

#include <cglib-config.h>

#include "cg-object-private.h"
#include "cg-attribute-buffer.h"
#include "cg-attribute-buffer-private.h"
#include "cg-device-private.h"

static void _cg_attribute_buffer_free(cg_attribute_buffer_t *array);

CG_BUFFER_DEFINE(AttributeBuffer, attribute_buffer);

cg_attribute_buffer_t *
cg_attribute_buffer_new_with_size(cg_device_t *dev,
                                  size_t bytes)
{
    cg_attribute_buffer_t *buffer = c_slice_new(cg_attribute_buffer_t);

    /* parent's constructor */
    _cg_buffer_initialize(CG_BUFFER(buffer),
                          dev,
                          bytes,
                          CG_BUFFER_BIND_TARGET_ATTRIBUTE_BUFFER,
                          CG_BUFFER_USAGE_HINT_ATTRIBUTE_BUFFER,
                          CG_BUFFER_UPDATE_HINT_STATIC);

    return _cg_attribute_buffer_object_new(buffer);
}

cg_attribute_buffer_t *
cg_attribute_buffer_new(cg_device_t *dev, size_t bytes, const void *data)
{
    cg_attribute_buffer_t *buffer;

    c_return_val_if_fail(data, NULL);

    buffer = cg_attribute_buffer_new_with_size(dev, bytes);

    /* Note: to keep the common cases simple this API doesn't throw
     * cg_error_ts, so developers can assume this function never returns
     * NULL and we will simply abort on error.
     *
     * Developers wanting to catch errors can use
     * cg_attribute_buffer_new_with_size() and catch errors when later
     * calling cg_buffer_set_data() or cg_buffer_map().
     */

    cg_buffer_set_data(CG_BUFFER(buffer), 0, data, bytes, NULL);

    return buffer;
}

static void
_cg_attribute_buffer_free(cg_attribute_buffer_t *array)
{
    /* parent's destructor */
    _cg_buffer_fini(CG_BUFFER(array));

    c_slice_free(cg_attribute_buffer_t, array);
}
