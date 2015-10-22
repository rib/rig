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

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef __CG_GSOURCE_H__
#define __CG_GSOURCE_H__

#include <glib.h>
#include <cglib/cg-device.h>

CG_BEGIN_DECLS

/**
 * cg_glib_source_new:
 * @dev: A #cg_device_t
 * @priority: The priority of the #GSource
 *
 * Creates a #GSource which handles CGlib's internal system event
 * processing. This can be used as a convenience instead of
 * cg_loop_get_info() and cg_loop_dispatch() in
 * applications that are already using the GLib main loop. After this
 * is called the #GSource should be attached to the main loop using
 * c_source_attach().
 *
 * Applications that manually connect to a #cg_renderer_t before they
 * create a #cg_device_t should instead use
 * cg_glib_renderer_source_new() so that events may be dispatched
 * before a context has been created. In that case you don't need to
 * use this api in addition later, it is simply enough to use
 * cg_glib_renderer_source_new() instead.
 *
 * <note>This api is actually just a thin convenience wrapper around
 * cg_glib_renderer_source_new()</note>
 *
 * Return value: a new #GSource
 *
 * Stability: unstable
 */
GSource *cg_glib_source_new(cg_device_t *dev, int priority);

/**
 * cg_glib_renderer_source_new:
 * @renderer: A #cg_renderer_t
 * @priority: The priority of the #GSource
 *
 * Creates a #GSource which handles CGlib's internal system event
 * processing. This can be used as a convenience instead of
 * cg_loop_get_info() and cg_loop_dispatch() in
 * applications that are already using the GLib main loop. After this
 * is called the #GSource should be attached to the main loop using
 * c_source_attach().
 *
 * Return value: a new #GSource
 *
 * Stability: unstable
 */
GSource *cg_glib_renderer_source_new(cg_renderer_t *renderer, int priority);

CG_END_DECLS

#endif /* __CG_GSOURCE_H__ */
