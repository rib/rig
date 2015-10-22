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
 */

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef __CG_WAYLAND_RENDERER_H__
#define __CG_WAYLAND_RENDERER_H__

#include <cglib/cg-types.h>
#include <cglib/cg-renderer.h>

#include <wayland-client.h>

CG_BEGIN_DECLS

/**
 * cg_wayland_renderer_set_foreign_display:
 * @renderer: A #cg_renderer_t
 * @display: A Wayland display
 *
 * Allows you to explicitly control what Wayland display you want CGlib
 * to work with instead of leaving CGlib to automatically connect to a
 * wayland compositor.
 *
 * Stability: unstable
 */
void cg_wayland_renderer_set_foreign_display(cg_renderer_t *renderer,
                                             struct wl_display *display);

/**
 * cg_wayland_renderer_set_event_dispatch_enabled:
 * @renderer: A #cg_renderer_t
 * @enable: The new value
 *
 * Sets whether CGlib should handle calling wl_display_dispatch() and
 * wl_display_flush() as part of its main loop integration via
 * cg_loop_get_info() and cg_loop_dispatch().
 * The default value is %true. When it is enabled the application can
 * register listeners for Wayland interfaces and the callbacks will be
 * invoked during cg_loop_dispatch(). If the application
 * wants to integrate with its own code that is already handling
 * reading from the Wayland display socket, it should disable this to
 * avoid having competing code read from the socket.
 *
 * Stability: unstable
 */
void cg_wayland_renderer_set_event_dispatch_enabled(cg_renderer_t *renderer,
                                                    bool enable);

/**
 * cg_wayland_renderer_get_display:
 * @renderer: A #cg_renderer_t
 *
 * Retrieves the Wayland display that CGlib is using. If a foreign
 * display has been specified using
 * cg_wayland_renderer_set_foreign_display() then that display will
 * be returned. If no foreign display has been specified then the
 * display that CGlib creates internally will be returned unless the
 * renderer has not yet been connected (either implicitly or explicitly by
 * calling cg_renderer_connect()) in which case %NULL is returned.
 *
 * Returns: The wayland display currently associated with @renderer,
 *          or %NULL if the renderer hasn't yet been connected and no
 *          foreign display has been specified.
 *
 * Stability: unstable
 */
struct wl_display *cg_wayland_renderer_get_display(cg_renderer_t *renderer);

CG_END_DECLS

#endif /* __CG_WAYLAND_RENDERER_H__ */
