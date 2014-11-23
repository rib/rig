/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2014  Intel Corporation
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

#ifndef _RIG_PB_RPC_STREAM_H_
#define _RIG_PB_RPC_STREAM_H_

typedef struct _rig_pb_stream_t rig_pb_stream_t;

#include "rig-protobuf-c-dispatch.h"
#include "rig-protobuf-c-data-buffer.h"
#include "rig-protobuf-c-rpc.h"

enum stream_type {
    STREAM_TYPE_DISCONNECTED,
    STREAM_TYPE_FD,
#ifdef USE_UV
    STREAM_TYPE_UV,
#endif
    STREAM_TYPE_BUFFER,
};

struct _rig_pb_stream_t {
    rut_object_base_t _base;

    rut_shell_t *shell;
    rig_protobuf_c_dispatch_t *dispatch;

    enum stream_type type;

    /* STREAM_TYPE_FD... */
    int fd;

    /* STREAM_TYPE_UV... */
#ifdef USE_UV
    uv_stream_t *uv_in_stream;
    uv_stream_t *uv_out_stream;
#endif

    /* STREAM_TYPE_BUFFER... */
    /* XXX: The "remote" end of a stream is sometimes in the same
     * address space and instead of polling a file descriptor we
     * simply queue idle callbacks when we write to either end of
     * a stream...
     */
    rig_pb_stream_t *other_end;
    rig_protobuf_c_dispatch_idle_t *read_idle;

    protobuf_c_data_buffer_t incoming;
    protobuf_c_data_buffer_t outgoing;

    pb_rpc__server_connection_t *conn;
    pb_rpc__client_t *client;
};

rig_pb_stream_t *
rig_pb_stream_new(rut_shell_t *shell);

void
rig_pb_stream_set_fd_transport(rig_pb_stream_t *stream, int fd);

#ifdef USE_UV
void
rig_pb_stream_set_uv_streams_transport(rig_pb_stream_t *stream,
                                       uv_stream_t *in_stream,
                                       uv_stream_t *out_stream);
#endif

/* So we can support having both ends of a connection in the same
 * process without using file descriptors and polling to communicate
 * this lets us create a stream with a direct pointer to the "remote"
 * end and we can queue work for the remote in an idle function...
 *
 * Note: This mechanism is *not* currently threadsafe.
 */
void
rig_pb_stream_set_in_thread_direct_transport(rig_pb_stream_t *stream,
                                             rig_pb_stream_t *other_end);

#endif
