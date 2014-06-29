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

#ifndef _RIG_PROTOBUF_C_DISPATCH_H_
#define _RIG_PROTOBUF_C_DISPATCH_H_

typedef struct _rig_protobuf_c_dispatch_t rig_protobuf_c_dispatch_t;
typedef struct _rig_protobuf_c_dispatch_timer_t rig_protobuf_c_dispatch_timer_t;
typedef struct _rig_protobuf_c_dispatch_idle_t rig_protobuf_c_dispatch_idle_t;

#include <rut.h>

#include <protobuf-c/protobuf-c.h>

typedef enum {
    PROTOBUF_C_EVENT_READABLE = (1 << 0),
    PROTOBUF_C_EVENT_WRITABLE = (1 << 1)
} ProtobufC_Events;

#ifdef WIN32
typedef SOCKET ProtobufC_FD;
#else
typedef int ProtobufC_FD;
#endif

/* Create or destroy a Dispatch */
rig_protobuf_c_dispatch_t *
rig_protobuf_c_dispatch_new(rut_shell_t *shell, ProtobufCAllocator *allocator);

void rig_protobuf_c_dispatch_free(rig_protobuf_c_dispatch_t *dispatch);

ProtobufCAllocator *
rig_protobuf_c_dispatch_peek_allocator(rig_protobuf_c_dispatch_t *);

typedef void (*rig_protobuf_c_dispatch_callback_t)(ProtobufC_FD fd,
                                                   unsigned events,
                                                   void *callback_data);

/* Registering file-descriptors to watch. */
void
rig_protobuf_c_dispatch_watch_fd(rig_protobuf_c_dispatch_t *dispatch,
                                 ProtobufC_FD fd,
                                 unsigned events,
                                 rig_protobuf_c_dispatch_callback_t callback,
                                 void *callback_data);

void rig_protobuf_c_dispatch_close_fd(rig_protobuf_c_dispatch_t *dispatch,
                                      ProtobufC_FD fd);

void rig_protobuf_c_dispatch_fd_closed(rig_protobuf_c_dispatch_t *dispatch,
                                       ProtobufC_FD fd);

/* Timers */
typedef void (*rig_protobuf_c_dispatch_timer_func_t)(
    rig_protobuf_c_dispatch_t *dispatch, void *func_data);
rig_protobuf_c_dispatch_timer_t *rig_protobuf_c_dispatch_add_timer_millis(
    rig_protobuf_c_dispatch_t *dispatch,
    unsigned milliseconds,
    rig_protobuf_c_dispatch_timer_func_t func,
    void *func_data);

void rig_protobuf_c_dispatch_remove_timer(rig_protobuf_c_dispatch_timer_t *);

/* Idle functions */
typedef void (*rig_protobuf_c_dispatch_idle_func_t)(
    rig_protobuf_c_dispatch_t *dispatch, void *func_data);
rig_protobuf_c_dispatch_idle_t *
rig_protobuf_c_dispatch_add_idle(rig_protobuf_c_dispatch_t *dispatch,
                                 rig_protobuf_c_dispatch_idle_func_t func,
                                 void *func_data);

void rig_protobuf_c_dispatch_remove_idle(rig_protobuf_c_dispatch_idle_t *);

struct _rig_protobuf_c_dispatch_t {
    rut_shell_t *shell;
    ProtobufCAllocator *allocator;
    c_list_t *dispatch_closures;
};

#endif /* _RIG_PROTOBUF_C_DISPATCH_H_ */
