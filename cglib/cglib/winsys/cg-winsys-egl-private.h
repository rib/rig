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
 */

#ifndef __CG_WINSYS_EGL_PRIVATE_H
#define __CG_WINSYS_EGL_PRIVATE_H

#include "cg-defines.h"
#include "cg-winsys-private.h"
#include "cg-device.h"
#include "cg-device-private.h"
#include "cg-framebuffer-private.h"

/* XXX: depending on what version of Mesa you have then
 * eglQueryWaylandBuffer may take a wl_buffer or wl_resource argument
 * and the EGL header will only forward declare the corresponding
 * type.
 *
 * The use of wl_buffer has been deprecated and so internally we
 * assume that eglQueryWaylandBuffer takes a wl_resource but for
 * compatibility we forward declare wl_resource in case we are
 * building with EGL headers that still use wl_buffer.
 *
 * Placing the forward declaration here means it comes before we
 * #include cg-winsys-egl-feature-functions.h bellow which
 * declares lots of function pointers for accessing EGL extensions
 * and cg-winsys-egl.c will include this header before it also
 * includes cg-winsys-egl-feature-functions.h that may depend
 * on this type.
 */
#ifdef EGL_WL_bind_wayland_display
struct wl_resource;
#endif

typedef struct _cg_winsys_egl_vtable_t {
    bool (*display_setup)(cg_display_t *display, cg_error_t **error);
    void (*display_destroy)(cg_display_t *display);

    bool (*device_created)(cg_display_t *display, cg_error_t **error);

    void (*cleanup_device)(cg_display_t *display);

    bool (*device_init)(cg_device_t *dev, cg_error_t **error);

    void (*device_deinit)(cg_device_t *dev);

    bool (*onscreen_init)(cg_onscreen_t *onscreen,
                          EGLConfig config,
                          cg_error_t **error);
    void (*onscreen_deinit)(cg_onscreen_t *onscreen);

    int (*add_config_attributes)(cg_display_t *display,
                                 cg_framebuffer_config_t *config,
                                 EGLint *attributes);

    void (*swap_interval)(cg_onscreen_t *onscreen);
    
    void (*start_swap)(cg_onscreen_t *onscreen);
    void (*end_swap)(cg_onscreen_t *onscreen);
} cg_winsys_egl_vtable_t;

typedef enum _cg_egl_winsys_feature_t {
    CG_EGL_WINSYS_FEATURE_SWAP_REGION = 1L << 0,
    CG_EGL_WINSYS_FEATURE_EGL_IMAGE_FROM_X11_PIXMAP = 1L << 1,
    CG_EGL_WINSYS_FEATURE_EGL_IMAGE_FROM_WAYLAND_BUFFER = 1L << 2,
    CG_EGL_WINSYS_FEATURE_CREATE_CONTEXT = 1L << 3,
    CG_EGL_WINSYS_FEATURE_BUFFER_AGE = 1L << 4,
    CG_EGL_WINSYS_FEATURE_FENCE_SYNC = 1L << 5,
    CG_EGL_WINSYS_FEATURE_SURFACELESS_CONTEXT = 1L << 6
} cg_egl_winsys_feature_t;

typedef struct _cg_renderer_egl_t {
    cg_egl_winsys_feature_t private_features;

    EGLDisplay edpy;

    EGLint egl_version_major;
    EGLint egl_version_minor;

    cg_closure_t *resize_notify_idle;

    /* Data specific to the EGL platform */
    void *platform;
    /* vtable for platform specific parts */
    const cg_winsys_egl_vtable_t *platform_vtable;

/* Function pointers for EGL specific extensions */
#define CG_WINSYS_FEATURE_BEGIN(a, b, c, d)

#define CG_WINSYS_FEATURE_FUNCTION(ret, name, args)                            \
    ret(APIENTRY *pf_##name) args;

#define CG_WINSYS_FEATURE_END()

#include "cg-winsys-egl-feature-functions.h"

#undef CG_WINSYS_FEATURE_BEGIN
#undef CG_WINSYS_FEATURE_FUNCTION
#undef CG_WINSYS_FEATURE_END
} cg_renderer_egl_t;

typedef struct _cg_display_egl_t {
    EGLContext egl_context;
    EGLSurface dummy_surface;
    EGLSurface egl_surface;

    EGLConfig egl_config;
    bool found_egl_config;

    EGLSurface current_read_surface;
    EGLSurface current_draw_surface;
    EGLContext current_context;

    /* Platform specific display data */
    void *platform;
} cg_display_egl_t;

typedef struct _cg_device_egl_t {
    EGLSurface saved_draw_surface;
    EGLSurface saved_read_surface;
} cg_device_egl_t;

typedef struct _cg_onscreen_egl_t {
    EGLSurface egl_surface;

    bool pending_resize_notify;

    /* Platform specific data */
    void *platform;
} cg_onscreen_egl_t;

const cg_winsys_vtable_t *_cg_winsys_egl_get_vtable(void);

EGLBoolean _cg_winsys_egl_make_current(cg_display_t *display,
                                       EGLSurface draw,
                                       EGLSurface read,
                                       EGLContext context);

bool
_cg_egl_find_config(cg_onscreen_t *onscreen,
                    EGLConfig *egl_config,
                    cg_error_t **error);

#ifdef EGL_KHR_image_base
EGLImageKHR _cg_egl_create_image(cg_device_t *dev,
                                 EGLenum target,
                                 EGLClientBuffer buffer,
                                 const EGLint *attribs);

void _cg_egl_destroy_image(cg_device_t *dev, EGLImageKHR image);
#endif

#ifdef EGL_WL_bind_wayland_display
bool _cg_egl_query_wayland_buffer(cg_device_t *dev,
                                  struct wl_resource *buffer,
                                  int attribute,
                                  int *value);
#endif

bool _cg_winsys_egl_renderer_connect_common(cg_renderer_t *renderer,
                                            cg_error_t **error);

#endif /* __CG_WINSYS_EGL_PRIVATE_H */
