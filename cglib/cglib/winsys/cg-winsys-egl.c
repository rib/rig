/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009,2010,2011,2013 Intel Corporation.
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
 */

#include <cglib-config.h>

#include "cg-i18n-private.h"
#include "cg-util.h"
#include "cg-winsys-egl-private.h"
#include "cg-winsys-private.h"
#include "cg-feature-private.h"
#include "cg-device-private.h"
#include "cg-framebuffer.h"
#include "cg-onscreen-private.h"
#include "cg-renderer-private.h"
#include "cg-onscreen-template-private.h"
#include "cg-gles2-context-private.h"
#include "cg-error-private.h"

#include "cg-private.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef EGL_KHR_create_context
#define EGL_CONTEXT_MAJOR_VERSION_KHR 0x3098
#define EGL_CONTEXT_MINOR_VERSION_KHR 0x30FB
#define EGL_CONTEXT_FLAGS_KHR 0x30FC
#define EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR 0x30FD
#define EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR 0x31BD
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#define EGL_NO_RESET_NOTIFICATION_KHR 0x31BE
#define EGL_LOSE_CONTEXT_ON_RESET_KHR 0x31BF
#define EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR 0x00000001
#define EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR 0x00000002
#define EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR 0x00000004
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR 0x00000001
#define EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR 0x00000002
#endif

#define MAX_EGL_CONFIG_ATTRIBS 30

/* Define a set of arrays containing the functions required from GL
   for each winsys feature */
#define CG_WINSYS_FEATURE_BEGIN(                                               \
        name, namespaces, extension_names, egl_private_flags)                      \
    static const cg_feature_function_t cg_egl_feature_##name##_funcs[] = {
#define CG_WINSYS_FEATURE_FUNCTION(ret, name, args)                            \
    {                                                                          \
        C_STRINGIFY(name), C_STRUCT_OFFSET(cg_renderer_egl_t, pf_##name)       \
    }                                                                          \
    ,
#define CG_WINSYS_FEATURE_END()                                                \
    {                                                                          \
        NULL, 0                                                                \
    }                                                                          \
    ,                                                                          \
    }                                                                          \
    ;
#include "cg-winsys-egl-feature-functions.h"

/* Define an array of features */
#undef CG_WINSYS_FEATURE_BEGIN
#define CG_WINSYS_FEATURE_BEGIN(                                               \
        name, namespaces, extension_names, egl_private_flags)                      \
    {                                                                          \
        255, 255, 0, namespaces, extension_names, egl_private_flags, 0,        \
        cg_egl_feature_##name##_funcs                                      \
    }                                                                          \
    ,
#undef CG_WINSYS_FEATURE_FUNCTION
#define CG_WINSYS_FEATURE_FUNCTION(ret, name, args)
#undef CG_WINSYS_FEATURE_END
#define CG_WINSYS_FEATURE_END()

static const cg_feature_data_t winsys_feature_data[] = {
#include "cg-winsys-egl-feature-functions.h"
};

static const char *
get_error_string(void)
{
    switch (eglGetError()) {
    case EGL_BAD_DISPLAY:
        return "Invalid display";
    case EGL_NOT_INITIALIZED:
        return "Display not initialized";
    case EGL_BAD_ALLOC:
        return "Not enough resources to allocate context";
    case EGL_BAD_ATTRIBUTE:
        return "Invalid attribute";
    case EGL_BAD_CONFIG:
        return "Invalid config";
    case EGL_BAD_CONTEXT:
        return "Invalid context";
    case EGL_BAD_CURRENT_SURFACE:
        return "Invalid current surface";
    case EGL_BAD_MATCH:
        return "Bad match";
    case EGL_BAD_NATIVE_PIXMAP:
        return "Invalid native pixmap";
    case EGL_BAD_NATIVE_WINDOW:
        return "Invalid native window";
    case EGL_BAD_PARAMETER:
        return "Invalid parameter";
    case EGL_BAD_SURFACE:
        return "Invalid surface";
    default:
        c_assert_not_reached();
    }
}

static cg_func_ptr_t
_cg_winsys_renderer_get_proc_address(
    cg_renderer_t *renderer, const char *name, bool in_core)
{
    void *ptr = NULL;

    if (!in_core)
        ptr = eglGetProcAddress(name);

    /* eglGetProcAddress doesn't support fetching core API so we need to
       get that separately with c_module_t */
    if (ptr == NULL)
        c_module_symbol(renderer->libgl_module, name, &ptr);

    return ptr;
}

static void
_cg_winsys_renderer_disconnect(cg_renderer_t *renderer)
{
    /* This function must be overridden by a platform winsys */
    c_assert_not_reached();
}

/* Updates all the function pointers */
static void
check_egl_extensions(cg_renderer_t *renderer)
{
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    const char *egl_extensions;
    char **split_extensions;

    egl_extensions = eglQueryString(egl_renderer->edpy, EGL_EXTENSIONS);
    split_extensions = c_strsplit(egl_extensions, " ", 0 /* max_tokens */);

    CG_NOTE(WINSYS, "  EGL Extensions: %s", egl_extensions);

    egl_renderer->private_features = 0;
    for (unsigned i = 0; i < C_N_ELEMENTS(winsys_feature_data); i++)
        if (_cg_feature_check(renderer,
                              "EGL",
                              winsys_feature_data + i,
                              0,
                              0,
                              CG_DRIVER_GL, /* the driver isn't used */
                              split_extensions,
                              egl_renderer)) {
            egl_renderer->private_features |=
                winsys_feature_data[i].feature_flags_private;
        }

    c_strfreev(split_extensions);
}

bool
_cg_winsys_egl_renderer_connect_common(cg_renderer_t *renderer,
                                       cg_error_t **error)
{
    cg_renderer_egl_t *egl_renderer = renderer->winsys;

    if (!eglInitialize(egl_renderer->edpy,
                       &egl_renderer->egl_version_major,
                       &egl_renderer->egl_version_minor)) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_INIT,
                      "Couldn't initialize EGL");
        return false;
    }

    check_egl_extensions(renderer);

    return true;
}

static bool
_cg_winsys_renderer_connect(cg_renderer_t *renderer,
                            cg_error_t **error)
{
    /* This function must be overridden by a platform winsys */
    c_assert_not_reached();
}

static void
egl_attributes_from_framebuffer_config(
    cg_display_t *display, cg_framebuffer_config_t *config, EGLint *attributes)
{
    cg_renderer_t *renderer = display->renderer;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    int i = 0;

    /* Let the platform add attributes first */
    if (egl_renderer->platform_vtable->add_config_attributes)
        i = egl_renderer->platform_vtable->add_config_attributes(
            display, config, attributes);

    if (config->need_stencil) {
        attributes[i++] = EGL_STENCIL_SIZE;
        attributes[i++] = 2;
    }

    attributes[i++] = EGL_RED_SIZE;
    attributes[i++] = 1;
    attributes[i++] = EGL_GREEN_SIZE;
    attributes[i++] = 1;
    attributes[i++] = EGL_BLUE_SIZE;
    attributes[i++] = 1;

    attributes[i++] = EGL_ALPHA_SIZE;
    attributes[i++] = config->has_alpha ? 1 : EGL_DONT_CARE;

    attributes[i++] = EGL_DEPTH_SIZE;
    attributes[i++] = 1;

    attributes[i++] = EGL_BUFFER_SIZE;
    attributes[i++] = EGL_DONT_CARE;

    attributes[i++] = EGL_RENDERABLE_TYPE;
    attributes[i++] =
        ((renderer->driver == CG_DRIVER_GL || renderer->driver == CG_DRIVER_GL3)
         ? EGL_OPENGL_BIT
         : EGL_OPENGL_ES2_BIT);

    attributes[i++] = EGL_SURFACE_TYPE;
    attributes[i++] = EGL_WINDOW_BIT;

    if (config->samples_per_pixel) {
        attributes[i++] = EGL_SAMPLE_BUFFERS;
        attributes[i++] = 1;
        attributes[i++] = EGL_SAMPLES;
        attributes[i++] = config->samples_per_pixel;
    }

    attributes[i++] = EGL_NONE;

    c_assert(i < MAX_EGL_CONFIG_ATTRIBS);
}

EGLBoolean
_cg_winsys_egl_make_current(cg_display_t *display,
                            EGLSurface draw,
                            EGLSurface read,
                            EGLContext context)
{
    cg_display_egl_t *egl_display = display->winsys;
    cg_renderer_egl_t *egl_renderer = display->renderer->winsys;
    EGLBoolean ret;

    if (egl_display->current_draw_surface == draw &&
        egl_display->current_read_surface == read &&
        egl_display->current_context == context)
        return EGL_TRUE;

    ret = eglMakeCurrent(egl_renderer->edpy, draw, read, context);

    egl_display->current_draw_surface = draw;
    egl_display->current_read_surface = read;
    egl_display->current_context = context;

    return ret;
}

static void
cleanup_device(cg_display_t *display)
{
    cg_renderer_t *renderer = display->renderer;
    cg_display_egl_t *egl_display = display->winsys;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;

    if (egl_display->egl_context != EGL_NO_CONTEXT) {
        _cg_winsys_egl_make_current(
            display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(egl_renderer->edpy, egl_display->egl_context);
        egl_display->egl_context = EGL_NO_CONTEXT;
    }

    if (egl_renderer->platform_vtable->cleanup_device)
        egl_renderer->platform_vtable->cleanup_device(display);
}

static bool
try_create_context(cg_display_t *display, cg_error_t **error)
{
    cg_renderer_t *renderer = display->renderer;
    cg_display_egl_t *egl_display = display->winsys;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    EGLDisplay edpy;
    EGLConfig config;
    EGLint config_count = 0;
    EGLBoolean status;
    EGLint attribs[9];
    EGLint cfg_attribs[MAX_EGL_CONFIG_ATTRIBS];
    const char *error_message;

    c_return_val_if_fail(egl_display->egl_context == NULL, true);

    if (renderer->driver == CG_DRIVER_GL || renderer->driver == CG_DRIVER_GL3)
        eglBindAPI(EGL_OPENGL_API);

    egl_attributes_from_framebuffer_config(
        display, &display->onscreen_template->config, cfg_attribs);

    edpy = egl_renderer->edpy;

    status = eglChooseConfig(edpy, cfg_attribs, &config, 1, &config_count);
    if (status != EGL_TRUE || config_count == 0) {
        error_message = "Unable to find a usable EGL configuration";
        goto fail;
    }

    egl_display->egl_config = config;

    if (display->renderer->driver == CG_DRIVER_GL3) {
        if (!(egl_renderer->private_features &
              CG_EGL_WINSYS_FEATURE_CREATE_CONTEXT)) {
            error_message = "Driver does not support GL 3 contexts";
            goto fail;
        }

        /* Try to get a core profile 3.1 context with no deprecated features */
        attribs[0] = EGL_CONTEXT_MAJOR_VERSION_KHR;
        attribs[1] = 3;
        attribs[2] = EGL_CONTEXT_MINOR_VERSION_KHR;
        attribs[3] = 1;
        attribs[4] = EGL_CONTEXT_FLAGS_KHR;
        attribs[5] = EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR;
        attribs[6] = EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR;
        attribs[7] = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR;
        attribs[8] = EGL_NONE;
    } else if (display->renderer->driver == CG_DRIVER_GLES2) {
        attribs[0] = EGL_CONTEXT_CLIENT_VERSION;
        attribs[1] = 2;
        attribs[2] = EGL_NONE;
    } else
        attribs[0] = EGL_NONE;

    egl_display->egl_context =
        eglCreateContext(edpy, config, EGL_NO_CONTEXT, attribs);

    if (egl_display->egl_context == EGL_NO_CONTEXT) {
        error_message = "Unable to create a suitable EGL context";
        goto fail;
    }

    if (egl_renderer->platform_vtable->device_created &&
        !egl_renderer->platform_vtable->device_created(display, error))
        return false;

    return true;

fail:
    _cg_set_error(error,
                  CG_WINSYS_ERROR,
                  CG_WINSYS_ERROR_CREATE_CONTEXT,
                  "%s",
                  error_message);

    cleanup_device(display);

    return false;
}

static void
_cg_winsys_display_destroy(cg_display_t *display)
{
    cg_renderer_egl_t *egl_renderer = display->renderer->winsys;
    cg_display_egl_t *egl_display = display->winsys;

    c_return_if_fail(egl_display != NULL);

    cleanup_device(display);

    if (egl_renderer->platform_vtable->display_destroy)
        egl_renderer->platform_vtable->display_destroy(display);

    c_slice_free(cg_display_egl_t, display->winsys);
    display->winsys = NULL;
}

static bool
_cg_winsys_display_setup(cg_display_t *display, cg_error_t **error)
{
    cg_display_egl_t *egl_display;
    cg_renderer_t *renderer = display->renderer;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;

    c_return_val_if_fail(display->winsys == NULL, false);

    egl_display = c_slice_new0(cg_display_egl_t);
    display->winsys = egl_display;

#ifdef CG_HAS_WAYLAND_EGL_SERVER_SUPPORT
    if (display->wayland_compositor_display) {
        struct wl_display *wayland_display =
            display->wayland_compositor_display;
        cg_renderer_egl_t *egl_renderer = display->renderer->winsys;

        if (egl_renderer->pf_eglBindWaylandDisplay)
            egl_renderer->pf_eglBindWaylandDisplay(egl_renderer->edpy,
                                                   wayland_display);
    }
#endif

    if (egl_renderer->platform_vtable->display_setup &&
        !egl_renderer->platform_vtable->display_setup(display, error))
        goto error;

    if (!try_create_context(display, error))
        goto error;

    egl_display->found_egl_config = true;

    return true;

error:
    _cg_winsys_display_destroy(display);
    return false;
}

static bool
_cg_winsys_device_init(cg_device_t *dev, cg_error_t **error)
{
    cg_renderer_t *renderer = dev->display->renderer;
    cg_display_egl_t *egl_display = dev->display->winsys;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;

    dev->winsys = c_new0(cg_device_egl_t, 1);

    c_return_val_if_fail(egl_display->egl_context, false);

    memset(dev->winsys_features, 0, sizeof(dev->winsys_features));

    check_egl_extensions(renderer);

    if (!_cg_device_update_features(dev, error))
        return false;

    if (egl_renderer->private_features & CG_EGL_WINSYS_FEATURE_SWAP_REGION) {
        CG_FLAGS_SET(dev->winsys_features, CG_WINSYS_FEATURE_SWAP_REGION,
                     true);
        CG_FLAGS_SET(dev->winsys_features,
                     CG_WINSYS_FEATURE_SWAP_REGION_THROTTLE,
                     true);
    }

    if ((egl_renderer->private_features & CG_EGL_WINSYS_FEATURE_FENCE_SYNC) &&
        _cg_has_private_feature(dev, CG_PRIVATE_FEATURE_OES_EGL_SYNC))
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_FENCE, true);

    if (egl_renderer->private_features & CG_EGL_WINSYS_FEATURE_BUFFER_AGE)
        CG_FLAGS_SET(dev->winsys_features, CG_WINSYS_FEATURE_BUFFER_AGE,
                     true);

    /* NB: We currently only support creating standalone GLES2 contexts
     * for offscreen rendering and so we need a dummy (non-visible)
     * surface to be able to bind those contexts */
    if (egl_display->dummy_surface != EGL_NO_SURFACE &&
        dev->driver == CG_DRIVER_GLES2)
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_GLES2_CONTEXT, true);

    if (egl_renderer->platform_vtable->device_init &&
        !egl_renderer->platform_vtable->device_init(dev, error))
        return false;

    return true;
}

static void
_cg_winsys_device_deinit(cg_device_t *dev)
{
    cg_renderer_t *renderer = dev->display->renderer;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;

    if (egl_renderer->platform_vtable->device_deinit)
        egl_renderer->platform_vtable->device_deinit(dev);

    c_free(dev->winsys);
}

typedef struct _cg_gles2_context_egl_t {
    EGLContext egl_context;
    EGLSurface dummy_surface;
} cg_gles2_context_egl_t;

static void *
_cg_winsys_device_create_gles2_context(cg_device_t *dev,
                                       cg_error_t **error)
{
    cg_renderer_egl_t *egl_renderer = dev->display->renderer->winsys;
    cg_display_egl_t *egl_display = dev->display->winsys;
    EGLint attribs[3];
    EGLContext egl_context;

    attribs[0] = EGL_CONTEXT_CLIENT_VERSION;
    attribs[1] = 2;
    attribs[2] = EGL_NONE;

    egl_context = eglCreateContext(egl_renderer->edpy,
                                   egl_display->egl_config,
                                   egl_display->egl_context,
                                   attribs);
    if (egl_context == EGL_NO_CONTEXT) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_CREATE_GLES2_CONTEXT,
                      "%s",
                      get_error_string());
        return NULL;
    }

    return (void *)egl_context;
}

static void
_cg_winsys_destroy_gles2_context(cg_gles2_context_t *gles2_ctx)
{
    cg_device_t *dev = gles2_ctx->dev;
    cg_display_t *display = dev->display;
    cg_display_egl_t *egl_display = display->winsys;
    cg_renderer_t *renderer = display->renderer;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    EGLContext egl_context = gles2_ctx->winsys;

    c_return_if_fail(egl_display->current_context != egl_context);

    eglDestroyContext(egl_renderer->edpy, egl_context);
}

bool
_cg_egl_find_config(cg_onscreen_t *onscreen,
                    EGLConfig *egl_config,
                    cg_error_t **error)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_display_t *display = dev->display;
    cg_renderer_t *renderer = display->renderer;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    EGLint attributes[MAX_EGL_CONFIG_ATTRIBS];
    EGLint config_count;
    EGLBoolean status;

    egl_attributes_from_framebuffer_config(
        display, &framebuffer->config, attributes);

    status = eglChooseConfig(
        egl_renderer->edpy, attributes, egl_config, 1, &config_count);
    if (status != EGL_TRUE || config_count == 0) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_CREATE_ONSCREEN,
                      "Failed to find a suitable EGL configuration");
        return false;
    }

    return true;
}

static bool
_cg_winsys_onscreen_init(cg_onscreen_t *onscreen,
                         cg_error_t **error)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_display_t *display = dev->display;
    cg_display_egl_t *egl_display = display->winsys;
    cg_renderer_t *renderer = display->renderer;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    EGLConfig egl_config;
    EGLBoolean status;

    c_return_val_if_fail(egl_display->egl_context, false);

    if (!_cg_egl_find_config(onscreen, &egl_config, error))
        return false;

    /* Update the real number of samples_per_pixel now that we have
     * found an egl_config... */
    if (framebuffer->config.samples_per_pixel) {
        EGLint samples;
        status = eglGetConfigAttrib(
            egl_renderer->edpy, egl_config, EGL_SAMPLES, &samples);
        c_return_val_if_fail(status == EGL_TRUE, true);
        framebuffer->samples_per_pixel = samples;
    }

    onscreen->winsys = c_slice_new0(cg_onscreen_egl_t);

    if (egl_renderer->platform_vtable->onscreen_init &&
        !egl_renderer->platform_vtable->onscreen_init(
            onscreen, egl_config, error)) {
        c_slice_free(cg_onscreen_egl_t, onscreen->winsys);
        return false;
    }

    return true;
}

static void
_cg_winsys_onscreen_deinit(cg_onscreen_t *onscreen)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_display_egl_t *egl_display = dev->display->winsys;
    cg_renderer_t *renderer = dev->display->renderer;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    cg_onscreen_egl_t *egl_onscreen = onscreen->winsys;

    /* If we never successfully allocated then there's nothing to do */
    if (egl_onscreen == NULL)
        return;

    if (egl_onscreen->egl_surface != EGL_NO_SURFACE) {
        /* CGlib always needs a valid context bound to something so if we
         * are destroying the onscreen that is currently bound we'll
         * switch back to the dummy drawable. */
        if (egl_display->dummy_surface != EGL_NO_SURFACE &&
            (egl_display->current_draw_surface == egl_onscreen->egl_surface ||
             egl_display->current_read_surface == egl_onscreen->egl_surface)) {
            _cg_winsys_egl_make_current(dev->display,
                                        egl_display->dummy_surface,
                                        egl_display->dummy_surface,
                                        egl_display->current_context);
        }

        if (!eglDestroySurface(egl_renderer->edpy, egl_onscreen->egl_surface))
            c_warning("Failed to destroy EGL surface");
        egl_onscreen->egl_surface = EGL_NO_SURFACE;
    }

    if (egl_renderer->platform_vtable->onscreen_deinit)
        egl_renderer->platform_vtable->onscreen_deinit(onscreen);

    c_slice_free(cg_onscreen_egl_t, onscreen->winsys);
    onscreen->winsys = NULL;
}

static bool
bind_onscreen_with_context(cg_onscreen_t *onscreen,
                           EGLContext egl_context)
{
    cg_framebuffer_t *fb = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = fb->dev;
    cg_onscreen_egl_t *egl_onscreen = onscreen->winsys;

    bool status = _cg_winsys_egl_make_current(dev->display,
                                              egl_onscreen->egl_surface,
                                              egl_onscreen->egl_surface,
                                              egl_context);
    if (status) {
        cg_renderer_t *renderer = dev->display->renderer;
        cg_renderer_egl_t *egl_renderer = renderer->winsys;

        if (egl_renderer->platform_vtable->swap_interval)
            egl_renderer->platform_vtable->swap_interval(onscreen);
        else {
            if (fb->config.swap_throttled)
                eglSwapInterval(egl_renderer->edpy, 1);
            else
                eglSwapInterval(egl_renderer->edpy, 0);
        }
    }

    return status;
}

static bool
bind_onscreen(cg_onscreen_t *onscreen)
{
    cg_framebuffer_t *fb = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = fb->dev;
    cg_display_egl_t *egl_display = dev->display->winsys;

    return bind_onscreen_with_context(onscreen, egl_display->egl_context);
}

static void
_cg_winsys_onscreen_bind(cg_onscreen_t *onscreen)
{
    bind_onscreen(onscreen);
}

#ifndef EGL_BUFFER_AGE_EXT
#define EGL_BUFFER_AGE_EXT 0x313D
#endif

static int
_cg_winsys_onscreen_get_buffer_age(cg_onscreen_t *onscreen)
{
    cg_device_t *dev = CG_FRAMEBUFFER(onscreen)->dev;
    cg_renderer_t *renderer = dev->display->renderer;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    cg_onscreen_egl_t *egl_onscreen = onscreen->winsys;
    EGLSurface surface = egl_onscreen->egl_surface;
    int age;

    if (!(egl_renderer->private_features & CG_EGL_WINSYS_FEATURE_BUFFER_AGE))
        return 0;

    eglQuerySurface(egl_renderer->edpy, surface, EGL_BUFFER_AGE_EXT, &age);

    return age;
}

static void
_cg_winsys_onscreen_swap_region(cg_onscreen_t *onscreen,
                                const int *user_rectangles,
                                int n_rectangles)
{
    cg_device_t *dev = CG_FRAMEBUFFER(onscreen)->dev;
    cg_renderer_t *renderer = dev->display->renderer;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    cg_onscreen_egl_t *egl_onscreen = onscreen->winsys;
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    int framebuffer_height = cg_framebuffer_get_height(framebuffer);
    int *rectangles = c_alloca(sizeof(int) * n_rectangles * 4);
    int i;

    if (egl_renderer->platform_vtable->start_swap)
        egl_renderer->platform_vtable->start_swap(onscreen);

    /* eglSwapBuffersRegion expects rectangles relative to the
     * bottom left corner but we are given rectangles relative to
     * the top left so we need to flip them... */
    memcpy(rectangles, user_rectangles, sizeof(int) * n_rectangles * 4);
    for (i = 0; i < n_rectangles; i++) {
        int *rect = &rectangles[4 * i];
        rect[1] = framebuffer_height - rect[1] - rect[3];
    }

    /* At least for eglSwapBuffers the EGL spec says that the surface to
       swap must be bound to the current context. It looks like Mesa
       also validates that this is the case for eglSwapBuffersRegion so
       we must bind here too */
    _cg_framebuffer_flush_state(CG_FRAMEBUFFER(onscreen),
                                CG_FRAMEBUFFER(onscreen),
                                CG_FRAMEBUFFER_STATE_BIND);

    if (!egl_renderer->pf_eglSwapBuffersRegion(egl_renderer->edpy,
                                               egl_onscreen->egl_surface,
                                               n_rectangles,
                                               rectangles))
        c_warning("Error reported by eglSwapBuffersRegion");


    if (egl_renderer->platform_vtable->end_swap)
        egl_renderer->platform_vtable->end_swap(onscreen);
}

static void
_cg_winsys_onscreen_swap_buffers_with_damage(
    cg_onscreen_t *onscreen, const int *rectangles, int n_rectangles)
{
    cg_device_t *dev = CG_FRAMEBUFFER(onscreen)->dev;
    cg_renderer_t *renderer = dev->display->renderer;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    cg_onscreen_egl_t *egl_onscreen = onscreen->winsys;

    if (egl_renderer->platform_vtable->start_swap)
        egl_renderer->platform_vtable->start_swap(onscreen);

    /* The specification for EGL (at least in 1.4) says that the surface
       needs to be bound to the current context for the swap to work
       although it may change in future. Mesa explicitly checks for this
       and just returns an error if this is not the case so we can't
       just pretend this isn't in the spec. */
    _cg_framebuffer_flush_state(CG_FRAMEBUFFER(onscreen),
                                CG_FRAMEBUFFER(onscreen),
                                CG_FRAMEBUFFER_STATE_BIND);

    if (egl_renderer->pf_eglSwapBuffersWithDamage) {
        cg_framebuffer_t *fb = CG_FRAMEBUFFER(onscreen);
        size_t size = n_rectangles * sizeof(int) * 4;
        int *flipped = alloca(size);
        int i;

        memcpy(flipped, rectangles, size);
        for (i = 0; i < n_rectangles; i++) {
            const int *rect = rectangles + 4 * i;
            int *flip_rect = flipped + 4 * i;
            flip_rect[1] = fb->height - rect[1] - rect[3];
        }

        if (!egl_renderer->pf_eglSwapBuffersWithDamage(egl_renderer->edpy,
                                                       egl_onscreen->egl_surface,
                                                       flipped,
                                                       n_rectangles))
            c_warning("Error reported by eglSwapBuffersWithDamage");
    } else
        eglSwapBuffers(egl_renderer->edpy, egl_onscreen->egl_surface);

    if (egl_renderer->platform_vtable->end_swap)
        egl_renderer->platform_vtable->end_swap(onscreen);
}

static void
_cg_winsys_onscreen_update_swap_throttled(cg_onscreen_t *onscreen)
{
    cg_device_t *dev = CG_FRAMEBUFFER(onscreen)->dev;
    cg_display_egl_t *egl_display = dev->display->winsys;
    cg_onscreen_egl_t *egl_onscreen = onscreen->winsys;

    if (egl_display->current_draw_surface != egl_onscreen->egl_surface)
        return;

    egl_display->current_draw_surface = EGL_NO_SURFACE;

    _cg_winsys_onscreen_bind(onscreen);
}

static EGLDisplay
_cg_winsys_device_egl_get_egl_display(cg_device_t *dev)
{
    cg_renderer_egl_t *egl_renderer = dev->display->renderer->winsys;

    return egl_renderer->edpy;
}

static void
_cg_winsys_save_device(cg_device_t *dev)
{
    cg_device_egl_t *egl_context = dev->winsys;
    cg_display_egl_t *egl_display = dev->display->winsys;

    egl_context->saved_draw_surface = egl_display->current_draw_surface;
    egl_context->saved_read_surface = egl_display->current_read_surface;
}

static bool
_cg_winsys_set_gles2_context(cg_gles2_context_t *gles2_ctx,
                             cg_error_t **error)
{
    cg_device_t *dev = gles2_ctx->dev;
    cg_display_egl_t *egl_display = dev->display->winsys;
    bool status;

    if (gles2_ctx->write_buffer && cg_is_onscreen(gles2_ctx->write_buffer))
        status = bind_onscreen_with_context(
            CG_ONSCREEN(gles2_ctx->write_buffer), gles2_ctx->winsys);
    else
        status = _cg_winsys_egl_make_current(dev->display,
                                             egl_display->dummy_surface,
                                             egl_display->dummy_surface,
                                             gles2_ctx->winsys);

    if (!status) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_MAKE_CURRENT,
                      "Failed to make gles2 context current");
        return false;
    }

    return true;
}

static void
_cg_winsys_restore_context(cg_device_t *dev)
{
    cg_device_egl_t *egl_context = dev->winsys;
    cg_display_egl_t *egl_display = dev->display->winsys;

    _cg_winsys_egl_make_current(dev->display,
                                egl_context->saved_draw_surface,
                                egl_context->saved_read_surface,
                                egl_display->egl_context);
}

#if defined(EGL_KHR_fence_sync) || defined(EGL_KHR_reusable_sync)
static void *
_cg_winsys_fence_add(cg_device_t *dev)
{
    cg_renderer_egl_t *renderer = dev->display->renderer->winsys;
    void *ret;

    if (renderer->pf_eglCreateSync)
        ret = renderer->pf_eglCreateSync(
            renderer->edpy, EGL_SYNC_FENCE_KHR, NULL);
    else
        ret = NULL;

    return ret;
}

static bool
_cg_winsys_fence_is_complete(cg_device_t *dev, void *fence)
{
    cg_renderer_egl_t *renderer = dev->display->renderer->winsys;
    EGLint ret;

    ret = renderer->pf_eglClientWaitSync(
        renderer->edpy, fence, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, 0);
    return (ret == EGL_CONDITION_SATISFIED_KHR);
}

static void
_cg_winsys_fence_destroy(cg_device_t *dev, void *fence)
{
    cg_renderer_egl_t *renderer = dev->display->renderer->winsys;

    renderer->pf_eglDestroySync(renderer->edpy, fence);
}
#endif

static cg_winsys_vtable_t _cg_winsys_vtable = {
    .constraints = CG_RENDERER_CONSTRAINT_USES_EGL |
                   CG_RENDERER_CONSTRAINT_SUPPORTS_CG_GLES2,

    /* This winsys is only used as a base for the EGL-platform
       winsys's so it does not have an ID or a name */
    .renderer_get_proc_address = _cg_winsys_renderer_get_proc_address,
    .renderer_connect = _cg_winsys_renderer_connect,
    .renderer_disconnect = _cg_winsys_renderer_disconnect,
    .display_setup = _cg_winsys_display_setup,
    .display_destroy = _cg_winsys_display_destroy,
    .device_init = _cg_winsys_device_init,
    .device_deinit = _cg_winsys_device_deinit,
    .device_egl_get_egl_display = _cg_winsys_device_egl_get_egl_display,
    .device_create_gles2_context = _cg_winsys_device_create_gles2_context,
    .destroy_gles2_context = _cg_winsys_destroy_gles2_context,
    .onscreen_init = _cg_winsys_onscreen_init,
    .onscreen_deinit = _cg_winsys_onscreen_deinit,
    .onscreen_bind = _cg_winsys_onscreen_bind,
    .onscreen_swap_buffers_with_damage =
        _cg_winsys_onscreen_swap_buffers_with_damage,
    .onscreen_swap_region = _cg_winsys_onscreen_swap_region,
    .onscreen_get_buffer_age = _cg_winsys_onscreen_get_buffer_age,
    .onscreen_update_swap_throttled = _cg_winsys_onscreen_update_swap_throttled,

    /* cg_gles2_context_t related methods */
    .save_device = _cg_winsys_save_device,
    .set_gles2_context = _cg_winsys_set_gles2_context,
    .restore_context = _cg_winsys_restore_context,

#if defined(EGL_KHR_fence_sync) || defined(EGL_KHR_reusable_sync)
    .fence_add = _cg_winsys_fence_add,
    .fence_is_complete = _cg_winsys_fence_is_complete,
    .fence_destroy = _cg_winsys_fence_destroy,
#endif
};

/* XXX: we use a function because no doubt someone will complain
 * about using c99 member initializers because they aren't portable
 * to windows. We want to avoid having to rigidly follow the real
 * order of members since some members are #ifdefd and we'd have
 * to mirror the #ifdefing to add padding etc. For any winsys that
 * can assume the platform has a sane compiler then we can just use
 * c99 initializers for insane platforms they can initialize
 * the members by name in a function.
 */
const cg_winsys_vtable_t *
_cg_winsys_egl_get_vtable(void)
{
    return &_cg_winsys_vtable;
}

#ifdef EGL_KHR_image_base
EGLImageKHR
_cg_egl_create_image(cg_device_t *dev,
                     EGLenum target,
                     EGLClientBuffer buffer,
                     const EGLint *attribs)
{
    cg_display_egl_t *egl_display = dev->display->winsys;
    cg_renderer_egl_t *egl_renderer = dev->display->renderer->winsys;
    EGLContext egl_ctx;

    c_return_val_if_fail(egl_renderer->pf_eglCreateImage, EGL_NO_IMAGE_KHR);

/* The EGL_KHR_image_pixmap spec explicitly states that EGL_NO_CONTEXT must
 * always be used in conjunction with the EGL_NATIVE_PIXMAP_KHR target */
#ifdef EGL_KHR_image_pixmap
    if (target == EGL_NATIVE_PIXMAP_KHR)
        egl_ctx = EGL_NO_CONTEXT;
    else
#endif
    egl_ctx = egl_display->egl_context;

    return egl_renderer->pf_eglCreateImage(
        egl_renderer->edpy, egl_ctx, target, buffer, attribs);
}

void
_cg_egl_destroy_image(cg_device_t *dev, EGLImageKHR image)
{
    cg_renderer_egl_t *egl_renderer = dev->display->renderer->winsys;

    c_return_if_fail(egl_renderer->pf_eglDestroyImage);

    egl_renderer->pf_eglDestroyImage(egl_renderer->edpy, image);
}
#endif

#ifdef EGL_WL_bind_wayland_display
bool
_cg_egl_query_wayland_buffer(cg_device_t *dev,
                             struct wl_resource *buffer,
                             int attribute,
                             int *value)
{
    cg_renderer_egl_t *egl_renderer = dev->display->renderer->winsys;

    c_return_val_if_fail(egl_renderer->pf_eglQueryWaylandBuffer, false);

    return egl_renderer->pf_eglQueryWaylandBuffer(
        egl_renderer->edpy, buffer, attribute, value);
}
#endif
