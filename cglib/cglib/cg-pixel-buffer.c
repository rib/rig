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
 *   Damien Lespiau <damien.lespiau@intel.com>
 *   Robert Bragg <robert@linux.intel.com>
 */

/* For an overview of the functionality implemented here, please see
 * cg-buffer-array.h, which contains the gtk-doc section overview for the
 * Pixel Buffers API.
 */

#include <cglib-config.h>

#include <stdio.h>
#include <string.h>
#include <clib.h>

#include "cg-private.h"
#include "cg-util.h"
#include "cg-device-private.h"
#include "cg-object.h"
#include "cg-pixel-buffer-private.h"
#include "cg-pixel-buffer.h"

/*
 * GL/GLES compatibility defines for the buffer API:
 */

#if defined(CG_HAS_GL_SUPPORT)

#ifndef GL_PIXEL_UNPACK_BUFFER
#define GL_PIXEL_UNPACK_BUFFER GL_PIXEL_UNPACK_BUFFER_ARB
#endif

#ifndef GL_PIXEL_PACK_BUFFER
#define GL_PIXEL_PACK_BUFFER GL_PIXEL_PACK_BUFFER_ARB
#endif

#endif

static void _cg_pixel_buffer_free(cg_pixel_buffer_t *buffer);

CG_BUFFER_DEFINE(PixelBuffer, pixel_buffer)

cg_pixel_buffer_t *cg_pixel_buffer_new(cg_device_t *dev,
                                       size_t size,
                                       const void *data,
                                       cg_error_t **error)
{
    cg_pixel_buffer_t *pixel_buffer = c_slice_new0(cg_pixel_buffer_t);
    cg_buffer_t *buffer = CG_BUFFER(pixel_buffer);

    /* parent's constructor */
    _cg_buffer_initialize(buffer,
                          dev,
                          size,
                          CG_BUFFER_BIND_TARGET_PIXEL_UNPACK,
                          CG_BUFFER_USAGE_HINT_TEXTURE,
                          CG_BUFFER_UPDATE_HINT_STATIC);

    _cg_pixel_buffer_object_new(pixel_buffer);

    if (data) {
        if (!cg_buffer_set_data(
                CG_BUFFER(pixel_buffer), 0, data, size, error)) {
            cg_object_unref(pixel_buffer);
            return NULL;
        }
    }

    return pixel_buffer;
}

static void
_cg_pixel_buffer_free(cg_pixel_buffer_t *buffer)
{
    /* parent's destructor */
    _cg_buffer_fini(CG_BUFFER(buffer));

    c_slice_free(cg_pixel_buffer_t, buffer);
}
