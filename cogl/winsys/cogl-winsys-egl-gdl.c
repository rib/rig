/*
 * Cogl
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-winsys-egl-gdl-private.h"
#include "cogl-winsys-egl-private.h"
#include "cogl-renderer-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-onscreen-private.h"
#include "cogl-onscreen-template-private.h"

static const cg_winsys_egl_vtable_t _cg_winsys_egl_vtable;

typedef struct _cg_renderer_gdl_t {
    bool gdl_initialized;
} cg_renderer_gdl_t;

typedef struct _cg_display_gdl_t {
    int egl_surface_width;
    int egl_surface_height;
    bool have_onscreen;
} cg_display_gdl_t;

static void
_cg_winsys_renderer_disconnect(cg_renderer_t *renderer)
{
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    cg_renderer_gdl_t *gdl_renderer = egl_renderer->platform;

    if (gdl_renderer->gdl_initialized)
        gdl_close();

    eglTerminate(egl_renderer->edpy);

    c_slice_free(cg_renderer_egl_t, egl_renderer);
}

static bool
_cg_winsys_renderer_connect(cg_renderer_t *renderer,
                            cg_error_t **error)
{
    cg_renderer_egl_t *egl_renderer;
    cg_renderer_gdl_t *gdl_renderer;
    gdl_ret_t rc = GDL_SUCCESS;
    gdl_display_info_t gdl_display_info;

    renderer->winsys = c_slice_new0(cg_renderer_egl_t);
    egl_renderer = renderer->winsys;

    gdl_renderer = c_slice_new0(cg_renderer_gdl_t);
    egl_renderer->platform = gdl_renderer;

    egl_renderer->platform_vtable = &_cg_winsys_egl_vtable;

    egl_renderer->edpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    if (!_cg_winsys_egl_renderer_connect_common(renderer, error))
        goto error;

    /* Check we can talk to the GDL library */
    rc = gdl_init(NULL);
    if (rc != GDL_SUCCESS) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_INIT,
                      "GDL initialize failed. %s",
                      gdl_get_error_string(rc));
        goto error;
    }

    rc = gdl_get_display_info(GDL_DISPLAY_ID_0, &gdl_display_info);
    if (rc != GDL_SUCCESS) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_INIT,
                      "GDL failed to get display information: %s",
                      gdl_get_error_string(rc));
        gdl_close();
        goto error;
    }

    gdl_close();

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
    cg_display_gdl_t *gdl_display = egl_display->platform;
    const char *error_message;

    egl_display->egl_surface =
        eglCreateWindowSurface(egl_renderer->edpy,
                               egl_display->egl_config,
                               (NativeWindowType)display->gdl_plane,
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
                    &gdl_display->egl_surface_width);

    eglQuerySurface(egl_renderer->edpy,
                    egl_display->egl_surface,
                    EGL_HEIGHT,
                    &gdl_display->egl_surface_height);

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
gdl_plane_init(cg_display_t *display, cg_error_t **error)
{
    bool ret = true;
    gdl_color_space_t colorSpace = GDL_COLOR_SPACE_RGB;
    gdl_pixel_format_t pixfmt = GDL_PF_ARGB_32;
    gdl_rectangle_t dstRect;
    gdl_display_info_t display_info;
    gdl_ret_t rc = GDL_SUCCESS;

    if (!display->gdl_plane) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_CREATE_CONTEXT,
                      "No GDL plane specified with "
                      "cg_gdl_display_set_plane");
        return false;
    }

    rc = gdl_init(NULL);
    if (rc != GDL_SUCCESS) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_CREATE_CONTEXT,
                      "GDL initialize failed. %s",
                      gdl_get_error_string(rc));
        return false;
    }

    rc = gdl_get_display_info(GDL_DISPLAY_ID_0, &display_info);
    if (rc != GDL_SUCCESS) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_CREATE_CONTEXT,
                      "GDL failed to get display infomation: %s",
                      gdl_get_error_string(rc));
        gdl_close();
        return false;
    }

    dstRect.origin.x = 0;
    dstRect.origin.y = 0;
    dstRect.width = display_info.tvmode.width;
    dstRect.height = display_info.tvmode.height;

    /* Configure the plane attribute. */
    rc = gdl_plane_reset(display->gdl_plane);
    if (rc == GDL_SUCCESS)
        rc = gdl_plane_config_begin(display->gdl_plane);

    if (rc == GDL_SUCCESS)
        rc = gdl_plane_set_attr(GDL_PLANE_SRC_COLOR_SPACE, &colorSpace);

    if (rc == GDL_SUCCESS)
        rc = gdl_plane_set_attr(GDL_PLANE_PIXEL_FORMAT, &pixfmt);

    if (rc == GDL_SUCCESS)
        rc = gdl_plane_set_attr(GDL_PLANE_DST_RECT, &dstRect);

    if (rc == GDL_SUCCESS)
        rc = gdl_plane_set_uint(GDL_PLANE_NUM_GFX_SURFACES, 3);

    if (rc == GDL_SUCCESS)
        rc = gdl_plane_config_end(GDL_false);
    else
        gdl_plane_config_end(GDL_true);

    if (rc != GDL_SUCCESS) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_CREATE_CONTEXT,
                      "GDL configuration failed: %s.",
                      gdl_get_error_string(rc));
        ret = false;
    }

    gdl_close();

    return ret;
}

static bool
_cg_winsys_egl_display_setup(cg_display_t *display,
                             cg_error_t **error)
{
    cg_display_egl_t *egl_display = display->winsys;
    cg_display_gdl_t *gdl_display;

    gdl_display = c_slice_new0(cg_display_gdl_t);
    egl_display->platform = gdl_display;

    if (!gdl_plane_init(display, error))
        return false;

    return true;
}

static void
_cg_winsys_egl_display_destroy(cg_display_t *display)
{
    cg_display_egl_t *egl_display = display->winsys;

    c_slice_free(cg_display_gdl_t, egl_display->platform);
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
    cg_display_gdl_t *gdl_display = egl_display->platform;
    cg_onscreen_egl_t *egl_onscreen = onscreen->winsys;

    if (gdl_display->have_onscreen) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_CREATE_ONSCREEN,
                      "EGL platform only supports a single onscreen window");
        return false;
    }

    egl_onscreen->egl_surface = egl_display->egl_surface;

    _cg_framebuffer_winsys_update_size(framebuffer,
                                       gdl_display->egl_surface_width,
                                       gdl_display->egl_surface_height);
    gdl_display->have_onscreen = true;

    return true;
}

static int
_cg_winsys_egl_add_config_attributes(cg_display_t *display,
                                     cg_framebuffer_config_t *config,
                                     EGLint *attributes)
{
    int i = 0;

    /* XXX: Why does the GDL platform choose these by default? */
    attributes[i++] = EGL_BIND_TO_TEXTURE_RGBA;
    attributes[i++] = EGL_TRUE;
    attributes[i++] = EGL_BIND_TO_TEXTURE_RGB;
    attributes[i++] = EGL_TRUE;

    return i;
}

static const cg_winsys_egl_vtable_t _cg_winsys_egl_vtable = {
    .display_setup = _cg_winsys_egl_display_setup,
    .display_destroy = _cg_winsys_egl_display_destroy,
    .device_created = _cg_winsys_egl_device_created,
    .cleanup_device = _cg_winsys_egl_cleanup_device,
    .onscreen_init = _cg_winsys_egl_onscreen_init,
    .add_config_attributes = _cg_winsys_egl_add_config_attributes
};

const cg_winsys_vtable_t *
_cg_winsys_egl_gdl_get_vtable(void)
{
    static bool vtable_inited = false;
    static cg_winsys_vtable_t vtable;

    if (!vtable_inited) {
        /* The EGL_GDL winsys is a subclass of the EGL winsys so we
           start by copying its vtable */

        vtable = *_cg_winsys_egl_get_vtable();

        vtable.id = CG_WINSYS_ID_EGL_GDL;
        vtable.name = "EGL_GDL";

        vtable.renderer_connect = _cg_winsys_renderer_connect;
        vtable.renderer_disconnect = _cg_winsys_renderer_disconnect;

        vtable_inited = true;
    }

    return &vtable;
}
