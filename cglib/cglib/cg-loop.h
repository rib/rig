/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012-2015 Intel Corporation.
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

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef __CG_LOOP_H__
#define __CG_LOOP_H__

#include <cglib/cg-defines.h>
#include <cglib/cg-device.h>

#ifndef CG_HAS_POLL_SUPPORT
#include <poll.h>
#endif

CG_BEGIN_DECLS

/**
 * SECTION:cg-loop
 * @short_description: Functions for integrating CGlib with an
 *   application's main loop
 *
 * CGlib needs to integrate with the application's main loop so that it
 * can internally handle some events from the driver. All CGlib
 * applications must use these functions. They provide enough
 * information to describe the state that CGlib will need to wake up
 * on.
 *
 * An application using the libuv main loop can instead use
 * cg_uv_set_mainloop() as a high level convenience.
 *
 * An application using the GLib main loop can instead use
 * cg_glib_source_new() which provides a #GSource ready to be added
 * to the main loop.
 */

/**
 * cg_poll_fd_event_t:
 * @CG_POLL_FD_EVENT_IN: there is data to read
 * @CG_POLL_FD_EVENT_PRI: data can be written (without blocking)
 * @CG_POLL_FD_EVENT_OUT: there is urgent data to read.
 * @CG_POLL_FD_EVENT_ERR: error condition
 * @CG_POLL_FD_EVENT_HUP: hung up (the connection has been broken, usually
 *                          for pipes and sockets).
 * @CG_POLL_FD_EVENT_NVAL: invalid request. The file descriptor is not open.
 *
 * A bitmask of events that CGlib may need to wake on for a file
 * descriptor. Note that these all have the same values as the
 * corresponding defines for the poll function call on Unix so they
 * may be directly passed to poll.
 *
 * Stability: unstable
 */
typedef enum {
#ifndef CG_HAS_POLL_SUPPORT
    CG_POLL_FD_EVENT_IN = POLLIN,
    CG_POLL_FD_EVENT_PRI = POLLPRI,
    CG_POLL_FD_EVENT_OUT = POLLOUT,
    CG_POLL_FD_EVENT_ERR = POLLERR,
    CG_POLL_FD_EVENT_HUP = POLLHUP,
    CG_POLL_FD_EVENT_NVAL = POLLNVAL
#else
    CG_POLL_FD_EVENT_IN = 1,
    CG_POLL_FD_EVENT_PRI = 2,
    CG_POLL_FD_EVENT_OUT = 3,
    CG_POLL_FD_EVENT_ERR = 4,
    CG_POLL_FD_EVENT_HUP = 5,
    CG_POLL_FD_EVENT_NVAL = 6
#endif
} cg_poll_fd_event_t;

/**
 * cg_poll_fd_t:
 * @fd: The file descriptor to block on
 * @events: A bitmask of events to block on
 * @revents: A bitmask of returned events
 *
 * A struct for describing the state of a file descriptor that CGlib
 * needs to block on. The @events field contains a bitmask of
 * #cg_poll_fd_event_t<!-- -->s that should cause the application to wake
 * up. After the application is woken up from idle it should pass back
 * an array of #cg_poll_fd_t<!-- -->s to CGlib and update the @revents
 * mask to the actual events that occurred on the file descriptor.
 *
 * Note that cg_poll_fd_t is deliberately exactly the same as struct
 * pollfd on Unix so that it can simply be cast when calling poll.
 *
 * Stability: unstable
 */
typedef struct {
    int fd;
    short int events;
    short int revents;
} cg_poll_fd_t;

/**
 * cg_loop_get_info:
 * @renderer: A #cg_renderer_t
 * @poll_fds: A return location for a pointer to an array
 *            of #cg_poll_fd_t<!-- -->s
 * @n_poll_fds: A return location for the number of entries in *@poll_fds
 * @timeout: A return location for the maximum length of time to wait
 *           in microseconds, or -1 to wait indefinitely.
 *
 * Is used to integrate CGlib with an application mainloop that is based
 * on the unix poll(2) api (or select() or something equivalent). This
 * api should be called whenever an application is about to go idle so
 * that CGlib has a chance to describe what file descriptor events it
 * needs to be woken up for.
 *
 * <note>If your application is using the Glib mainloop then you
 * should jump to the cg_glib_source_new() api as a more convenient
 * way of integrating CGlib with the mainloop.</note>
 *
 * After the function is called *@poll_fds will contain a pointer to
 * an array of #cg_poll_fd_t structs describing the file descriptors
 * that CGlib expects. The fd and events members will be updated
 * accordingly. After the application has completed its idle it is
 * expected to either update the revents members directly in this
 * array or to create a copy of the array and update them
 * there.
 *
 * When the application mainloop returns from calling poll(2) (or its
 * equivalent) then it should call cg_loop_dispatch()
 * passing a pointer the array of cg_poll_fd_t<!-- -->s with updated
 * revent values.
 *
 * When using the %CG_WINSYS_ID_WGL winsys (where file descriptors
 * don't make any sense) or %CG_WINSYS_ID_SDL (where the event
 * handling functions of SDL don't allow blocking on a file
 * descriptor) *n_poll_fds is guaranteed to be zero.
 *
 * @timeout will contain a maximum amount of time to wait in
 * microseconds before the application should wake up or -1 if the
 * application should wait indefinitely. This can also be 0 if
 * CGlib needs to be woken up immediately.
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
 */
int cg_loop_get_info(cg_renderer_t *renderer,
                     cg_poll_fd_t **poll_fds,
                     int *n_poll_fds,
                     int64_t *timeout);

/**
 * cg_loop_dispatch:
 * @renderer: A #cg_renderer_t
 * @poll_fds: An array of #cg_poll_fd_t<!-- -->s describing the events
 *            that have occurred since the application went idle.
 * @n_poll_fds: The length of the @poll_fds array.
 *
 * This should be called whenever an application is woken up from
 * going idle in its main loop. The @poll_fds array should contain a
 * list of file descriptors matched with the events that occurred in
 * revents. The events field is ignored. It is safe to pass in extra
 * file descriptors that CGlib didn't request when calling
 * cg_loop_get_info() or a shorter array missing some file
 * descriptors that CGlib requested.
 *
 * <note>If your application didn't originally create a #cg_renderer_t
 * manually then you can easily get a #cg_renderer_t pointer by calling
 * cg_get_renderer().</note>
 *
 * Stability: unstable
 */
void cg_loop_dispatch(cg_renderer_t *renderer,
                      const cg_poll_fd_t *poll_fds,
                      int n_poll_fds);

/**
 * cg_loop_dispatch_fd:
 * @renderer: A #cg_renderer_t
 * @fd: One of the file descriptors returned by
 *      cg_loop_get_info()
 * @events: The events that have been triggered on @fd
 *
 * Dispatches any work associated with a specific file descriptor that
 * was previosly returned by cg_loop_get_info(). Depending
 * on how file descriptors are being integrated into an application
 * mainloop it may convenient to handle the dispatching of each file
 * descriptor separately.
 *
 * <note>If this api is used to dispatch work for each file descriptor
 * separately then the application must still call
 * cg_loop_dispatch() for each iteration of the mainloop
 * with a %NULL poll_fds array, with %0 length so that CGlib can run any
 * idle work or other work not associated with a file
 * descriptor.</note>
 *
 * Stability: unstable
 */
void cg_loop_dispatch_fd(cg_renderer_t *renderer, int fd, int events);

CG_END_DECLS

#endif /* __CG_LOOP_H__ */
