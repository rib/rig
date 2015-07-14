/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012-2014 Intel Corporation.
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
 */

#ifndef __RUT_POLL_H__
#define __RUT_POLL_H__

#include "rut-types.h"
#include "rut-closure.h"

C_BEGIN_DECLS

/**
 * SECTION:rut-poll
 * @short_description: Functions for integrating Rut with an
 *   application's main loop
 *
 * Rut needs to integrate with the application's main loop so that it
 * can internally handle some events from the driver. All Rut
 * applications must use these functions. They provide enough
 * information to describe the state that Rut will need to wake up
 * on. An application using the GLib main loop can instead use
 * rut_glib_source_new() which provides a #GSource ready to be added
 * to the main loop.
 */

/**
 * rut_poll_fd_event_t:
 * @RUT_POLL_FD_EVENT_IN: there is data to read
 * @RUT_POLL_FD_EVENT_PRI: data can be written (without blocking)
 * @RUT_POLL_FD_EVENT_OUT: there is urgent data to read.
 * @RUT_POLL_FD_EVENT_ERR: error condition
 * @RUT_POLL_FD_EVENT_HUP: hung up (the connection has been broken, usually
 *                          for pipes and sockets).
 * @RUT_POLL_FD_EVENT_NVAL: invalid request. The file descriptor is not open.
 *
 * A bitmask of events that Rut may need to wake on for a file
 * descriptor. Note that these all have the same values as the
 * corresponding defines for the poll function call on Unix so they
 * may be directly passed to poll.
 *
 * Since: 1.10
 * Stability: unstable
 */
#if defined(__linux__) || defined(__APPLE__)
#include <poll.h>
typedef enum {
    RUT_POLL_FD_EVENT_IN = POLLIN,
    RUT_POLL_FD_EVENT_PRI = POLLPRI,
    RUT_POLL_FD_EVENT_OUT = POLLOUT,
    RUT_POLL_FD_EVENT_ERR = POLLERR,
    RUT_POLL_FD_EVENT_HUP = POLLHUP,
    RUT_POLL_FD_EVENT_NVAL = POLLNVAL
} rut_poll_fd_event_t;
#else
typedef enum {
    RUT_POLL_FD_EVENT_IN = 1,
    RUT_POLL_FD_EVENT_PRI = 1 << 1,
    RUT_POLL_FD_EVENT_OUT = 1 << 2,
    RUT_POLL_FD_EVENT_ERR = 1 << 3,
    RUT_POLL_FD_EVENT_HUP = 1 << 4,
    RUT_POLL_FD_EVENT_NVAL = 1 << 5
} rut_poll_fd_event_t;
#endif

typedef struct _uv_source_t uv_source_t;

void rut_poll_shell_remove_fd(rut_shell_t *shell, int fd);

typedef int64_t (*rut_poll_prepare_callback_t)(void *user_data);
typedef void (*rut_poll_dispatch_callback_t)(void *user_data, int revents);

void rut_poll_renderer_modify_fd(rut_shell_t *shell,
                                 int fd,
                                 rut_poll_fd_event_t events);

typedef struct _rut_poll_source_t rut_poll_source_t;

rut_poll_source_t *
rut_poll_shell_add_fd(rut_shell_t *shell,
                      int fd,
                      rut_poll_fd_event_t events,
                      int64_t (*prepare)(void *user_data),
                      void (*dispatch)(void *user_data, int fd, int revents),
                      void *user_data);

rut_poll_source_t *rut_poll_shell_add_source(
    rut_shell_t *shell,
    int64_t (*prepare)(void *user_data),
    void (*dispatch)(void *user_data, int fd, int revents),
    void *user_data);

void rut_poll_shell_remove_source(rut_shell_t *shell,
                                  rut_poll_source_t *source);

void rut_poll_shell_add_idle(rut_shell_t *shell, rut_closure_t *closure);
void rut_poll_shell_remove_idle(rut_shell_t *shell, rut_closure_t *idle);

/* XXX: Deprecated, code should migrate to above api... */
rut_closure_t *rut_poll_shell_add_idle_FIXME(rut_shell_t *shell,
                                             void (*idle_cb)(void *user_data),
                                             void *user_data,
                                             void (*destroy_cb)(void *user_data));
void rut_poll_shell_remove_idle_FIXME(rut_shell_t *shell, rut_closure_t *idle);

rut_closure_t *rut_poll_shell_add_sigchild(rut_shell_t *shell,
                                           void (*sigchild_cb)(void *user_data),
                                           void *user_data,
                                           void (*destroy_cb)(void *user_data));
void rut_poll_shell_remove_sigchild(rut_shell_t *shell, rut_closure_t *sigchild);

void rut_poll_init(rut_shell_t *shell, rut_shell_t *main_shell);

void rut_poll_run(rut_shell_t *shell);

void rut_poll_quit(rut_shell_t *shell);

void rut_poll_shell_integrate_cg_events_via_libuv(rut_shell_t *shell);

C_END_DECLS

#endif /* __RUT_POLL_H__ */
