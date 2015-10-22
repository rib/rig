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
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 *   Neil Roberts <neil@linux.intel.com>
 */

#include <cglib-config.h>

#include "cg-winsys-egl-null-private.h"
#include "cg-winsys-egl-private.h"
#include "cg-renderer-private.h"
#include "cg-framebuffer-private.h"
#include "cg-onscreen-private.h"
#include "cg-error-private.h"

static const cg_winsys_egl_vtable_t _cg_winsys_egl_vtable;

typedef struct _cg_display_null_t {
    int egl_surface_width;
    int egl_surface_height;
    bool have_onscreen;
} cg_display_null_t;

static void
_cg_winsys_renderer_disconnect(cg_renderer_t *renderer)
{
    cg_renderer_egl_t *egl_renderer = renderer->winsys;

    eglTerminate(egl_renderer->edpy);

    c_slice_free(cg_renderer_egl_t, egl_renderer);
}

static bool
_cg_winsys_renderer_connect(cg_renderer_t *renderer,
                            cg_error_t **error)
{
    cg_renderer_egl_t *egl_renderer;

    renderer->winsys = c_slice_new0(cg_renderer_egl_t);
    egl_renderer = renderer->winsys;

    egl_renderer->platform_vtable = &_cg_winsys_egl_vtable;

    egl_renderer->edpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    if (!_cg_winsys_egl_renderer_connect_common(renderer, error))
        goto error;

    return true;

error:
    _cg_winsys_renderer_disconnect(renderer);
    return false;
}

static bool
_cg_winsys_egl_device_created(cg_display_t *display,
                               cg_error_t **error)
{
    cg_renderer_t *renderer = display->renderer;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    cg_display_egl_t *egl_display = display->winsys;
    cg_display_null_t *null_display = egl_display->platform;
    const char *error_message;

    egl_display->egl_surface = eglCreateWindowSurface(egl_renderer->edpy,
                                                      egl_display->egl_config,
                                                      (NativeWindowType)NULL,
                                                      NULL);
    if (egl_display->egl_surface == EGL_NO_SURFACE) {
        error_message = "Unable to create EGL window surface";
        goto fail;
    }

    if (!_cg_winsys_egl_make_current(display,
                                     egl_display->egl_surface,
                                     egl_display->egl_surface,
                                     egl_display->egl_context)) {
        error_message = "Unable to eglMakeCurrent with egl surface";
        goto fail;
    }

    eglQuerySurface(egl_renderer->edpy,
                    egl_display->egl_surface,
                    EGL_WIDTH,
                    &null_display->egl_surface_width);

    eglQuerySurface(egl_renderer->edpy,
                    egl_display->egl_surface,
                    EGL_HEIGHT,
                    &null_display->egl_surface_height);

    return true;

fail:
    _cg_set_error(error,
                  CG_WINSYS_ERROR,
                  CG_WINSYS_ERROR_CREATE_CONTEXT,
                  "%s",
                  error_message);
    return false;
}

static bool
_cg_winsys_egl_display_setup(cg_display_t *display,
                             cg_error_t **error)
{
    cg_display_egl_t *egl_display = display->winsys;
    cg_display_null_t *null_display;

    null_display = c_slice_new0(cg_display_null_t);
    egl_display->platform = null_display;

    return true;
}

static void
_cg_winsys_egl_display_destroy(cg_display_t *display)
{
    cg_display_egl_t *egl_display = display->winsys;

    c_slice_free(cg_display_null_t, egl_display->platform);
}

static void
_cg_winsys_egl_cleanup_device(cg_display_t *display)
{
    cg_renderer_t *renderer = display->renderer;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    cg_display_egl_t *egl_display = display->winsys;

    if (egl_display->egl_surface != EGL_NO_SURFACE) {
        eglDestroySurface(egl_renderer->edpy, egl_display->egl_surface);
        egl_display->egl_surface = EGL_NO_SURFACE;
    }
}

static bool
_cg_winsys_egl_onscreen_init(cg_onscreen_t *onscreen,
                             EGLConfig egl_config,
                             cg_error_t **error)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_display_t *display = dev->display;
    cg_display_egl_t *egl_display = display->winsys;
    cg_display_null_t *null_display = egl_display->platform;
    cg_onscreen_egl_t *egl_onscreen = onscreen->winsys;

    if (null_display->have_onscreen) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_CREATE_ONSCREEN,
                      "EGL platform only supports a single onscreen window");
        return false;
    }

    egl_onscreen->egl_surface = egl_display->egl_surface;

    _cg_framebuffer_winsys_update_size(framebuffer,
                                       null_display->egl_surface_width,
                                       null_display->egl_surface_height);
    null_display->have_onscreen = true;

    return true;
}

static void
_cg_winsys_egl_onscreen_deinit(cg_onscreen_t *onscreen)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_display_t *display = dev->display;
    cg_display_egl_t *egl_display = display->winsys;
    cg_display_null_t *null_display = egl_display->platform;

    null_display->have_onscreen = false;
}

static const cg_winsys_egl_vtable_t _cg_winsys_egl_vtable = {
    .display_setup = _cg_winsys_egl_display_setup,
    .display_destroy = _cg_winsys_egl_display_destroy,
    .device_created = _cg_winsys_egl_device_created,
    .cleanup_device = _cg_winsys_egl_cleanup_device,
    .onscreen_init = _cg_winsys_egl_onscreen_init,
    .onscreen_deinit = _cg_winsys_egl_onscreen_deinit
};

const cg_winsys_vtable_t *
_cg_winsys_egl_null_get_vtable(void)
{
    static bool vtable_inited = false;
    static cg_winsys_vtable_t vtable;

    if (!vtable_inited) {
        /* The EGL_NULL winsys is a subclass of the EGL winsys so we
           start by copying its vtable */

        vtable = *_cg_winsys_egl_get_vtable();

        vtable.id = CG_WINSYS_ID_EGL_NULL;
        vtable.name = "EGL_NULL";

        vtable.renderer_connect = _cg_winsys_renderer_connect;
        vtable.renderer_disconnect = _cg_winsys_renderer_disconnect;

        vtable_inited = true;
    }

    return &vtable;
}
