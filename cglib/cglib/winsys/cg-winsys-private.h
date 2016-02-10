/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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

#ifndef __CG_WINSYS_PRIVATE_H
#define __CG_WINSYS_PRIVATE_H

#include "cg-renderer.h"
#include "cg-onscreen.h"
#include "cg-gles2.h"

#ifdef CG_HAS_XLIB_SUPPORT
#include "cg-texture-pixmap-x11-private.h"
#endif

#ifdef CG_HAS_XLIB_SUPPORT
#include <X11/Xutil.h>
#include "cg-texture-pixmap-x11-private.h"
#endif

#ifdef CG_HAS_EGL_SUPPORT
#include "cg-egl.h"
#endif

#include "cg-loop.h"

uint32_t _cg_winsys_error_domain(void);

#define CG_WINSYS_ERROR (_cg_winsys_error_domain())

typedef enum { /*< prefix=CG_WINSYS_ERROR >*/
    CG_WINSYS_ERROR_INIT,
    CG_WINSYS_ERROR_CREATE_CONTEXT,
    CG_WINSYS_ERROR_CREATE_ONSCREEN,
    CG_WINSYS_ERROR_MAKE_CURRENT,
    CG_WINSYS_ERROR_CREATE_GLES2_CONTEXT,
} cg_winsys_error_t;

typedef enum {
    CG_WINSYS_RECTANGLE_STATE_UNKNOWN,
    CG_WINSYS_RECTANGLE_STATE_DISABLE,
    CG_WINSYS_RECTANGLE_STATE_ENABLE
} cg_winsys_rectangle_state_t;

typedef struct _cg_winsys_vtable_t {
    cg_winsys_id_t id;
    cg_renderer_constraint_t constraints;

    const char *name;

    /* Required functions */

    cg_func_ptr_t (*renderer_get_proc_address)(cg_renderer_t *renderer,
                                               const char *name,
                                               bool in_core);

    bool (*renderer_connect)(cg_renderer_t *renderer, cg_error_t **error);

    void (*renderer_disconnect)(cg_renderer_t *renderer);

    void (*renderer_outputs_changed)(cg_renderer_t *renderer);

    bool (*display_setup)(cg_display_t *display, cg_error_t **error);

    void (*display_destroy)(cg_display_t *display);

    bool (*device_init)(cg_device_t *dev, cg_error_t **error);

    void (*device_deinit)(cg_device_t *dev);

    void *(*device_create_gles2_context)(cg_device_t *dev,
                                         cg_error_t **error);

    bool (*onscreen_init)(cg_onscreen_t *onscreen, cg_error_t **error);

    void (*onscreen_deinit)(cg_onscreen_t *onscreen);

    void (*onscreen_bind)(cg_onscreen_t *onscreen);

    void (*onscreen_swap_buffers_with_damage)(cg_onscreen_t *onscreen,
                                              const int *rectangles,
                                              int n_rectangles);

    void (*onscreen_update_swap_throttled)(cg_onscreen_t *onscreen);

    void (*onscreen_set_visibility)(cg_onscreen_t *onscreen, bool visibility);

    /* Optional functions */

    void (*onscreen_swap_region)(cg_onscreen_t *onscreen,
                                 const int *rectangles,
                                 int n_rectangles);

    void (*onscreen_set_resizable)(cg_onscreen_t *onscreen, bool resizable);

    int (*onscreen_get_buffer_age)(cg_onscreen_t *onscreen);

#ifdef CG_HAS_EGL_SUPPORT
    EGLDisplay (*device_egl_get_egl_display)(cg_device_t *dev);
#endif

#ifdef CG_HAS_XLIB_SUPPORT
    XVisualInfo *(*xlib_get_visual_info)(cg_onscreen_t *onscreen,
                                         cg_error_t **error);
#endif

    uint32_t (*onscreen_x11_get_window_xid)(cg_onscreen_t *onscreen);

#ifdef CG_HAS_WIN32_SUPPORT
    HWND (*onscreen_win32_get_window)(cg_onscreen_t *onscreen);
#endif

#ifdef CG_HAS_XLIB_SUPPORT
    bool (*texture_pixmap_x11_create)(cg_texture_pixmap_x11_t *tex_pixmap);
    void (*texture_pixmap_x11_free)(cg_texture_pixmap_x11_t *tex_pixmap);

    bool (*texture_pixmap_x11_update)(cg_texture_pixmap_x11_t *tex_pixmap,
                                      bool needs_mipmap);

    void (*texture_pixmap_x11_damage_notify)(
        cg_texture_pixmap_x11_t *tex_pixmap);

    cg_texture_t *(*texture_pixmap_x11_get_texture)(
        cg_texture_pixmap_x11_t *tex_pixmap);
#endif

    void (*save_device)(cg_device_t *dev);

    bool (*set_gles2_context)(cg_gles2_context_t *gles2_ctx,
                              cg_error_t **error);

    void (*restore_context)(cg_device_t *dev);

    void (*destroy_gles2_context)(cg_gles2_context_t *gles2_ctx);

    void *(*fence_add)(cg_device_t *dev);

    bool (*fence_is_complete)(cg_device_t *dev, void *fence);

    void (*fence_destroy)(cg_device_t *dev, void *fence);

} cg_winsys_vtable_t;

bool
_cg_winsys_has_feature(cg_device_t *dev,
                       cg_winsys_feature_t feature);

#endif /* __CG_WINSYS_PRIVATE_H */
