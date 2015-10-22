/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Intel Corporation.
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

#include <cglib-config.h>

#include <string.h>

#include "cg-private.h"
#include "cg-device-private.h"
#include "cg-feature-private.h"
#include "cg-renderer-private.h"
#include "cg-error-private.h"
#include "cg-framebuffer-nop-private.h"
#include "cg-texture-2d-nop-private.h"
#include "cg-attribute-nop-private.h"
#include "cg-clip-stack-nop-private.h"

static bool
_cg_driver_update_features(cg_device_t *dev, cg_error_t **error)
{
    /* _cg_gpc_info_init (dev, &dev->gpu); */

    memset(dev->private_features, 0, sizeof(dev->private_features));

    return true;
}

const cg_driver_vtable_t _cg_driver_nop = {
    NULL, /* pixel_format_from_gl_internal */
    NULL, /* pixel_format_to_gl */
    _cg_driver_update_features,
    _cg_offscreen_nop_allocate,
    _cg_offscreen_nop_free,
    _cg_framebuffer_nop_flush_state,
    _cg_framebuffer_nop_clear,
    _cg_framebuffer_nop_query_bits,
    _cg_framebuffer_nop_finish,
    _cg_framebuffer_nop_discard_buffers,
    _cg_framebuffer_nop_draw_attributes,
    _cg_framebuffer_nop_draw_indexed_attributes,
    _cg_framebuffer_nop_read_pixels_into_bitmap,
    _cg_texture_2d_nop_free,
    _cg_texture_2d_nop_can_create,
    _cg_texture_2d_nop_init,
    _cg_texture_2d_nop_allocate,
    _cg_texture_2d_nop_copy_from_framebuffer,
    _cg_texture_2d_nop_get_gl_handle,
    _cg_texture_2d_nop_generate_mipmap,
    _cg_texture_2d_nop_copy_from_bitmap,
    NULL, /* texture_2d_get_data */
    _cg_nop_flush_attributes_state,
    _cg_clip_stack_nop_flush,
};
