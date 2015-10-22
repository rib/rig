/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Collabora Ltd.
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

#ifndef __CG_FENCE_PRIVATE_H__
#define __CG_FENCE_PRIVATE_H__

#include <clib.h>

#include "cg-fence.h"
#include "cg-winsys-private.h"

typedef enum {
    FENCE_TYPE_PENDING,
#ifdef GL_ARB_sync
    FENCE_TYPE_GL_ARB,
#endif
    FENCE_TYPE_WINSYS,
    FENCE_TYPE_ERROR
} cg_fence_type_t;

struct _cg_fence_closure_t {
    c_list_t link;
    cg_framebuffer_t *framebuffer;

    cg_fence_type_t type;
    void *fence_obj;

    cg_fence_callback_t callback;
    void *user_data;
};

void _cg_fence_submit(cg_fence_closure_t *fence);

void _cg_fence_cancel_fences_for_framebuffer(cg_framebuffer_t *framebuffer);

#endif /* __CG_FENCE_PRIVATE_H__ */
