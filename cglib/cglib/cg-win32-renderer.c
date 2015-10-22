/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
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
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#include <cglib-config.h>

#include "cg-renderer-private.h"
#include "cg-win32-renderer.h"

cg_filter_return_t
cg_win32_renderer_handle_event(cg_renderer_t *renderer,
                               MSG *event)
{
    return _cg_renderer_handle_native_event(renderer, event);
}

void
cg_win32_renderer_add_filter(cg_renderer_t *renderer,
                             cg_win32_filter_func_t func,
                             void *data)
{
    _cg_renderer_add_native_filter(
        renderer, (cg_native_filter_func_t)func, data);
}

void
cg_win32_renderer_remove_filter(cg_renderer_t *renderer,
                                cg_win32_filter_func_t func,
                                void *data)
{
    _cg_renderer_remove_native_filter(
        renderer, (cg_native_filter_func_t)func, data);
}

void
cg_win32_renderer_set_event_retrieval_enabled(cg_renderer_t *renderer,
                                              bool enable)
{
    c_return_if_fail(cg_is_renderer(renderer));
    /* NB: Renderers are considered immutable once connected */
    c_return_if_fail(!renderer->connected);

    renderer->win32_enable_event_retrieval = enable;
}
