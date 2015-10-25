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

#ifdef USE_UV
#include <uv.h>
#include <wslay_event.h>
#endif
#ifdef __EMSCRIPTEN__
#include "rig-emscripten-lib.h"
#endif

#include <rut.h>

typedef struct _rig_pb_stream_t rig_pb_stream_t;

#include "rig-protobuf-c-rpc.h"

enum stream_type {
    STREAM_TYPE_DISCONNECTED,
#ifdef USE_UV
    STREAM_TYPE_FD,
    STREAM_TYPE_TCP,
    STREAM_TYPE_WEBSOCKET_SERVER,
#endif
#ifdef __EMSCRIPTEN__
    STREAM_TYPE_WORKER_IPC,
    STREAM_TYPE_WEBSOCKET_CLIENT,
#endif
    STREAM_TYPE_BUFFER,
};

typedef struct _rig_pb_stream_write_closure rig_pb_stream_write_closure_t;

#ifndef USE_UV
typedef struct _rig_pb_stream_buf_t {
  char *base;
  size_t len;
} rig_pb_stream_buf_t;
#endif

struct _rig_pb_stream_write_closure
{
#ifdef USE_UV
    uv_write_t write_req;
    uv_buf_t buf;
    int current_offset;
#else
    rig_pb_stream_buf_t buf;
#endif
    void (*done_callback)(rig_pb_stream_write_closure_t *closure);
};

struct _rig_pb_stream_t {
    rut_object_base_t _base;

    rut_shell_t *shell;

    ProtobufCAllocator *allocator;

    enum stream_type type;

    /* These are only relevent for setting up TCP streams but we keep
     * them out of the following union because they are used while
     * stream->type is still _DISCONNECTED when we don't know which
     * members of the union are safe to reference. */
#ifdef USE_UV
    char *hostname;
    char *port;
    uv_getaddrinfo_t resolver;
    bool resolving;
    uv_connect_t connection_request;
    bool connecting;
#endif

    union {

#ifdef USE_UV
        struct {
            /* STREAM_TYPE_FD... */
            uv_pipe_t uv_fd_pipe;
        } fd;

        struct {
            /* STREAM_TYPE_TCP... */
            uv_tcp_t socket;
        } tcp;

        struct {
            struct wslay_event_context *ctx;
        } websocket_server;
#endif
#ifdef __EMSCRIPTEN__
        struct {
            bool in_worker;
            rig_worker_t worker;
        } worker_ipc;

        struct {
            int socket;
        } websocket_client;
#endif

        /* STREAM_TYPE_BUFFER... */
        struct {
            /* XXX: The "remote" end of a stream is sometimes in the
             * same address space and instead of polling a file
             * descriptor we simply queue idle callbacks when we write
             * to either end of a stream...
             */
            rig_pb_stream_t *other_end;
            rut_closure_t *connect_idle;
            rut_closure_t *read_idle;

            /* writes from the other end are queued in
             * incoming_write_closures and once they have been handled
             * they get moved to finished_write_closures so the other
             * end can free the closures
             *
             * TODO: allow pivoting these safely with atomic ops so we
             * can use this mechanism between threads.
             */
            c_array_t *incoming_write_closures;
            c_array_t *finished_write_closures;
        } buffer;
    };

    /* Common */

    c_list_t on_connect_closures;
    c_list_t on_error_closures;

    void (*read_callback)(rig_pb_stream_t *stream,
                          const uint8_t *buf,
                          size_t len,
                          void *user_data);
    void *read_data;
};

rig_pb_stream_t *
rig_pb_stream_new(rut_shell_t *shell);

typedef void (*rig_pb_stream_callback_t)(rig_pb_stream_t *stream, void *user_data);

rut_closure_t *
rig_pb_stream_add_on_connect_callback(rig_pb_stream_t *stream,
                                      rig_pb_stream_callback_t callback,
                                      void *user_data,
                                      rut_closure_destroy_callback_t destroy);

rut_closure_t *
rig_pb_stream_add_on_error_callback(rig_pb_stream_t *stream,
                                    rig_pb_stream_callback_t callback,
                                    void *user_data,
                                    rut_closure_destroy_callback_t destroy);

#ifdef USE_UV
void
rig_pb_stream_set_fd_transport(rig_pb_stream_t *stream, int fd);

void
rig_pb_stream_set_tcp_transport(rig_pb_stream_t *stream,
                                const char *hostname,
                                const char *port);

void
rig_pb_stream_accept_tcp_connection(rig_pb_stream_t *stream,
                                    uv_tcp_t *server);
#endif

#ifdef __EMSCRIPTEN__
void
rig_pb_stream_set_in_worker(rig_pb_stream_t *stream, bool in_worker);

void
rig_pb_stream_set_worker(rig_pb_stream_t *stream,
                         rig_worker_t worker);

void
rig_pb_stream_set_websocket_client_fd(rig_pb_stream_t *stream,
                                      int socket);
#endif

#ifdef USE_UV
void
rig_pb_stream_set_wslay_server_event_ctx(rig_pb_stream_t *stream,
                                         struct wslay_event_context *ctx);

void
rig_pb_stream_websocket_message(rig_pb_stream_t *stream,
                                const struct wslay_event_on_msg_recv_arg *arg);
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


void
rig_pb_stream_set_read_callback(rig_pb_stream_t *stream,
                                void (*read_callback)(rig_pb_stream_t *stream,
                                                      const uint8_t *buf,
                                                      size_t len,
                                                      void *user_data),
                                void *user_data);

void
rig_pb_stream_write(rig_pb_stream_t *stream,
                    rig_pb_stream_write_closure_t *closure);

void
rig_pb_stream_disconnect(rig_pb_stream_t *stream);

#endif
