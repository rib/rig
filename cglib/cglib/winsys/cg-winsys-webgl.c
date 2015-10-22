/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2015 Intel Corporation.
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

#include <emscripten.h>
#include <html5.h>

#include "cg-renderer-private.h"
#include "cg-display-private.h"
#include "cg-framebuffer-private.h"
#include "cg-onscreen-template-private.h"
#include "cg-device-private.h"
#include "cg-onscreen-private.h"
#include "cg-winsys-webgl-private.h"
#include "cg-error-private.h"
#include "cg-loop-private.h"
#include "cg-emscripten-lib.h"

typedef struct _cg_device_webgl_t {
    int current_window;
} cg_device_webgl_t;

typedef struct _cg_renderer_webgl_t {
    cg_closure_t *resize_notify_idle;
} cg_renderer_webgl_t;

typedef struct _cg_webgl_display_t {

    /* XXX: WebGL 1.0 has a 1:1 window:context mapping so
     * we can't support creating multiple onscreen
     * framebuffer's per-context... */

    int window;
    char *window_id;

    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context;
} cg_webgl_display_t;

typedef struct _cg_onscreen_webgl_t {
    int window;
} cg_onscreen_webgl_t;

extern void* emscripten_GetProcAddress(const char *x);

static cg_func_ptr_t
_cg_winsys_renderer_get_proc_address(
    cg_renderer_t *renderer, const char *name, bool in_core)
{
    return emscripten_GetProcAddress(name);
}

static void
_cg_winsys_renderer_disconnect(cg_renderer_t *renderer)
{
    c_slice_free(cg_renderer_webgl_t, renderer->winsys);
}

static bool
_cg_winsys_renderer_connect(cg_renderer_t *renderer,
                            cg_error_t **error)
{
    renderer->winsys = c_slice_new0(cg_renderer_webgl_t);

    return true;
}

static void
_cg_winsys_display_destroy(cg_display_t *display)
{
    cg_webgl_display_t *webgl_display = display->winsys;

    c_return_if_fail(webgl_display != NULL);

    if (webgl_display->context > 0)
        emscripten_webgl_destroy_context(webgl_display->context);

    if (webgl_display->window > 0)
        CGlib_Emscripten_DestroyWindow(webgl_display->window);

    c_free(webgl_display->window_id);

    c_slice_free(cg_webgl_display_t, display->winsys);
    display->winsys = NULL;
}

static void
init_webgl_attribs_from_framebuffer_config(EmscriptenWebGLContextAttributes *attribs,
                                           cg_framebuffer_config_t *config)
{
    emscripten_webgl_init_context_attributes(attribs);

    attribs->antialias = config->samples_per_pixel ? true : false;

    attribs->alpha = config->has_alpha ? true : false;
    attribs->stencil = config->need_stencil ? true : false;
}

static bool
_cg_winsys_display_setup(cg_display_t *display, cg_error_t **error)
{
    cg_webgl_display_t *webgl_display;
    int window;
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context;
    EmscriptenWebGLContextAttributes attribs;
    char *id;

    c_return_val_if_fail(display->winsys == NULL, false);

    init_webgl_attribs_from_framebuffer_config(&attribs,
                                               &display->onscreen_template->config);

    window = CGlib_Emscripten_CreateWindow(100, 100, 0, 0);

    if (window < 0) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_INIT,
                      "create window failed");
        return false;
    }

    CGlib_Emscripten_HideWindow(window);

    id = c_strdup_printf("cg_window_%d", window);
    context = emscripten_webgl_create_context(id, &attribs);

    if (context < 0) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_INIT,
                      "create context failed");
        CGlib_Emscripten_DestroyWindow(window);
        c_free(id);
        return false;
    }

    emscripten_webgl_make_context_current(context);

    webgl_display = c_slice_new0(cg_webgl_display_t);
    display->winsys = webgl_display;

    webgl_display->window = window;
    webgl_display->window_id = id;
    webgl_display->context = context;

    return true;
}

static bool
_cg_winsys_device_init(cg_device_t *dev, cg_error_t **error)
{
    dev->winsys = c_new0(cg_device_webgl_t, 1);

    if (!_cg_device_update_features(dev, error))
        return false;

    CG_FLAGS_SET(dev->winsys_features,
                 CG_WINSYS_FEATURE_SWAP_THROTTLE,
                 true);

    return true;
}

static void
_cg_winsys_device_deinit(cg_device_t *dev)
{
    c_free(dev->winsys);
}

static void
_cg_winsys_onscreen_bind(cg_onscreen_t *onscreen)
{
    cg_framebuffer_t *fb = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = fb->dev;
    cg_device_webgl_t *webgl_context = dev->winsys;
    cg_webgl_display_t *webgl_display = dev->display->winsys;
    cg_onscreen_webgl_t *webgl_onscreen = onscreen->winsys;

    if (webgl_context->current_window == webgl_onscreen->window)
        return;

    emscripten_webgl_make_context_current(webgl_display->context);
    webgl_context->current_window = webgl_onscreen->window;
}

static void
_cg_winsys_onscreen_deinit(cg_onscreen_t *onscreen)
{
    cg_onscreen_webgl_t *webgl_onscreen = onscreen->winsys;

    c_slice_free(cg_onscreen_webgl_t, webgl_onscreen);
    onscreen->winsys = NULL;
}

static bool
_cg_winsys_onscreen_init(cg_onscreen_t *onscreen,
                         cg_error_t **error)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_onscreen_webgl_t *webgl_onscreen;
    cg_device_t *dev = framebuffer->dev;
    cg_display_t *display = dev->display;
    cg_webgl_display_t *webgl_display = display->winsys;
    int width, height;

    width = cg_framebuffer_get_width(framebuffer);
    height = cg_framebuffer_get_height(framebuffer);

    CGlib_Emscripten_SetWindowSize(webgl_display->window, width, height);

    onscreen->winsys = c_slice_new(cg_onscreen_webgl_t);
    webgl_onscreen = onscreen->winsys;
    webgl_onscreen->window = webgl_display->window;

    return true;
}

static void
_cg_winsys_onscreen_swap_buffers_with_damage(
    cg_onscreen_t *onscreen, const int *rectangles, int n_rectangles)
{
    /* SwapBuffers happens implicitly within the browser mainloop */
}

static void
_cg_winsys_onscreen_update_swap_throttled(cg_onscreen_t *onscreen)
{
    /* NOP */
}

static void
_cg_winsys_onscreen_set_visibility(cg_onscreen_t *onscreen,
                                   bool visibility)
{
    cg_onscreen_webgl_t *webgl_onscreen = onscreen->winsys;

    if (visibility)
        CGlib_Emscripten_ShowWindow(webgl_onscreen->window);
    else
        CGlib_Emscripten_HideWindow(webgl_onscreen->window);
}

const char *
cg_webgl_onscreen_get_id(cg_onscreen_t *onscreen)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_display_t *display = dev->display;
    cg_webgl_display_t *webgl_display = display->winsys;

    return webgl_display->window_id;
}

void
cg_webgl_onscreen_resize(cg_onscreen_t *onscreen,
                         int width,
                         int height)
{
    cg_framebuffer_t *fb = CG_FRAMEBUFFER(onscreen);
    cg_onscreen_webgl_t *webgl_onscreen = onscreen->winsys;

    CGlib_Emscripten_SetWindowSize(webgl_onscreen->window, width, height);
    _cg_framebuffer_winsys_update_size(fb, width, height);
}

const cg_winsys_vtable_t *
_cg_winsys_webgl_get_vtable(void)
{
    static bool vtable_inited = false;
    static cg_winsys_vtable_t vtable;

    if (!vtable_inited) {
        memset(&vtable, 0, sizeof(vtable));

        vtable.id = CG_WINSYS_ID_WEBGL;
        vtable.name = "WEBGL";
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
