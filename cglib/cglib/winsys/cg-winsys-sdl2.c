/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011, 2012, 2013 Intel Corporation.
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

#include <cglib-config.h>

#include <SDL.h>

#include "cg-renderer-private.h"
#include "cg-display-private.h"
#include "cg-framebuffer-private.h"
#include "cg-onscreen-template-private.h"
#include "cg-device-private.h"
#include "cg-onscreen-private.h"
#include "cg-winsys-sdl-private.h"
#include "cg-error-private.h"
#include "cg-loop-private.h"
#include "cg-sdl.h"

typedef struct _cg_device_sdl2_t {
    SDL_Window *current_window;
} cg_device_sdl2_t;

typedef struct _cg_renderer_sdl2_t {
    cg_closure_t *resize_notify_idle;
} cg_renderer_sdl2_t;

typedef struct _cg_display_sdl2_t {
    SDL_Window *dummy_window;
    SDL_GLContext *context;
    bool have_onscreen;
} cg_display_sdl2_t;

typedef struct _cg_onscreen_sdl2_t {
    SDL_Window *window;
    bool pending_resize_notify;
} cg_onscreen_sdl2_t;

/* The key used to store a pointer to the cg_onscreen_t in an
 * SDL_Window */
#define CG_SDL_WINDOW_DATA_KEY "cg-onscreen"

static cg_func_ptr_t
_cg_winsys_renderer_get_proc_address(
    cg_renderer_t *renderer, const char *name, bool in_core)
{
    /* XXX: It's not totally clear whether it's safe to call this for
     * core functions. From the code it looks like the implementations
     * will fall back to using some form of dlsym if the winsys
     * GetProcAddress function returns NULL. Presumably this will work
     * in most cases apart from EGL platforms that return invalid
     * pointers for core functions. It's awkward for this code to get a
     * handle to the GL module that SDL has chosen to load so just
     * calling SDL_GL_GetProcAddress is probably the best we can do
     * here. */
    return SDL_GL_GetProcAddress(name);
}

static void
_cg_winsys_renderer_disconnect(cg_renderer_t *renderer)
{
    SDL_VideoQuit();

    c_slice_free(cg_renderer_sdl2_t, renderer->winsys);
}

static bool
_cg_winsys_renderer_connect(cg_renderer_t *renderer,
                            cg_error_t **error)
{
    if (SDL_VideoInit(NULL) < 0) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_INIT,
                      "SDL_Init failed: %s",
                      SDL_GetError());
        return false;
    }

    renderer->winsys = c_slice_new0(cg_renderer_sdl2_t);

    return true;
}

static void
_cg_winsys_display_destroy(cg_display_t *display)
{
    cg_display_sdl2_t *sdl_display = display->winsys;

    c_return_if_fail(sdl_display != NULL);

    if (sdl_display->context)
        SDL_GL_DeleteContext(sdl_display->context);

    if (sdl_display->dummy_window)
        SDL_DestroyWindow(sdl_display->dummy_window);

    c_slice_free(cg_display_sdl2_t, display->winsys);
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
    cg_display_sdl2_t *sdl_display;
    const char *(*get_string_func)(GLenum name);
    const char *gl_version;

    c_return_val_if_fail(display->winsys == NULL, false);

    sdl_display = c_slice_new0(cg_display_sdl2_t);
    display->winsys = sdl_display;

    set_gl_attribs_from_framebuffer_config(&display->onscreen_template->config);

    if (display->renderer->driver == CG_DRIVER_GLES2) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                            SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    } else if (display->renderer->driver == CG_DRIVER_GL3) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                            SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,
                            SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    }

    /* Create a dummy 1x1 window that never gets display so that we can
     * create a GL context */
    sdl_display->dummy_window =
        SDL_CreateWindow("",
                         0,
                         0, /* x/y */
                         1,
                         1, /* w/h */
                         SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (sdl_display->dummy_window == NULL) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_INIT,
                      "SDL_CreateWindow failed: %s",
                      SDL_GetError());
        goto error;
    }

    sdl_display->context = SDL_GL_CreateContext(sdl_display->dummy_window);

    if (sdl_display->context == NULL) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_INIT,
                      "SDL_GL_CreateContext failed: %s",
                      SDL_GetError());
        goto error;
    }

    /* SDL doesn't seem to provide a way to select between GL and GLES
     * and instead it will just pick one itself. We can at least try to
     * verify that it picked the one we were expecting by looking at the
     * GL version string */
    get_string_func = SDL_GL_GetProcAddress("glGetString");
    gl_version = get_string_func(GL_VERSION);

    switch (display->renderer->driver) {
    case CG_DRIVER_GL:
    case CG_DRIVER_GL3:
        /* The first character of the version string will be a digit if
         * it's normal GL */
        if (!c_ascii_isdigit(gl_version[0])) {
            _cg_set_error(error,
                          CG_WINSYS_ERROR,
                          CG_WINSYS_ERROR_INIT,
                          "The GL driver was requested but SDL is using GLES");
            goto error;
        }

        if (display->renderer->driver == CG_DRIVER_GL3 && gl_version[0] < '3') {
            _cg_set_error(error,
                          CG_WINSYS_ERROR,
                          CG_WINSYS_ERROR_INIT,
                          "The GL3 driver was requested but SDL is using "
                          "GL %c",
                          gl_version[0]);
            goto error;
        }
        break;

    case CG_DRIVER_GLES2:
        if (!c_str_has_prefix(gl_version, "OpenGL ES 2") &&
            !c_str_has_prefix(gl_version, "OpenGL ES 3")) {
            _cg_set_error(error,
                          CG_WINSYS_ERROR,
                          CG_WINSYS_ERROR_INIT,
                          "The GLES2 driver was requested but SDL is "
                          "not using GLES2 or GLES3");
            goto error;
        }
        break;

    default:
        c_assert_not_reached();
    }

    return true;

error:
    _cg_winsys_display_destroy(display);
    return false;
}

static void
flush_pending_notifications_cb(void *data, void *user_data)
{
    cg_framebuffer_t *framebuffer = data;

    if (framebuffer->type == CG_FRAMEBUFFER_TYPE_ONSCREEN) {
        cg_onscreen_t *onscreen = CG_ONSCREEN(framebuffer);
        cg_onscreen_sdl2_t *sdl_onscreen = onscreen->winsys;

        if (sdl_onscreen->pending_resize_notify) {
            _cg_onscreen_notify_resize(onscreen);
            sdl_onscreen->pending_resize_notify = false;
        }
    }
}

static void
flush_pending_resize_notifications_idle(void *user_data)
{
    cg_device_t *dev = user_data;
    cg_renderer_t *renderer = dev->display->renderer;
    cg_renderer_sdl2_t *sdl_renderer = renderer->winsys;

    /* This needs to be disconnected before invoking the callbacks in
     * case the callbacks cause it to be queued again */
    _cg_closure_disconnect(sdl_renderer->resize_notify_idle);
    sdl_renderer->resize_notify_idle = NULL;

    c_llist_foreach(dev->framebuffers, flush_pending_notifications_cb, NULL);
}

static cg_filter_return_t
sdl_window_event_filter(SDL_WindowEvent *event,
                        cg_device_t *dev)
{
    SDL_Window *window;
    cg_framebuffer_t *framebuffer;

    window = SDL_GetWindowFromID(event->windowID);

    if (window == NULL)
        return CG_FILTER_CONTINUE;

    framebuffer = SDL_GetWindowData(window, CG_SDL_WINDOW_DATA_KEY);

    if (framebuffer == NULL || framebuffer->dev != dev)
        return CG_FILTER_CONTINUE;

    if (event->event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        cg_display_t *display = dev->display;
        cg_renderer_t *renderer = display->renderer;
        cg_renderer_sdl2_t *sdl_renderer = renderer->winsys;
        float width = event->data1;
        float height = event->data2;
        cg_onscreen_sdl2_t *sdl_onscreen;

        _cg_framebuffer_winsys_update_size(framebuffer, width, height);

        /* We only want to notify that a resize happened when the
         * application calls cg_device_dispatch so instead of
         * immediately notifying we queue an idle callback */
        if (!sdl_renderer->resize_notify_idle) {
            sdl_renderer->resize_notify_idle = _cg_loop_add_idle(
                renderer,
                flush_pending_resize_notifications_idle,
                dev,
                NULL);
        }

        sdl_onscreen = CG_ONSCREEN(framebuffer)->winsys;
        sdl_onscreen->pending_resize_notify = true;
    } else if (event->event == SDL_WINDOWEVENT_EXPOSED) {
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

static cg_filter_return_t
sdl_event_filter_cb(SDL_Event *event, void *data)
{
    cg_device_t *dev = data;

    switch (event->type) {
    case SDL_WINDOWEVENT:
        return sdl_window_event_filter(&event->window, dev);

    default:
        return CG_FILTER_CONTINUE;
    }
}

static bool
_cg_winsys_device_init(cg_device_t *dev, cg_error_t **error)
{
    cg_renderer_t *renderer = dev->display->renderer;

    dev->winsys = c_new0(cg_device_sdl2_t, 1);

    if (!_cg_device_update_features(dev, error))
        return false;

    CG_FLAGS_SET(dev->features, CG_FEATURE_ID_ONSCREEN_MULTIPLE, true);

    if (SDL_GL_GetSwapInterval() != -1)
        CG_FLAGS_SET(dev->winsys_features,
                     CG_WINSYS_FEATURE_SWAP_THROTTLE,
                     true);

    /* We'll manually handle queueing dirty events in response to
     * SDL_WINDOWEVENT_EXPOSED events */
    CG_FLAGS_SET(dev->private_features, CG_PRIVATE_FEATURE_DIRTY_EVENTS,
                 true);

    _cg_renderer_add_native_filter(
        renderer, (cg_native_filter_func_t)sdl_event_filter_cb, dev);

    return true;
}

static void
_cg_winsys_device_deinit(cg_device_t *dev)
{
    cg_renderer_t *renderer = dev->display->renderer;

    _cg_renderer_remove_native_filter(
        renderer, (cg_native_filter_func_t)sdl_event_filter_cb, dev);

    c_free(dev->winsys);
}

static void
_cg_winsys_onscreen_bind(cg_onscreen_t *onscreen)
{
    cg_framebuffer_t *fb = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = fb->dev;
    cg_device_sdl2_t *sdl_context = dev->winsys;
    cg_display_sdl2_t *sdl_display = dev->display->winsys;
    cg_onscreen_sdl2_t *sdl_onscreen = onscreen->winsys;

    if (sdl_context->current_window == sdl_onscreen->window)
        return;

    SDL_GL_MakeCurrent(sdl_onscreen->window, sdl_display->context);

    sdl_context->current_window = sdl_onscreen->window;

    /* It looks like SDL just directly calls a glXSwapInterval function
     * when this is called. This may be provided by either the EXT
     * extension, the SGI extension or the Mesa extension. The SGI
     * extension is per context so we can't just do this once when the
     * framebuffer is allocated. See the comments in the GLX winsys for
     * more info. */
    if (CG_FLAGS_GET(dev->winsys_features,
                     CG_WINSYS_FEATURE_SWAP_THROTTLE)) {
        cg_framebuffer_t *fb = CG_FRAMEBUFFER(onscreen);

        SDL_GL_SetSwapInterval(fb->config.swap_throttled ? 1 : 0);
    }
}

static void
_cg_winsys_onscreen_deinit(cg_onscreen_t *onscreen)
{
    cg_onscreen_sdl2_t *sdl_onscreen = onscreen->winsys;

    if (sdl_onscreen->window != NULL) {
        cg_device_t *dev = CG_FRAMEBUFFER(onscreen)->dev;
        cg_device_sdl2_t *sdl_context = dev->winsys;

        if (sdl_context->current_window == sdl_onscreen->window) {
            cg_display_sdl2_t *sdl_display = dev->display->winsys;

            /* SDL explicitly unbinds the context when the currently
             * bound window is destroyed. CGlib always needs a context
             * bound so that for example it can create texture resources
             * at any time even without flushing a framebuffer.
             * Therefore we'll bind the dummy window. */
            SDL_GL_MakeCurrent(sdl_display->dummy_window, sdl_display->context);
            sdl_context->current_window = sdl_display->dummy_window;
        }

        SDL_DestroyWindow(sdl_onscreen->window);
        sdl_onscreen->window = NULL;
    }

    c_slice_free(cg_onscreen_sdl2_t, sdl_onscreen);
    onscreen->winsys = NULL;
}

static bool
_cg_winsys_onscreen_init(cg_onscreen_t *onscreen,
                         cg_error_t **error)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_onscreen_sdl2_t *sdl_onscreen;
    SDL_Window *window;
    int width, height;
    SDL_WindowFlags flags;

#ifdef __ANDROID__
    {
        cg_display_t *display = framebuffer->dev->display;
        cg_display_sdl2_t *sdl_display = display->winsys;
        int win_width, win_height;

        if (sdl_display->have_onscreen) {
            _cg_set_error(error,
                          CG_WINSYS_ERROR,
                          CG_WINSYS_ERROR_CREATE_ONSCREEN,
                          "Android platform only supports a single "
                          "onscreen window");
            return false;
        }

        window = sdl_display->dummy_window;

        SDL_GetWindowSize(window, &win_width, &win_height);

        _cg_framebuffer_winsys_update_size(framebuffer, win_width, win_height);

        sdl_display->have_onscreen = true;
    }

#else /* __ANDROID__ */

    width = cg_framebuffer_get_width(framebuffer);
    height = cg_framebuffer_get_height(framebuffer);

    flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN;

    /* The resizable property on SDL window apparently can only be set
     * on creation */
    if (onscreen->resizable)
        flags |= SDL_WINDOW_RESIZABLE;

    window = SDL_CreateWindow("" /* title */,
                              0,
                              0, /* x/y */
                              width,
                              height,
                              flags);
    if (window == NULL) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_CREATE_ONSCREEN,
                      "SDL_CreateWindow failed: %s",
                      SDL_GetError());
        return false;
    }

#endif /* __ANDROID__ */

    SDL_SetWindowData(window, CG_SDL_WINDOW_DATA_KEY, onscreen);

    onscreen->winsys = c_slice_new(cg_onscreen_sdl2_t);
    sdl_onscreen = onscreen->winsys;
    sdl_onscreen->window = window;

    return true;
}

static void
_cg_winsys_onscreen_swap_buffers_with_damage(
    cg_onscreen_t *onscreen, const int *rectangles, int n_rectangles)
{
    cg_onscreen_sdl2_t *sdl_onscreen = onscreen->winsys;

    SDL_GL_SwapWindow(sdl_onscreen->window);
}

static void
_cg_winsys_onscreen_update_swap_throttled(cg_onscreen_t *onscreen)
{
    cg_device_t *dev = CG_FRAMEBUFFER(onscreen)->dev;
    cg_device_sdl2_t *sdl_context = dev->winsys;
    cg_onscreen_sdl2_t *sdl_onscreen = onscreen->winsys;

    if (sdl_context->current_window != sdl_onscreen->window)
        return;

    sdl_context->current_window = NULL;
    _cg_winsys_onscreen_bind(onscreen);
}

static void
_cg_winsys_onscreen_set_visibility(cg_onscreen_t *onscreen,
                                   bool visibility)
{
    cg_onscreen_sdl2_t *sdl_onscreen = onscreen->winsys;

    if (visibility)
        SDL_ShowWindow(sdl_onscreen->window);
    else
        SDL_HideWindow(sdl_onscreen->window);
}

SDL_Window *
cg_sdl_onscreen_get_window(cg_onscreen_t *onscreen)
{
    cg_onscreen_sdl2_t *sdl_onscreen;

    c_return_val_if_fail(cg_is_onscreen(onscreen), NULL);

    if (!cg_framebuffer_allocate(CG_FRAMEBUFFER(onscreen), NULL))
        return NULL;

    sdl_onscreen = onscreen->winsys;

    return sdl_onscreen->window;
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

        vtable_inited = true;
    }

    return &vtable;
}
