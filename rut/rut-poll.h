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
#ifdef unix
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

/**
 * RutPollFD:
 * @fd: The file descriptor to block on
 * @events: A bitmask of events to block on
 * @revents: A bitmask of returned events
 *
 * A struct for describing the state of a file descriptor that Rut
 * needs to block on. The @events field contains a bitmask of
 * #RutPollFDEvent<!-- -->s that should cause the application to wake
 * up. After the application is woken up from idle it should pass back
 * an array of #RutPollFD<!-- -->s to Rut and update the @revents
 * mask to the actual events that occurred on the file descriptor.
 *
 * Note that RutPollFD is deliberately exactly the same as struct
 * pollfd on Unix so that it can simply be cast when calling poll.
 *
 * Since: 1.10
 * Stability: unstable
 */
typedef struct {
  int fd;
  short int events;
  short int revents;
} RutPollFD;

/**
 * rut_poll_shell_get_info:
 * @shell: A #RutShell
 * @poll_fds: A return location for a pointer to an array
 *            of #RutPollFD<!-- -->s
 * @n_poll_fds: A return location for the number of entries in *@poll_fds
 * @timeout: A return location for the maximum length of time to wait
 *           in microseconds, or -1 to wait indefinitely.
 *
 * Is used to integrate Rut with an application mainloop that is based
 * on the unix poll(2) api (or select() or something equivalent). This
 * api should be called whenever an application is about to go idle so
 * that Rut has a chance to describe what file descriptor events it
 * needs to be woken up for.
 *
 * <note>If your application is using the Glib mainloop then you
 * should jump to the rut_glib_source_new() api as a more convenient
 * way of integrating Rut with the mainloop.</note>
 *
 * After the function is called *@poll_fds will contain a pointer to
 * an array of #RutPollFD structs describing the file descriptors
 * that Rut expects. The fd and events members will be updated
 * accordingly. After the application has completed its idle it is
 * expected to either update the revents members directly in this
 * array or to create a copy of the array and update them
 * there.
 *
 * When the application mainloop returns from calling poll(2) (or its
 * equivalent) then it should call rut_poll_shell_dispatch()
 * passing a pointer the array of RutPollFD<!-- -->s with updated
 * revent values.
 *
 * When using the %RUT_WINSYS_ID_WGL winsys (where file descriptors
 * don't make any sense) or %RUT_WINSYS_ID_SDL (where the event
 * handling functions of SDL don't allow blocking on a file
 * descriptor) *n_poll_fds is guaranteed to be zero.
 *
 * @timeout will contain a maximum amount of time to wait in
 * microseconds before the application should wake up or -1 if the
 * application should wait indefinitely. This can also be 0 if
 * Rut needs to be woken up immediately.
 *
 * Return value: A "poll fd state age" that changes whenever the set
 *               of poll_fds has changed. If this API is being used to
 *               integrate with another system mainloop api then
 *               knowing if the set of file descriptors and events has
 *               really changed can help avoid redundant work
 *               depending the api. The age isn't guaranteed to change
 *               when the timeout changes.
 *
 * Stability: unstable
 * Since: 1.16
 */
int
rut_poll_shell_get_info (RutShell *shell,
                         RutPollFD **poll_fds,
                         int *n_poll_fds,
                         int64_t *timeout);

/**
 * rut_poll_shell_dispatch:
 * @shell: A #RutShell
 * @poll_fds: An array of #RutPollFD<!-- -->s describing the events
 *            that have occurred since the application went idle.
 * @n_poll_fds: The length of the @poll_fds array.
 *
 * This should be called whenever an application is woken up from
 * going idle in its main loop. The @poll_fds array should contain a
 * list of file descriptors matched with the events that occurred in
 * revents. The events field is ignored. It is safe to pass in extra
 * file descriptors that Rut didn't request when calling
 * rut_poll_shell_get_info() or a shorter array missing some file
 * descriptors that Rut requested.
 *
 * <note>If your application didn't originally create a #RutShell
 * manually then you can easily get a #RutShell pointer by calling
 * rut_get_shell().</note>
 *
 * Stability: unstable
 * Since: 1.16
 */
void
rut_poll_shell_dispatch (RutShell *shell,
                         const RutPollFD *poll_fds,
                         int n_poll_fds);

void
rut_poll_shell_remove_fd (RutShell *shell, int fd);

typedef int64_t (*RutPollPrepareCallback) (void *user_data);
typedef void (*RutPollDispatchCallback) (void *user_data, int revents);

void
rut_poll_renderer_modify_fd (RutShell *shell,
                             int fd,
                             RutPollFDEvent events);

void
rut_poll_shell_add_fd (RutShell *shell,
                       int fd,
                       RutPollFDEvent events,
                       int64_t (*prepare) (void *user_data),
                       void (*dispatch) (void *user_data,
                                         int revents),
                       void *user_data);

typedef struct _RutPollSource RutPollSource;

RutPollSource *
rut_poll_shell_add_source (RutShell *shell,
                           int64_t (*prepare) (void *user_data),
                           void (*dispatch) (void *user_data,
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
rut_poll_init (RutShell *shell);

G_END_DECLS

#endif /* __RUT_POLL_H__ */
