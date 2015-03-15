/*
 * Cogl
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011,2013 Intel Corporation.
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
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <SDL.h>

#include "cogl-renderer-private.h"
#include "cogl-display-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-onscreen-template-private.h"
#include "cogl-device-private.h"
#include "cogl-onscreen-private.h"
#include "cogl-winsys-sdl-private.h"
#include "cogl-error-private.h"
#include "cogl-loop-private.h"

typedef struct _cg_renderer_sdl_t {
    cg_closure_t *resize_notify_idle;
} cg_renderer_sdl_t;

typedef struct _cg_display_sdl_t {
    SDL_Surface *surface;
    cg_onscreen_t *onscreen;
    Uint32 video_mode_flags;
} cg_display_sdl_t;

static cg_func_ptr_t
_cg_winsys_renderer_get_proc_address(
    cg_renderer_t *renderer, const char *name, bool in_core)
{
    cg_func_ptr_t ptr;

/* XXX: It's not totally clear whether it's safe to call this for
 * core functions. From the code it looks like the implementations
 * will fall back to using some form of dlsym if the winsys
 * GetProcAddress function returns NULL. Presumably this will work
 * in most cases apart from EGL platforms that return invalid
 * pointers for core functions. It's awkward for this code to get a
 * handle to the GL module that SDL has chosen to load so just
 * calling SDL_GL_GetProcAddress is probably the best we can do
 * here. */

#ifdef CG_HAS_SDL_GLES_SUPPORT
    if (renderer->driver != CG_DRIVER_GL)
        return SDL_GLES_GetProcAddress(name);
#endif

    return SDL_GL_GetProcAddress(name);
}

static void
_cg_winsys_renderer_disconnect(cg_renderer_t *renderer)
{
    SDL_Quit();

    c_slice_free(cg_renderer_sdl_t, renderer->winsys);
}

static bool
_cg_winsys_renderer_connect(cg_renderer_t *renderer,
                            cg_error_t **error)
{
#ifdef EMSCRIPTEN
    if (renderer->driver != CG_DRIVER_GLES2) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_INIT,
                      "The SDL winsys with emscripten only supports "
                      "the GLES2 driver");
        return false;
    }
#elif !defined(CG_HAS_SDL_GLES_SUPPORT)
    if (renderer->driver != CG_DRIVER_GL) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_INIT,
                      "The SDL winsys only supports the GL driver");
        return false;
    }
#endif

    if (SDL_Init(SDL_INIT_VIDEO) == -1) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_INIT,
                      "SDL_Init failed: %s",
                      SDL_GetError());
        return false;
    }

    renderer->winsys = c_slice_new0(cg_renderer_sdl_t);

    return true;
}

static void
_cg_winsys_display_destroy(cg_display_t *display)
{
    cg_display_sdl_t *sdl_display = display->winsys;

    c_return_if_fail(sdl_display != NULL);

    /* No need to destroy the surface - it is freed by SDL_Quit */

    c_slice_free(cg_display_sdl_t, display->winsys);
    display->winsys = NULL;
}

static void
set_gl_attribs_from_framebuffer_config(cg_framebuffer_config_t *config)
{
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 1);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 1);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 1);

    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, config->need_stencil ? 1 : 0);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, config->has_alpha ? 1 : 0);
}

static bool
_cg_winsys_display_setup(cg_display_t *display, cg_error_t **error)
{
    cg_display_sdl_t *sdl_display;

    c_return_val_if_fail(display->winsys == NULL, false);

    sdl_display = c_slice_new0(cg_display_sdl_t);
    display->winsys = sdl_display;

    set_gl_attribs_from_framebuffer_config(&display->onscreen_template->config);

    switch (display->renderer->driver) {
    case CG_DRIVER_GL:
        sdl_display->video_mode_flags = SDL_OPENGL;
        break;

    case CG_DRIVER_GL3:
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_INIT,
                      "The SDL winsys does not support GL 3");
        goto error;

#ifdef CG_HAS_SDL_GLES_SUPPORT
    case CG_DRIVER_GLES2:
        sdl_display->video_mode_flags = SDL_OPENGLES;
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        break;

#elif defined(EMSCRIPTEN)
    case CG_DRIVER_GLES2:
        sdl_display->video_mode_flags = SDL_OPENGL;
        break;
#endif

    default:
        c_assert_not_reached();
    }

    /* There's no way to know what size the application will need until
       it creates the first onscreen but we need to set the video mode
       now so that we can get a GL context. We'll have to just guess at
       a size an resize it later */
    sdl_display->surface = SDL_SetVideoMode(640,
                                            480, /* width/height */
                                            0, /* bitsperpixel */
                                            sdl_display->video_mode_flags);

    if (sdl_display->surface == NULL) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_INIT,
                      "SDL_SetVideoMode failed: %s",
                      SDL_GetError());
        goto error;
    }

    return true;

error:
    _cg_winsys_display_destroy(display);
    return false;
}

static void
flush_pending_resize_notification_idle(void *user_data)
{
    cg_device_t *dev = user_data;
    cg_renderer_t *renderer = dev->display->renderer;
    cg_renderer_sdl_t *sdl_renderer = renderer->winsys;
    cg_display_sdl_t *sdl_display = dev->display->winsys;
    cg_onscreen_t *onscreen = sdl_display->onscreen;

    /* This needs to be disconnected before invoking the callbacks in
     * case the callbacks cause it to be queued again */
    _cg_closure_disconnect(sdl_renderer->resize_notify_idle);
    sdl_renderer->resize_notify_idle = NULL;

    _cg_onscreen_notify_resize(onscreen);
}

static cg_filter_return_t
sdl_event_filter_cb(SDL_Event *event, void *data)
{
    cg_device_t *dev = data;
    cg_display_t *display = dev->display;
    cg_display_sdl_t *sdl_display = display->winsys;
    cg_framebuffer_t *framebuffer;

    if (!sdl_display->onscreen)
        return CG_FILTER_CONTINUE;

    framebuffer = CG_FRAMEBUFFER(sdl_display->onscreen);

    if (event->type == SDL_VIDEORESIZE) {
        cg_renderer_t *renderer = display->renderer;
        cg_renderer_sdl_t *sdl_renderer = renderer->winsys;
        float width = event->resize.w;
        float height = event->resize.h;

        sdl_display->surface = SDL_SetVideoMode(width,
                                                height,
                                                0, /* bitsperpixel */
                                                sdl_display->video_mode_flags);

        _cg_framebuffer_winsys_update_size(framebuffer, width, height);

        /* We only want to notify that a resize happened when the
         * application calls cg_device_dispatch so instead of
         * immediately notifying we queue an idle callback */
        if (!sdl_renderer->resize_notify_idle) {
            sdl_renderer->resize_notify_idle = _cg_loop_add_idle(
                renderer,
                flush_pending_resize_notification_idle,
                dev,
                NULL);
        }

        return CG_FILTER_CONTINUE;
    } else if (event->type == SDL_VIDEOEXPOSE) {
        cg_onscreen_dirty_info_t info;

        /* Sadly SDL doesn't seem to report the rectangle of the expose
         * event so we'll just queue the whole window */
        info.x = 0;
        info.y = 0;
        info.width = framebuffer->width;
        info.height = framebuffer->height;

        _cg_onscreen_queue_dirty(CG_ONSCREEN(framebuffer), &info);
    }

    return CG_FILTER_CONTINUE;
}

static bool
_cg_winsys_device_init(cg_device_t *dev, cg_error_t **error)
{
    cg_renderer_t *renderer = dev->display->renderer;

    _cg_renderer_add_native_filter(
        renderer, (cg_native_filter_func_t)sdl_event_filter_cb, dev);

    /* We'll manually handle queueing dirty events in response to
     * SDL_VIDEOEXPOSE events */
    CG_FLAGS_SET(dev->private_features, CG_PRIVATE_FEATURE_DIRTY_EVENTS,
                 true);

    return _cg_device_update_features(dev, error);
}

static void
_cg_winsys_device_deinit(cg_device_t *dev)
{
}

static void
_cg_winsys_onscreen_bind(cg_onscreen_t *onscreen)
{
}

static void
_cg_winsys_onscreen_deinit(cg_onscreen_t *onscreen)
{
    cg_device_t *dev = CG_FRAMEBUFFER(onscreen)->dev;
    cg_display_t *display = dev->display;
    cg_display_sdl_t *sdl_display = display->winsys;

    sdl_display->onscreen = NULL;
}

static bool
_cg_winsys_onscreen_init(cg_onscreen_t *onscreen,
                         cg_error_t **error)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_display_t *display = dev->display;
    cg_display_sdl_t *sdl_display = display->winsys;
    bool flags_changed = false;
    int width, height;

    if (sdl_display->onscreen) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_CREATE_ONSCREEN,
                      "SDL winsys only supports a single onscreen window");
        return false;
    }

    width = cg_framebuffer_get_width(framebuffer);
    height = cg_framebuffer_get_height(framebuffer);

    if (cg_onscreen_get_resizable(onscreen)) {
        sdl_display->video_mode_flags |= SDL_RESIZABLE;
        flags_changed = true;
    }

    /* Try to update the video size using the onscreen size */
    if (width != sdl_display->surface->w || height != sdl_display->surface->h ||
        flags_changed) {
        sdl_display->surface = SDL_SetVideoMode(width,
                                                height,
                                                0, /* bitsperpixel */
                                                sdl_display->video_mode_flags);

        if (sdl_display->surface == NULL) {
            _cg_set_error(error,
                          CG_WINSYS_ERROR,
                          CG_WINSYS_ERROR_CREATE_ONSCREEN,
                          "SDL_SetVideoMode failed: %s",
                          SDL_GetError());
            return false;
        }
    }

    _cg_framebuffer_winsys_update_size(
        framebuffer, sdl_display->surface->w, sdl_display->surface->h);

    sdl_display->onscreen = onscreen;

    return true;
}

static void
_cg_winsys_onscreen_swap_buffers_with_damage(
    cg_onscreen_t *onscreen, const int *rectangles, int n_rectangles)
{
    SDL_GL_SwapBuffers();
}

static void
_cg_winsys_onscreen_update_swap_throttled(cg_onscreen_t *onscreen)
{
    /* SDL doesn't appear to provide a way to set this */
}

static void
_cg_winsys_onscreen_set_visibility(cg_onscreen_t *onscreen,
                                   bool visibility)
{
    /* SDL doesn't appear to provide a way to set this */
}

static void
_cg_winsys_onscreen_set_resizable(cg_onscreen_t *onscreen,
                                  bool resizable)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_display_t *display = dev->display;
    cg_display_sdl_t *sdl_display = display->winsys;
    int width, height;

    width = cg_framebuffer_get_width(framebuffer);
    height = cg_framebuffer_get_height(framebuffer);

    if (resizable)
        sdl_display->video_mode_flags |= SDL_RESIZABLE;
    else
        sdl_display->video_mode_flags &= ~SDL_RESIZABLE;

    sdl_display->surface = SDL_SetVideoMode(width,
                                            height,
                                            0, /* bitsperpixel */
                                            sdl_display->video_mode_flags);
}

const cg_winsys_vtable_t *
_cg_winsys_sdl_get_vtable(void)
{
    static bool vtable_inited = false;
    static cg_winsys_vtable_t vtable;

    /* It would be nice if we could use C99 struct initializers here
       like the GLX backend does. However this code is more likely to be
       compiled using Visual Studio which (still!) doesn't support them
       so we initialize it in code instead */

    if (!vtable_inited) {
        memset(&vtable, 0, sizeof(vtable));

        vtable.id = CG_WINSYS_ID_SDL;
        vtable.name = "SDL";
        vtable.renderer_get_proc_address = _cg_winsys_renderer_get_proc_address;
        vtable.renderer_connect = _cg_winsys_renderer_connect;
        vtable.renderer_disconnect = _cg_winsys_renderer_disconnect;
        vtable.display_setup = _cg_winsys_display_setup;
        vtable.display_destroy = _cg_winsys_display_destroy;
        vtable.device_init = _cg_winsys_device_init;
        vtable.device_deinit = _cg_winsys_device_deinit;
        vtable.onscreen_init = _cg_winsys_onscreen_init;
        vtable.onscreen_deinit = _cg_winsys_onscreen_deinit;
        vtable.onscreen_bind = _cg_winsys_onscreen_bind;
        vtable.onscreen_swap_buffers_with_damage =
            _cg_winsys_onscreen_swap_buffers_with_damage;
        vtable.onscreen_update_swap_throttled =
            _cg_winsys_onscreen_update_swap_throttled;
        vtable.onscreen_set_visibility = _cg_winsys_onscreen_set_visibility;
        vtable.onscreen_set_resizable = _cg_winsys_onscreen_set_resizable;

        vtable_inited = true;
    }

    return &vtable;
}
