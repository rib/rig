/*
 * Rut
 *
 * Copyright (C) 2012,2014  Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef __RUT_POLL_H__
#define __RUT_POLL_H__

#include "rut-types.h"
#include "rut-closure.h"

G_BEGIN_DECLS

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
 * RutPollFDEvent:
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
#if defined (linux) || defined (__APPLE__)
#include <poll.h>
typedef enum
{
  RUT_POLL_FD_EVENT_IN = POLLIN,
  RUT_POLL_FD_EVENT_PRI = POLLPRI,
  RUT_POLL_FD_EVENT_OUT = POLLOUT,
  RUT_POLL_FD_EVENT_ERR = POLLERR,
  RUT_POLL_FD_EVENT_HUP = POLLHUP,
  RUT_POLL_FD_EVENT_NVAL = POLLNVAL
} RutPollFDEvent;
#else
typedef enum
{
  RUT_POLL_FD_EVENT_IN = 1,
  RUT_POLL_FD_EVENT_PRI = 1<<1,
  RUT_POLL_FD_EVENT_OUT = 1<<2,
  RUT_POLL_FD_EVENT_ERR = 1<<3,
  RUT_POLL_FD_EVENT_HUP = 1<<4,
  RUT_POLL_FD_EVENT_NVAL = 1<<5
} RutPollFDEvent;
#endif

typedef struct _UVSource UVSource;

void
rut_poll_shell_remove_fd (RutShell *shell, int fd);

typedef int64_t (*RutPollPrepareCallback) (void *user_data);
typedef void (*RutPollDispatchCallback) (void *user_data, int revents);

void
rut_poll_renderer_modify_fd (RutShell *shell,
                             int fd,
                             RutPollFDEvent events);

typedef struct _RutPollSource RutPollSource;

RutPollSource *
rut_poll_shell_add_fd (RutShell *shell,
                       int fd,
                       RutPollFDEvent events,
                       int64_t (*prepare) (void *user_data),
                       void (*dispatch) (void *user_data,
                                         int fd,
                                         int revents),
                       void *user_data);

RutPollSource *
rut_poll_shell_add_source (RutShell *shell,
                           int64_t (*prepare) (void *user_data),
                           void (*dispatch) (void *user_data,
                                             int fd,
                                             int revents),
                           void *user_data);

void
rut_poll_shell_remove_source (RutShell *shell,
                              RutPollSource *source);

RutClosure *
rut_poll_shell_add_idle (RutShell *shell,
                         void (*idle_cb) (void *user_data),
                         void *user_data,
                         void (*destroy_cb) (void *user_data));

void
rut_poll_shell_remove_idle (RutShell *shell, RutClosure *idle);

void
rut_poll_init (RutShell *shell);

void
rut_poll_run (RutShell *shell);

void
rut_poll_quit (RutShell *shell);

G_END_DECLS

#endif /* __RUT_POLL_H__ */
