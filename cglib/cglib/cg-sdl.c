/*
 * CGlib
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

#include <cglib-config.h>

#include "cg-sdl.h"
#include "cg-device-private.h"
#include "cg-renderer-private.h"

void
cg_sdl_renderer_set_event_type(cg_renderer_t *renderer, int type)
{
    renderer->sdl_event_type_set = true;
    renderer->sdl_event_type = type;
}

int
cg_sdl_renderer_get_event_type(cg_renderer_t *renderer)
{
    c_return_val_if_fail(renderer->sdl_event_type_set, SDL_USEREVENT);

    return renderer->sdl_event_type;
}

cg_device_t *
cg_sdl_device_new(int type, cg_error_t **error)
{
    cg_renderer_t *renderer = cg_renderer_new();
    cg_device_t *dev = NULL;

    cg_renderer_set_winsys_id(renderer, CG_WINSYS_ID_SDL);

    cg_sdl_renderer_set_event_type(renderer, type);

    if (!cg_renderer_connect(renderer, error))
        goto error;

    dev = cg_device_new();
    cg_device_set_renderer(dev, renderer);

    return dev;

error:
    cg_object_unref(renderer);
    if (dev)
        cg_object_unref(dev);

    return NULL;
}

void
cg_sdl_handle_event(cg_device_t *dev, SDL_Event *event)
{
    cg_renderer_t *renderer;

    c_return_if_fail(cg_is_device(dev));

    renderer = dev->display->renderer;

    _cg_renderer_handle_native_event(renderer, event);
}

static void
_cg_sdl_push_wakeup_event(cg_device_t *dev)
{
    cg_renderer_t *renderer = dev->display->renderer;
    SDL_Event wakeup_event;

    if (renderer->sdl_event_type_set)
        wakeup_event.type = renderer->sdl_event_type;
    else
        wakeup_event.type = SDL_USEREVENT;

    SDL_PushEvent(&wakeup_event);
}

void
cg_sdl_idle(cg_device_t *dev)
{
    cg_renderer_t *renderer = dev->display->renderer;

    cg_loop_dispatch(renderer, NULL, 0);

    /* It is expected that this will be called from the application
     * immediately before blocking in SDL_WaitEvent. However,
     * dispatching cause more work to be queued. If that happens we need
     * to make sure the blocking returns immediately. We'll post our
     * dummy event to make sure that happens
     */
    if (!c_list_empty(&renderer->idle_closures))
        _cg_sdl_push_wakeup_event(dev);
}
