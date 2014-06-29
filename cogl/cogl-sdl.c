/*
 * Cogl
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012, 2013 Intel Corporation.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-sdl.h"
#include "cogl-context-private.h"
#include "cogl-renderer-private.h"

void
cg_sdl_renderer_set_event_type(cg_renderer_t *renderer, int type)
{
    renderer->sdl_event_type_set = true;
    renderer->sdl_event_type = type;
}

int
cg_sdl_renderer_get_event_type(cg_renderer_t *renderer)
{
    _CG_RETURN_VAL_IF_FAIL(renderer->sdl_event_type_set, SDL_USEREVENT);

    return renderer->sdl_event_type;
}

cg_context_t *
cg_sdl_context_new(int type, cg_error_t **error)
{
    cg_renderer_t *renderer = cg_renderer_new();
    cg_display_t *display;

    cg_renderer_set_winsys_id(renderer, CG_WINSYS_ID_SDL);

    cg_sdl_renderer_set_event_type(renderer, type);

    if (!cg_renderer_connect(renderer, error))
        return NULL;

    display = cg_display_new(renderer, NULL);
    if (!cg_display_setup(display, error))
        return NULL;

    return cg_context_new(display, error);
}

void
cg_sdl_handle_event(cg_context_t *context, SDL_Event *event)
{
    cg_renderer_t *renderer;

    _CG_RETURN_IF_FAIL(cg_is_context(context));

    renderer = context->display->renderer;

    _cg_renderer_handle_native_event(renderer, event);
}

static void
_cg_sdl_push_wakeup_event(cg_context_t *context)
{
    SDL_Event wakeup_event;

    wakeup_event.type = context->display->renderer->sdl_event_type;

    SDL_PushEvent(&wakeup_event);
}

void
cg_sdl_idle(cg_context_t *context)
{
    cg_renderer_t *renderer = context->display->renderer;

    cg_poll_renderer_dispatch(renderer, NULL, 0);

    /* It is expected that this will be called from the application
     * immediately before blocking in SDL_WaitEvent. However,
     * dispatching cause more work to be queued. If that happens we need
     * to make sure the blocking returns immediately. We'll post our
     * dummy event to make sure that happens
     */
    if (!_cg_list_empty(&renderer->idle_closures))
        _cg_sdl_push_wakeup_event(context);
}
