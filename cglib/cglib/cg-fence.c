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
 */

#include <cglib-config.h>

#include "cg-fence.h"
#include "cg-fence-private.h"
#include "cg-device-private.h"
#include "cg-winsys-private.h"

#define FENCE_CHECK_TIMEOUT 5000 /* microseconds */

void *
cg_fence_closure_get_user_data(cg_fence_closure_t *closure)
{
    return closure->user_data;
}

static void
_cg_fence_check(cg_fence_closure_t *fence)
{
    cg_device_t *dev = fence->framebuffer->dev;

    if (fence->type == FENCE_TYPE_WINSYS) {
        const cg_winsys_vtable_t *winsys = _cg_device_get_winsys(dev);
        bool ret;

        ret = winsys->fence_is_complete(dev, fence->fence_obj);
        if (!ret)
            return;
    }
#ifdef GL_ARB_sync
    else if (fence->type == FENCE_TYPE_GL_ARB) {
        GLenum arb;

        arb = dev->glClientWaitSync(
            fence->fence_obj, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
        if (arb != GL_ALREADY_SIGNALED && arb != GL_CONDITION_SATISFIED)
            return;
    }
#endif

    fence->callback(NULL, /* dummy cg_fence_t object */
                    fence->user_data);
    cg_framebuffer_cancel_fence_callback(fence->framebuffer, fence);
}

static void
_cg_fence_poll_dispatch(void *source, int revents)
{
    cg_device_t *dev = source;
    cg_fence_closure_t *fence, *tmp;

    c_list_for_each_safe(fence, tmp, &dev->fences, link)
        _cg_fence_check(fence);
}

static int64_t
_cg_fence_poll_prepare(void *source)
{
    cg_device_t *dev = source;

    if (!c_list_empty(&dev->fences))
        return FENCE_CHECK_TIMEOUT;
    else
        return -1;
}

void
_cg_fence_submit(cg_fence_closure_t *fence)
{
    cg_device_t *dev = fence->framebuffer->dev;
    const cg_winsys_vtable_t *winsys = _cg_device_get_winsys(dev);

    fence->type = FENCE_TYPE_ERROR;

    if (winsys->fence_add) {
        fence->fence_obj = winsys->fence_add(dev);
        if (fence->fence_obj) {
            fence->type = FENCE_TYPE_WINSYS;
            goto done;
        }
    }

#ifdef GL_ARB_sync
    if (dev->glFenceSync) {
        fence->fence_obj =
            dev->glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        if (fence->fence_obj) {
            fence->type = FENCE_TYPE_GL_ARB;
            goto done;
        }
    }
#endif

done:
    c_list_insert(dev->fences.prev, &fence->link);

    if (!dev->fences_poll_source) {
        dev->fences_poll_source =
            _cg_loop_add_source(dev->display->renderer,
                                         _cg_fence_poll_prepare,
                                         _cg_fence_poll_dispatch,
                                         dev);
    }
}

cg_fence_closure_t *
cg_framebuffer_add_fence_callback(cg_framebuffer_t *framebuffer,
                                  cg_fence_callback_t callback,
                                  void *user_data)
{
    cg_device_t *dev = framebuffer->dev;
    cg_fence_closure_t *fence;

    if (!CG_FLAGS_GET(dev->features, CG_FEATURE_ID_FENCE))
        return NULL;

    fence = c_slice_new(cg_fence_closure_t);
    fence->framebuffer = framebuffer;
    fence->callback = callback;
    fence->user_data = user_data;
    fence->fence_obj = NULL;

    _cg_fence_submit(fence);

    return fence;
}

void
cg_framebuffer_cancel_fence_callback(cg_framebuffer_t *framebuffer,
                                     cg_fence_closure_t *fence)
{
    cg_device_t *dev = framebuffer->dev;

    if (fence->type == FENCE_TYPE_PENDING) {
        c_list_remove(&fence->link);
    } else {
        c_list_remove(&fence->link);

        if (fence->type == FENCE_TYPE_WINSYS) {
            const cg_winsys_vtable_t *winsys = _cg_device_get_winsys(dev);

            winsys->fence_destroy(dev, fence->fence_obj);
        }
#ifdef GL_ARB_sync
        else if (fence->type == FENCE_TYPE_GL_ARB) {
            dev->glDeleteSync(fence->fence_obj);
        }
#endif
    }

    c_slice_free(cg_fence_closure_t, fence);
}

void
_cg_fence_cancel_fences_for_framebuffer(cg_framebuffer_t *framebuffer)
{
    cg_device_t *dev = framebuffer->dev;
    cg_fence_closure_t *fence, *tmp;

    c_list_for_each_safe(fence, tmp, &dev->fences, link) {
        if (fence->framebuffer == framebuffer)
            cg_framebuffer_cancel_fence_callback(framebuffer, fence);
    }
}
