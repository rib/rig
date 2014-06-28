/*
 * Copyright (c) 2008-2013, Dave Benson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <config.h>

#include <unistd.h>

#include <rut.h>

#include "rig-protobuf-c-dispatch.h"

struct _RigProtobufCDispatchTimer
{
  int dummy;
};

struct _RigProtobufCDispatchIdle
{
  int dummy;
};

/* Create or destroy a Dispatch */
RigProtobufCDispatch *
rig_protobuf_c_dispatch_new (RutShell *shell,
                             ProtobufCAllocator *allocator)
{
  RigProtobufCDispatch *dispatch = c_new0 (RigProtobufCDispatch, 1);

  dispatch->shell = shell;
  dispatch->allocator = allocator;

  return dispatch;
}

typedef struct _DispatchClosure
{
  ProtobufC_FD fd;
  RigProtobufCDispatch *dispatch;
  void *callback;
  void *user_data;
  RutClosure *rut_closure;
} DispatchClosure;

void
rig_protobuf_c_dispatch_free (RigProtobufCDispatch *dispatch)
{
  c_list_t *l;

  for (l = dispatch->dispatch_closures; l; l = l->next)
    {
      DispatchClosure *closure = l->data;
      if (closure->rut_closure)
        rut_closure_disconnect (closure->rut_closure);
      c_slice_free (DispatchClosure, closure);
    }
  c_list_free (dispatch->dispatch_closures);

  c_free (dispatch);
}

ProtobufCAllocator *
rig_protobuf_c_dispatch_peek_allocator (RigProtobufCDispatch *dispatch)
{
  return dispatch->allocator;
}

static unsigned int
pollfd_events_to_protobuf_events (unsigned ev)
{
  return ((ev & (RUT_POLL_FD_EVENT_IN|RUT_POLL_FD_EVENT_HUP)) ?
          PROTOBUF_C_EVENT_READABLE : 0) |
    ((ev & RUT_POLL_FD_EVENT_OUT) ? PROTOBUF_C_EVENT_WRITABLE : 0);
}

static void
fd_watch_dispatch_cb (void *user_data,
                      int fd,
                      int revents)
{
  DispatchClosure *closure = user_data;
  RigProtobufCDispatchCallback callback = closure->callback;

  callback (closure->fd,
            pollfd_events_to_protobuf_events (revents),
            closure->user_data);
}

static unsigned int
protobuf_events_to_rut_pollfd_events (unsigned int events)
{
  return ((events & PROTOBUF_C_EVENT_READABLE) ? RUT_POLL_FD_EVENT_IN : 0) |
    ((events & PROTOBUF_C_EVENT_WRITABLE) ? RUT_POLL_FD_EVENT_OUT : 0);
}

/* Registering file-descriptors to watch. */
void
rig_protobuf_c_dispatch_watch_fd (RigProtobufCDispatch *dispatch,
                                  ProtobufC_FD fd,
                                  unsigned events,
                                  RigProtobufCDispatchCallback callback,
                                  void *callback_data)
{
  DispatchClosure *closure = c_slice_new (DispatchClosure);

  closure->fd = fd;
  closure->dispatch = dispatch;
  closure->callback = callback;
  closure->user_data = callback_data;
  closure->rut_closure = NULL;

  rig_protobuf_c_dispatch_fd_closed (dispatch, fd);

  rut_poll_shell_add_fd (dispatch->shell,
                         fd,
                         protobuf_events_to_rut_pollfd_events (events),
                         NULL, /* prepare */
                         fd_watch_dispatch_cb,
                         closure);

  dispatch->dispatch_closures =
    c_list_prepend (dispatch->dispatch_closures, closure);
}

void
rig_protobuf_c_dispatch_close_fd (RigProtobufCDispatch *dispatch,
                                  ProtobufC_FD fd)
{
  rig_protobuf_c_dispatch_fd_closed (dispatch, fd);
  close (fd);
}

void
rig_protobuf_c_dispatch_fd_closed (RigProtobufCDispatch *dispatch,
                                   ProtobufC_FD fd)
{
  c_list_t *l;

  c_return_if_fail (fd != -1);

  for (l = dispatch->dispatch_closures; l; l = l->next)
    {
      DispatchClosure *closure = l->data;
      if (closure->fd == fd)
        {
          rut_poll_shell_remove_fd (dispatch->shell, fd);
          dispatch->dispatch_closures =
            c_list_delete_link (dispatch->dispatch_closures, l);
          c_slice_free (DispatchClosure, closure);
          return;
        }
    }
}

RigProtobufCDispatchTimer *
rig_protobuf_c_dispatch_add_timer_millis
                             (RigProtobufCDispatch *dispatch,
                              unsigned millis,
                              RigProtobufCDispatchTimerFunc func,
                              void *func_data)
{
  g_error ("FIXME: implement rig_protobuf_c_dispatch_add_timer_millis");
  return NULL;
}

void
rig_protobuf_c_dispatch_remove_timer (RigProtobufCDispatchTimer *timer)
{
  g_error ("TODO: implement rig_protobuf_c_dispatch_remove_timer");
}

static void
idle_dispatch_cb (void *user_data)
{
  DispatchClosure *closure = user_data;
  RigProtobufCDispatchIdleFunc func = closure->callback;

  func (closure->dispatch, closure->user_data);

  rig_protobuf_c_dispatch_remove_idle ((RigProtobufCDispatchIdle *)closure);
}

RigProtobufCDispatchIdle *
rig_protobuf_c_dispatch_add_idle (RigProtobufCDispatch *dispatch,
                                  RigProtobufCDispatchIdleFunc func,
                                  void *func_data)
{
  DispatchClosure *closure = c_slice_new (DispatchClosure);

  closure->fd = -1;
  closure->dispatch = dispatch;
  closure->callback = func;
  closure->user_data = func_data;
  closure->rut_closure =
    rut_poll_shell_add_idle (dispatch->shell,
                             idle_dispatch_cb,
                             closure,
                             NULL); /* destroy */

  dispatch->dispatch_closures =
    c_list_prepend (dispatch->dispatch_closures, closure);

  return (RigProtobufCDispatchIdle *)closure;
}

void
rig_protobuf_c_dispatch_remove_idle (RigProtobufCDispatchIdle *idle)
{
  DispatchClosure *closure = (DispatchClosure *)idle;
  RigProtobufCDispatch *dispatch = closure->dispatch;
  c_list_t *l;

  for (l = dispatch->dispatch_closures; l; l = l->next)
    {
      if (l->data == idle)
        {
          rut_poll_shell_remove_idle (dispatch->shell, closure->rut_closure);
          dispatch->dispatch_closures =
            c_list_delete_link (dispatch->dispatch_closures, l);
          c_slice_free (DispatchClosure, closure);
          return;
        }
    }
}
