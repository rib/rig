/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2013 Intel Corporation.
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

#ifndef __CG_POLL_PRIVATE_H__
#define __CG_POLL_PRIVATE_H__

#include "cg-loop.h"
#include "cg-renderer.h"
#include "cg-closure-list-private.h"

void _cg_loop_remove_fd(cg_renderer_t *renderer, int fd);

typedef int64_t (*cg_poll_prepare_callback_t)(void *user_data);
typedef void (*cg_poll_dispatch_callback_t)(void *user_data, int revents);

void _cg_loop_add_fd(cg_renderer_t *renderer,
                              int fd,
                              cg_poll_fd_event_t events,
                              cg_poll_prepare_callback_t prepare,
                              cg_poll_dispatch_callback_t dispatch,
                              void *user_data);

void _cg_loop_modify_fd(cg_renderer_t *renderer,
                                 int fd,
                                 cg_poll_fd_event_t events);

typedef struct _cg_poll_source_t cg_poll_source_t;

cg_poll_source_t *
_cg_loop_add_source(cg_renderer_t *renderer,
                             cg_poll_prepare_callback_t prepare,
                             cg_poll_dispatch_callback_t dispatch,
                             void *user_data);

void _cg_loop_remove_source(cg_renderer_t *renderer,
                                     cg_poll_source_t *source);

typedef void (*cg_idle_callback_t)(void *user_data);

cg_closure_t *
_cg_loop_add_idle(cg_renderer_t *renderer,
                           cg_idle_callback_t idle_cb,
                           void *user_data,
                           cg_user_data_destroy_callback_t destroy_cb);

#endif /* __CG_POLL_PRIVATE_H__ */
