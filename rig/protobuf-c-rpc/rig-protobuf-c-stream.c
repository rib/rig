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

#include <uv.h>

#include <rut.h>

#include "rig-protobuf-c-stream.h"

static void
drain_finished_write_closures(rig_pb_stream_t *stream)
{
    int i;

    for (i = 0; i < stream->finished_write_closures->len; i++) {
        rig_pb_stream_write_closure_t *closure =
            c_array_index(stream->finished_write_closures, void *, i);

        if (closure->done_callback)
            closure->done_callback(closure);
    }

    c_array_set_size(stream->finished_write_closures, 0);
}

void
rig_pb_stream_disconnect(rig_pb_stream_t *stream)
{
    switch(stream->type)
    {
#ifdef USE_UV
    case STREAM_TYPE_FD:
        uv_read_stop((uv_stream_t *)&stream->uv_fd_pipe);
        uv_close((uv_handle_t *)&stream->uv_fd_pipe,
                 NULL /* closed callback */);
        break;
#endif
    case STREAM_TYPE_BUFFER:
        {
            int i;

            /* Give all incoming write closures back to the other end
             * so they can be freed */
            for (i = 0; i < stream->incoming_write_closures->len; i++) {
                rig_pb_stream_write_closure_t *closure =
                    c_array_index(stream->incoming_write_closures, void *, i);

                c_array_append_val(stream->other_end->finished_write_closures,
                                   closure);
            }
            c_array_free(stream->incoming_write_closures,
                         true /* free storage */);
            stream->incoming_write_closures = NULL;

            drain_finished_write_closures(stream);

            c_array_free(stream->finished_write_closures,
                         true /* free storage */);
            stream->finished_write_closures = NULL;

            stream->other_end->other_end = NULL;
            stream->other_end = NULL;
        }

        if (stream->read_idle) {
            rut_poll_shell_remove_idle(stream->shell, stream->read_idle);
            stream->read_idle = NULL;
        }
        break;
    case STREAM_TYPE_DISCONNECTED:
        return;
    }

    stream->type = STREAM_TYPE_DISCONNECTED;

    rut_closure_list_invoke(&stream->on_error_closures,
                            rig_pb_stream_callback_t,
                            stream);
}

static void
_stream_free(void *object)
{
    rig_pb_stream_t *stream = object;

    rig_pb_stream_disconnect(stream);

    if (stream->connect_idle) {
        rut_poll_shell_remove_idle(stream->shell, stream->connect_idle);
        stream->connect_idle = NULL;
    }

    rut_closure_list_disconnect_all(&stream->on_connect_closures);
    rut_closure_list_disconnect_all(&stream->on_error_closures);

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
    rig_pb_stream_t *stream =
        rut_object_alloc0(rig_pb_stream_t, &rig_pb_stream_type,
                          _rig_pb_stream_init_type);

    stream->shell = shell;
    stream->allocator = &protobuf_c_default_allocator;

    stream->type = STREAM_TYPE_DISCONNECTED;

    rut_list_init(&stream->on_connect_closures);
    rut_list_init(&stream->on_error_closures);

    return stream;
}

rut_closure_t *
rig_pb_stream_add_on_connect_callback(rig_pb_stream_t *stream,
                                      rig_pb_stream_callback_t callback,
                                      void *user_data,
                                      rut_closure_destroy_callback_t destroy)
{
    return rut_closure_list_add(&stream->on_connect_closures,
                                callback,
                                user_data,
                                destroy);
}

rut_closure_t *
rig_pb_stream_add_on_error_callback(rig_pb_stream_t *stream,
                                    rig_pb_stream_callback_t callback,
                                    void *user_data,
                                    rut_closure_destroy_callback_t destroy)
{
    return rut_closure_list_add(&stream->on_error_closures,
                                callback,
                                user_data,
                                destroy);
}

static void
set_connected(rig_pb_stream_t *stream)
{
    rut_closure_list_invoke(&stream->on_connect_closures,
                            rig_pb_stream_callback_t,
                            stream);
}

#ifdef USE_UV
void
rig_pb_stream_set_fd_transport(rig_pb_stream_t *stream, int fd)
{
    uv_loop_t *loop = rut_uv_shell_get_loop(stream->shell);

    c_return_if_fail(stream->type == STREAM_TYPE_DISCONNECTED);

    stream->type = STREAM_TYPE_FD;

    uv_pipe_init(loop, &stream->uv_fd_pipe, true /* enable handle passing */);
    stream->uv_fd_pipe.data = stream;

    uv_pipe_open(&stream->uv_fd_pipe, fd);

    set_connected(stream);
}
#endif

static void
data_buffer_stream_read_idle(void *user_data)
{
    rig_pb_stream_t *stream = user_data;
    int i;

    rut_poll_shell_remove_idle(stream->shell, stream->read_idle);
    stream->read_idle = NULL;

    c_return_if_fail(stream->type == STREAM_TYPE_BUFFER);
    c_return_if_fail(stream->other_end != NULL);
    c_return_if_fail(stream->other_end->type == STREAM_TYPE_BUFFER);
    c_return_if_fail(stream->read_callback != NULL);

    for (i = 0; i < stream->incoming_write_closures->len; i++) {
        rig_pb_stream_write_closure_t *closure =
            c_array_index(stream->incoming_write_closures, void *, i);

        stream->read_callback(stream,
                              (uint8_t *)closure->buf.base,
                              closure->buf.len,
                              stream->read_data);

        /* Give the closure back so it can be freed*/
        c_array_append_val(stream->other_end->finished_write_closures,
                           closure);
    }
    c_array_set_size(stream->incoming_write_closures, 0);

    drain_finished_write_closures(stream);
}

static void
queue_data_buffer_stream_read(rig_pb_stream_t *stream)
{
    c_return_if_fail(stream->other_end != NULL);
    c_return_if_fail(stream->other_end->type == STREAM_TYPE_BUFFER);
    c_return_if_fail(stream->incoming_write_closures->len);

    if (!stream->read_callback)
        return;

    if (stream->read_idle == NULL) {
        stream->read_idle =
            rut_poll_shell_add_idle(stream->shell,
                                    data_buffer_stream_read_idle,
                                    stream,
                                    NULL); /* destroy */
    }
}

static void
stream_set_connected_idle(void *user_data)
{
    rig_pb_stream_t *stream = user_data;

    rut_poll_shell_remove_idle(stream->shell, stream->connect_idle);
    stream->connect_idle = NULL;

    set_connected(stream);
}

static void
queue_set_connected(rig_pb_stream_t *stream)
{
    c_return_if_fail(stream->connect_idle == NULL);

    stream->connect_idle =
        rut_poll_shell_add_idle(stream->shell,
                                stream_set_connected_idle,
                                stream,
                                NULL); /* destroy */
}

void
rig_pb_stream_set_in_thread_direct_transport(rig_pb_stream_t *stream,
                                             rig_pb_stream_t *other_end)
{
    c_return_if_fail(stream->type == STREAM_TYPE_DISCONNECTED);

    stream->incoming_write_closures = c_array_new(false, /* nul terminated */
                                                  false, /* clear */
                                                  sizeof(void *));

    stream->finished_write_closures = c_array_new(false, /* nul terminated */
                                                  false, /* clear */
                                                  sizeof(void *));

    stream->other_end = other_end;

    /* Only consider the streams connected when both ends have been
     * initialised... */
    if (other_end->other_end == stream) {
        stream->type = STREAM_TYPE_BUFFER;
        other_end->type = STREAM_TYPE_BUFFER;

        queue_set_connected(stream);
        queue_set_connected(other_end);
    }
}

#ifdef USE_UV
static void
read_buf_alloc_cb(uv_handle_t *handle, size_t len, uv_buf_t *buf)
{
    *buf = uv_buf_init(c_malloc(len), len);
}

static void
read_cb(uv_stream_t *uv_stream, ssize_t len, const uv_buf_t *buf)
{
    rig_pb_stream_t *stream = uv_stream->data;

    if (len == UV_EOF)
        rig_pb_stream_disconnect(stream);
    else if (len > 0)
        stream->read_callback(stream, (uint8_t *)buf->base, len,
                              stream->read_data);

    if (buf->base)
        c_free(buf->base);
}
#endif

void
rig_pb_stream_set_read_callback(rig_pb_stream_t *stream,
                                void (*read_callback)(rig_pb_stream_t *stream,
                                                      uint8_t *buf,
                                                      size_t len,
                                                      void *user_data),
                                void *user_data)
{
    stream->read_callback = read_callback;
    stream->read_data = user_data;

    if (stream->type == STREAM_TYPE_BUFFER) {
        c_return_if_fail(stream->other_end != NULL);
        c_return_if_fail(stream->other_end->type == STREAM_TYPE_BUFFER);

        if (stream->incoming_write_closures->len)
            queue_data_buffer_stream_read(stream);
    }
#ifdef USE_UV
    else {
        uv_read_start((uv_stream_t *)&stream->uv_fd_pipe,
                      read_buf_alloc_cb, read_cb);
    }
#endif
}

static void
uv_write_done_cb(uv_write_t *write_req, int status)
{
    rig_pb_stream_write_closure_t *closure = write_req->data;
    closure->done_callback(closure);
}

void
rig_pb_stream_write(rig_pb_stream_t *stream,
                    rig_pb_stream_write_closure_t *closure)
{
    c_return_if_fail(stream->type != STREAM_TYPE_DISCONNECTED);

    if (stream->type == STREAM_TYPE_BUFFER) {
        c_return_if_fail(stream->other_end != NULL);
        c_return_if_fail(stream->other_end->type == STREAM_TYPE_BUFFER);

        c_array_append_val(stream->other_end->incoming_write_closures, closure);

        queue_data_buffer_stream_read(stream->other_end);
    } else {
        closure->write_req.data = closure;

        uv_write(&closure->write_req,
                 (uv_stream_t *)&stream->uv_fd_pipe,
                 &closure->buf,
                 1, /* n buffers */
                 uv_write_done_cb);
    }
}
