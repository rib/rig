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

#ifndef __CG_RENDERER_PRIVATE_H
#define __CG_RENDERER_PRIVATE_H

#include <cmodule.h>

#include "cg-object-private.h"
#include "cg-winsys-private.h"
#include "cg-driver.h"
#include "cg-texture-driver.h"
#include "cg-device.h"
#include "cg-closure-list-private.h"

#ifdef CG_HAS_XLIB_SUPPORT
#include <X11/Xlib.h>
#endif

#if defined(CG_HAS_EGL_PLATFORM_WAYLAND_SUPPORT)
#include <wayland-client.h>
#endif

struct _cg_renderer_t {
    cg_object_t _parent;
    bool connected;
    cg_driver_t driver_override;
    const cg_driver_vtable_t *driver_vtable;
    const cg_texture_driver_t *texture_driver;
    const cg_winsys_vtable_t *winsys_vtable;
    cg_winsys_id_t winsys_id_override;
    c_llist_t *constraints;

    c_array_t *poll_fds;
    int poll_fds_age;
    c_llist_t *poll_sources;

#ifdef __EMSCRIPTEN__
    bool browser_idle_queued;
#endif

    c_list_t idle_closures;

    c_llist_t *outputs;

#ifdef CG_HAS_XLIB_SUPPORT
    Display *foreign_xdpy;
    bool xlib_enable_event_retrieval;
#endif

#ifdef CG_HAS_WIN32_SUPPORT
    bool win32_enable_event_retrieval;
#endif

    cg_driver_t driver;
    unsigned long private_features
    [CG_FLAGS_N_LONGS_FOR_SIZE(CG_N_PRIVATE_FEATURES)];
#ifndef HAVE_DIRECTLY_LINKED_GL_LIBRARY
    c_module_t *libgl_module;
#endif

#if defined(CG_HAS_EGL_PLATFORM_WAYLAND_SUPPORT)
    struct wl_display *foreign_wayland_display;
    bool wayland_enable_event_dispatch;
#endif

#if defined(CG_HAS_EGL_PLATFORM_KMS_SUPPORT)
    int kms_fd;
#endif

#ifdef CG_HAS_SDL_SUPPORT
    bool sdl_event_type_set;
    uint32_t sdl_event_type;
#endif

    /* List of callback functions that will be given every native event */
    c_sllist_t *event_filters;
    void *winsys;
};

/* Mask of constraints that effect driver selection. All of the other
 * constraints effect only the winsys selection */
#define CG_RENDERER_DRIVER_CONSTRAINTS CG_RENDERER_CONSTRAINT_SUPPORTS_CG_GLES2

typedef cg_filter_return_t (*cg_native_filter_func_t)(void *native_event,
                                                      void *data);

cg_filter_return_t _cg_renderer_handle_native_event(cg_renderer_t *renderer,
                                                    void *event);

void _cg_renderer_add_native_filter(cg_renderer_t *renderer,
                                    cg_native_filter_func_t func,
                                    void *data);

void _cg_renderer_remove_native_filter(cg_renderer_t *renderer,
                                       cg_native_filter_func_t func,
                                       void *data);

void *_cg_renderer_get_proc_address(cg_renderer_t *renderer,
                                    const char *name,
                                    bool in_core);

#endif /* __CG_RENDERER_PRIVATE_H */
