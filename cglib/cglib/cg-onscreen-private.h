/*
 * CGlib
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
 */

#ifndef __CG_ONSCREEN_PRIVATE_H
#define __CG_ONSCREEN_PRIVATE_H

#include <clib.h>

#include "cg-onscreen.h"
#include "cg-framebuffer-private.h"
#include "cg-closure-list-private.h"

#ifdef CG_HAS_WIN32_SUPPORT
#include <windows.h>
#endif

typedef struct _cg_onscreen_event_t {
    c_list_t link;

    cg_frame_info_t *info;
    cg_frame_event_t type;
} cg_onscreen_event_t;

typedef struct _cg_onscreen_queued_dirty_t {
    c_list_t link;

    cg_onscreen_dirty_info_t info;
} cg_onscreen_queued_dirty_t;

struct _cg_onscreen_t {
    cg_framebuffer_t _parent;

#ifdef CG_HAS_X11_SUPPORT
    uint32_t foreign_xid;
    cg_onscreen_x11_mask_callback_t foreign_update_mask_callback;
    void *foreign_update_mask_data;
#endif

#ifdef CG_HAS_WIN32_SUPPORT
    HWND foreign_hwnd;
#endif

#ifdef CG_HAS_EGL_PLATFORM_WAYLAND_SUPPORT
    struct wl_surface *foreign_surface;
#endif

    bool swap_throttled;

    c_list_t frame_closures;

    bool resizable;
    c_list_t resize_closures;

    c_list_t dirty_closures;

    c_list_t events_queue;
    c_list_t dirty_queue;
    cg_closure_t *dispatch_idle;
    bool pending_resize_notify;

    int64_t frame_counter;
    int64_t swap_frame_counter; /* frame counter at last all to
                                * cg_onscreen_swap_region() or
                                * cg_onscreen_swap_buffers() */
    c_queue_t pending_frame_infos;

    void *winsys;
};

void _cg_framebuffer_winsys_update_size(cg_framebuffer_t *framebuffer,
                                        int width,
                                        int height);

void _cg_onscreen_queue_event(cg_onscreen_t *onscreen,
                              cg_frame_event_t type,
                              cg_frame_info_t *info);

void _cg_onscreen_queue_dirty(cg_onscreen_t *onscreen,
                              const cg_onscreen_dirty_info_t *info);

void _cg_onscreen_queue_full_dirty(cg_onscreen_t *onscreen);

void _cg_onscreen_queue_resize_notify(cg_onscreen_t *onscreen);

#endif /* __CG_ONSCREEN_PRIVATE_H */
