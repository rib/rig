/*
 * Cogl
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-fence.h"
#include "cogl-fence-private.h"
#include "cogl-context-private.h"
#include "cogl-winsys-private.h"

#define FENCE_CHECK_TIMEOUT 5000 /* microseconds */

void *
cg_fence_closure_get_user_data(cg_fence_closure_t *closure)
{
    return closure->user_data;
}

static void
_cg_fence_check(cg_fence_closure_t *fence)
{
    cg_context_t *context = fence->framebuffer->context;

    if (fence->type == FENCE_TYPE_WINSYS) {
        const cg_winsys_vtable_t *winsys = _cg_context_get_winsys(context);
        bool ret;

        ret = winsys->fence_is_complete(context, fence->fence_obj);
        if (!ret)
            return;
    }
#ifdef GL_ARB_sync
    else if (fence->type == FENCE_TYPE_GL_ARB) {
        GLenum arb;

        arb = context->glClientWaitSync(
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
    cg_context_t *context = source;
    cg_fence_closure_t *fence, *tmp;

    _cg_list_for_each_safe(fence, tmp, &context->fences, link)
    _cg_fence_check(fence);
}

static int64_t
_cg_fence_poll_prepare(void *source)
{
    cg_context_t *context = source;
    c_list_t *l;

    /* If there are any pending fences in any of the journals then we
     * need to flush the journal otherwise the fence will never be
     * hit and the main loop might block forever */
    for (l = context->framebuffers; l; l = l->next) {
        cg_framebuffer_t *fb = l->data;

        if (!_cg_list_empty(&fb->journal->pending_fences))
            _cg_framebuffer_flush_journal(fb);
    }

    if (!_cg_list_empty(&context->fences))
        return FENCE_CHECK_TIMEOUT;
    else
        return -1;
}

void
_cg_fence_submit(cg_fence_closure_t *fence)
{
    cg_context_t *context = fence->framebuffer->context;
    const cg_winsys_vtable_t *winsys = _cg_context_get_winsys(context);

    fence->type = FENCE_TYPE_ERROR;

    if (winsys->fence_add) {
        fence->fence_obj = winsys->fence_add(context);
        if (fence->fence_obj) {
            fence->type = FENCE_TYPE_WINSYS;
            goto done;
        }
    }

#ifdef GL_ARB_sync
    if (context->glFenceSync) {
        fence->fence_obj =
            context->glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        if (fence->fence_obj) {
            fence->type = FENCE_TYPE_GL_ARB;
            goto done;
        }
    }
#endif

done:
    _cg_list_insert(context->fences.prev, &fence->link);

    if (!context->fences_poll_source) {
        context->fences_poll_source =
            _cg_poll_renderer_add_source(context->display->renderer,
                                         _cg_fence_poll_prepare,
                                         _cg_fence_poll_dispatch,
                                         context);
    }
}

cg_fence_closure_t *
cg_framebuffer_add_fence_callback(cg_framebuffer_t *framebuffer,
                                  cg_fence_callback_t callback,
                                  void *user_data)
{
    cg_context_t *context = framebuffer->context;
    cg_journal_t *journal = framebuffer->journal;
    cg_fence_closure_t *fence;

    if (!CG_FLAGS_GET(context->features, CG_FEATURE_ID_FENCE))
        return NULL;

    fence = c_slice_new(cg_fence_closure_t);
    fence->framebuffer = framebuffer;
    fence->callback = callback;
    fence->user_data = user_data;
    fence->fence_obj = NULL;

    if (journal->entries->len) {
        _cg_list_insert(journal->pending_fences.prev, &fence->link);
        fence->type = FENCE_TYPE_PENDING;
    } else
        _cg_fence_submit(fence);

    return fence;
}

void
cg_framebuffer_cancel_fence_callback(cg_framebuffer_t *framebuffer,
                                     cg_fence_closure_t *fence)
{
    cg_context_t *context = framebuffer->context;

    if (fence->type == FENCE_TYPE_PENDING) {
        _cg_list_remove(&fence->link);
    } else {
        _cg_list_remove(&fence->link);

        if (fence->type == FENCE_TYPE_WINSYS) {
            const cg_winsys_vtable_t *winsys = _cg_context_get_winsys(context);

            winsys->fence_destroy(context, fence->fence_obj);
        }
#ifdef GL_ARB_sync
        else if (fence->type == FENCE_TYPE_GL_ARB) {
            context->glDeleteSync(fence->fence_obj);
        }
#endif
    }

    c_slice_free(cg_fence_closure_t, fence);
}

void
_cg_fence_cancel_fences_for_framebuffer(cg_framebuffer_t *framebuffer)
{
    cg_journal_t *journal = framebuffer->journal;
    cg_context_t *context = framebuffer->context;
    cg_fence_closure_t *fence, *tmp;

    while (!_cg_list_empty(&journal->pending_fences)) {
        fence = _cg_container_of(
            journal->pending_fences.next, cg_fence_closure_t, link);
        cg_framebuffer_cancel_fence_callback(framebuffer, fence);
    }

    _cg_list_for_each_safe(fence, tmp, &context->fences, link)
    {
        if (fence->framebuffer == framebuffer)
            cg_framebuffer_cancel_fence_callback(framebuffer, fence);
    }
}
