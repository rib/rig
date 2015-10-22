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
#include "cg-indices.h"
#include "cg-indices-private.h"
#include "cg-device-private.h"

static void _cg_index_buffer_free(cg_index_buffer_t *indices);

CG_BUFFER_DEFINE(IndexBuffer, index_buffer);

/* XXX: Unlike the wiki design this just takes a size. A single
 * indices buffer should be able to contain multiple ranges of indices
 * which the wiki design doesn't currently consider. */
cg_index_buffer_t *
cg_index_buffer_new(cg_device_t *dev, size_t bytes)
{
    cg_index_buffer_t *indices = c_slice_new(cg_index_buffer_t);

    /* parent's constructor */
    _cg_buffer_initialize(CG_BUFFER(indices),
                          dev,
                          bytes,
                          CG_BUFFER_BIND_TARGET_INDEX_BUFFER,
                          CG_BUFFER_USAGE_HINT_INDEX_BUFFER,
                          CG_BUFFER_UPDATE_HINT_STATIC);

    return _cg_index_buffer_object_new(indices);
}

static void
_cg_index_buffer_free(cg_index_buffer_t *indices)
{
    /* parent's destructor */
    _cg_buffer_fini(CG_BUFFER(indices));

    c_slice_free(cg_index_buffer_t, indices);
}

/* XXX: do we want a convenience function like this as an alternative
 * to using cg_buffer_set_data? The advantage of this is that we can
 * track meta data such as the indices type and max_index_value for a
 * range as part of the indices buffer. If we just leave people to use
 * cg_buffer_set_data then we either need a way to specify the type
 * and max index value at draw time or we'll want a separate way to
 * declare the type and max value for a range after uploading the
 * data.
 *
 * XXX: I think in the end it'll be that cg_indices_t are to
 * cg_index_buffer_ts as cg_attribute_ts are to cg_attribute_buffer_ts. I.e
 * a cg_index_buffer_t is a lite subclass of cg_buffer_t that simply
 * implies that the buffer will later be bound as indices but doesn't
 * track more detailed meta data. cg_indices_t build on a
 * cg_index_buffer_t and define the type and max_index_value for some
 * sub-range of a cg_index_buffer_t.
 */
#if 0
void
cg_index_buffer_set_data (cg_index_buffer_t *indices,
                          cg_indices_type_t type,
                          int max_index_value,
                          size_t write_offset,
                          void *user_indices,
                          int n_indices)
{
    c_llist_t *l;

    for (l = indices->ranges; l; l = l->next)
    {

    }
    cg_buffer_set
}
#endif
