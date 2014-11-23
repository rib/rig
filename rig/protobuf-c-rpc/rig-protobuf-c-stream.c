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

#include <config.h>

#include <rut.h>

#include "rig-protobuf-c-data-buffer.h"
#include "rig-protobuf-c-stream.h"

static void
_stream_free(void *object)
{
    rig_pb_stream_t *stream = object;

    if (stream->read_idle)
        rig_protobuf_c_dispatch_remove_idle(stream->read_idle);

#warning "track whether stream->fd is foreign"
    if (!stream->type == STREAM_TYPE_FD && stream->fd != -1)
        rig_protobuf_c_dispatch_close_fd(stream->dispatch, stream->fd);

    rig_protobuf_c_data_buffer_clear(&stream->incoming);
    rig_protobuf_c_data_buffer_clear(&stream->outgoing);

    c_slice_free(rig_pb_stream_t, stream);
}

static rut_type_t rig_pb_stream_type;

static void
_rig_pb_stream_init_type(void)
{
    rut_type_t *type = &rig_pb_stream_type;
#define TYPE rig_pb_stream_t

    rut_type_init(type, C_STRINGIFY(TYPE), _stream_free);

#undef TYPE
}

rig_pb_stream_t *
rig_pb_stream_new(rut_shell_t *shell)
{
    ProtobufCAllocator *allocator = &protobuf_c_default_allocator;
    rig_protobuf_c_dispatch_t *dispatch =
        rig_protobuf_c_dispatch_new(shell, allocator);
    rig_pb_stream_t *stream =
        rut_object_alloc0(rig_pb_stream_t, &rig_pb_stream_type,
                          _rig_pb_stream_init_type);

    stream->shell = shell;
    stream->dispatch = dispatch;

    stream->type = STREAM_TYPE_DISCONNECTED;

    stream->fd = -1;

    rig_protobuf_c_data_buffer_init(&stream->incoming, allocator);
    rig_protobuf_c_data_buffer_init(&stream->outgoing, allocator);

    return stream;
}

void
rig_pb_stream_set_fd_transport(rig_pb_stream_t *stream, int fd)
{
    c_warn_if_fail(stream->type == STREAM_TYPE_DISCONNECTED);

    stream->type = STREAM_TYPE_FD;
    stream->fd = fd;
}

#ifdef USE_UV
void
rig_pb_stream_set_uv_streams_transport(rig_pb_stream_t *stream,
                                       uv_stream_t *in_stream,
                                       uv_stream_t *out_stream)
{
    c_warn_if_fail(stream->type == STREAM_TYPE_DISCONNECTED);

    stream->type = STREAM_TYPE_UV;
    stream->uv_in_stream = in_stream;
    stream->uv_out_stream = out_stream;
}
#endif

void
rig_pb_stream_set_in_thread_direct_transport(rig_pb_stream_t *stream,
                                             rig_pb_stream_t *other_end)
{
    c_warn_if_fail(stream->type == STREAM_TYPE_DISCONNECTED);

    stream->type = STREAM_TYPE_BUFFER;
    stream->other_end = other_end;
}
