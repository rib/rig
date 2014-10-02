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
rig_pb_stream_new(rig_protobuf_c_dispatch_t *dispatch, int fd)
{
    ProtobufCAllocator *allocator =
        rig_protobuf_c_dispatch_peek_allocator(dispatch);
    rig_pb_stream_t *stream =
        rut_object_alloc0(rig_pb_stream_t, &rig_pb_stream_type,
                          _rig_pb_stream_init_type);

    /* We have to explicitly track the dispatch with the stream since
     * it may be referenced when freeing the stream after any client
     * or server connection have been disassociated from the stream.
     */
    stream->dispatch = dispatch;

    stream->type = STREAM_TYPE_FD;
    stream->fd = fd;
    rig_protobuf_c_data_buffer_init(&stream->incoming, allocator);
    rig_protobuf_c_data_buffer_init(&stream->outgoing, allocator);

    return stream;
}

void
rig_pb_stream_set_other_end(rig_pb_stream_t *stream,
                            rig_pb_stream_t *other_end)
{
    c_warn_if_fail(stream->fd == -1);
    stream->type = STREAM_TYPE_BUFFER;
    stream->other_end = other_end;
}
