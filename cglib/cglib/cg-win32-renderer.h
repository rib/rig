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

#ifndef __CG_WIN32_RENDERER_H__
#define __CG_WIN32_RENDERER_H__

#include <cglib/cg-types.h>
#include <cglib/cg-renderer.h>

CG_BEGIN_DECLS

/**
 * cg_win32_renderer_handle_event:
 * @renderer: a #cg_renderer_t
 * @message: A pointer to a win32 MSG struct
 *
 * This function processes a single event; it can be used to hook into
 * external event retrieval (for example that done by Clutter or
 * GDK).
 *
 * Return value: #cg_filter_return_t. %CG_FILTER_REMOVE indicates that
 * CGlib has internally handled the event and the caller should do no
 * further processing. %CG_FILTER_CONTINUE indicates that CGlib is
 * either not interested in the event, or has used the event to update
 * internal state without taking any exclusive action.
 */
cg_filter_return_t cg_win32_renderer_handle_event(cg_renderer_t *renderer,
                                                  MSG *message);

/**
 * cg_win32_filter_func_t:
 * @message: A pointer to a win32 MSG struct
 * @data: The data that was given when the filter was added
 *
 * A callback function that can be registered with
 * cg_win32_renderer_add_filter(). The function should return
 * %CG_FILTER_REMOVE if it wants to prevent further processing or
 * %CG_FILTER_CONTINUE otherwise.
 */
typedef cg_filter_return_t (*cg_win32_filter_func_t)(MSG *message, void *data);

/**
 * cg_win32_renderer_add_filter:
 * @renderer: a #cg_renderer_t
 * @func: the callback function
 * @data: user data passed to @func when called
 *
 * Adds a callback function that will receive all native events. The
 * function can stop further processing of the event by return
 * %CG_FILTER_REMOVE.
 */
void cg_win32_renderer_add_filter(cg_renderer_t *renderer,
                                  cg_win32_filter_func_t func,
                                  void *data);

/**
 * cg_win32_renderer_remove_filter:
 * @renderer: a #cg_renderer_t
 * @func: the callback function
 * @data: user data given when the callback was installed
 *
 * Removes a callback that was previously added with
 * cg_win32_renderer_add_filter().
 */
void cg_win32_renderer_remove_filter(cg_renderer_t *renderer,
                                     cg_win32_filter_func_t func,
                                     void *data);

/**
 * cg_win32_renderer_set_event_retrieval_enabled:
 * @renderer: a #cg_renderer_t
 * @enable: The new value
 *
 * Sets whether CGlib should automatically retrieve messages from
 * Windows. It defaults to %true. It can be set to %false if the
 * application wants to handle its own message retrieval. Note that
 * CGlib still needs to see all of the messages to function properly so
 * the application should call cg_win32_renderer_handle_event() for
 * each message if it disables automatic event retrieval.
 *
 * Stability: unstable
 */
void cg_win32_renderer_set_event_retrieval_enabled(cg_renderer_t *renderer,
                                                   bool enable);

CG_END_DECLS

#endif /* __CG_WIN32_RENDERER_H__ */
