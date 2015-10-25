#include <rig-config.h>

#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <clib.h>

#include <rut.h>

#include "rig-protobuf-c-rpc.h"

/* Protocol is:
 *    client issues request with header:
 *         method_index              32-bit little-endian
 *         message_length            32-bit little-endian
 *         request_id                32-bit any-endian
 *    server responds with header:
 *         method_index              32-bit little-endian
 *         message_length            32-bit little-endian
 *         request_id                32-bit any-endian
 */


c_quark_t
rig_pb_rpc_server_error_quark(void)
{
    return c_quark_from_static_string("rig-pb-rpc-server-error-quark");
}

c_quark_t
rig_pb_rpc_client_error_quark(void)
{
    return c_quark_from_static_string("rig-pb-rpc-client-error-quark");
}

typedef enum {
    PB_RPC_CLIENT_STATE_INIT,
    PB_RPC_CLIENT_STATE_CONNECTED,
    PB_RPC_CLIENT_STATE_FAILED,
    PB_RPC_CLIENT_STATE_DESTROYED
} rig_pb_rpc_client_state_t;

typedef struct _rig_pb_rpc_request_closure_t rig_pb_rpc_request_closure_t;
struct _rig_pb_rpc_request_closure_t {

    c_list_t link;

    uint32_t request_id;

    const ProtobufCMessageDescriptor *response_type;

    ProtobufCClosure callback;
    void *user_data;
};

struct _rig_pb_rpc_client_t {
    rut_object_base_t _base;

    /* TODO: directly embed the client/server
     * members in rig_pb_rpc_peer_t */

    ProtobufCService service;
    rig_pb_stream_t *stream;
    ProtobufCAllocator *allocator;

    rig_pb_rpc_error_func_t error_handler;
    void *error_handler_data;
    rig_pb_rpc_connect_func_t connect_handler;
    void *connect_handler_data;
    rig_pb_rpc_client_state_t state;

    c_list_t request_closures;
    uint32_t next_request_id;
};

/* === Server === */

typedef struct _server_request_t server_request_t;
struct _server_request_t {
    uint32_t request_id; /* in little-endian */
    uint32_t method_index; /* in native-endian */
    rig_pb_rpc_server_connection_t *conn;
    rig_pb_rpc_server_t *server;
    ProtobufCMessage *message;

    rig_pb_stream_write_closure_t response_write_closure;

    c_list_t link;
};

typedef enum {
    PB_RPC_CONNECTION_STATE_INIT,
    PB_RPC_CONNECTION_STATE_CONNECTED,
    PB_RPC_CONNECTION_STATE_FAILED,
    PB_RPC_CONNECTION_STATE_DESTROYED
} rig_pb_rpc_server_connection_state_t;

struct _rig_pb_rpc_server_connection_t {

    rig_pb_rpc_server_connection_state_t state;

    rig_pb_rpc_server_t *server;

    rig_pb_stream_t *stream;

    c_list_t link;

    c_list_t pending_requests;

    rig_pb_rpc_server_connection_t_Close_Func close_handler;
    void *close_handler_data;

    rig_pb_rpc_server_connection_t_Error_Func error_handler;
    void *error_handler_data;

    void *user_data;
};

struct _rig_pb_rpc_server_t {
    rut_object_base_t _base;

    rut_shell_t *shell;

    bool has_shutdown;

    ProtobufCAllocator *allocator;
    ProtobufCService *service;

    c_list_t connections;

    rig_pb_rpc_error_func_t error_handler;
    void *error_handler_data;

    rig_pb_rpc_client_t_Connect_Func client_connect_handler;
    void *client_connect_handler_data;

    rig_pb_rpc_client_t_Close_Func client_close_handler;
    void *client_close_handler_data;
};

struct _rig_pb_rpc_peer_t {
    rut_object_base_t _base;

    rig_pb_stream_t *stream;

    /* Read's may be delivered split into buffers that fragment
     * requests/replies so sometimes we need to gather a read into a
     * contiguous buffer before unpacking a message... */
    c_array_t *scratch_array;
    int scratch_offset;

    rig_pb_rpc_server_t *server;
    rig_pb_rpc_server_connection_t *conn;

    rig_pb_rpc_client_t *client;

    rut_closure_t *connect_idle;
};

static uint32_t
uint32_to_le(uint32_t le)
{
#if IS_LITTLE_ENDIAN
    return le;
#else
    return (le << 24) | (le >> 24) | ((le >> 8) & 0xff00) |
           ((le << 8) & 0xff0000);
#endif
}
#define uint32_from_le uint32_to_le /* make the code more readable, i guess */

static void
client_discard_request_closures(rig_pb_rpc_client_t *client)
{
    rig_pb_rpc_request_closure_t *closure;
    rig_pb_rpc_request_closure_t *tmp;

    c_list_for_each_safe(closure, tmp, &client->request_closures, link) {
        c_list_remove(&closure->link);

        closure->callback(NULL, closure->user_data);

        c_slice_free(rig_pb_rpc_request_closure_t, closure);
    }
}

static void
client_disconnect(rig_pb_rpc_client_t *client)
{
    if (!client->stream)
        return;

    rig_pb_stream_disconnect(client->stream);
    rut_object_unref(client->stream);
    client->stream = NULL;

    client_discard_request_closures(client);
}

static void
_rig_pb_rpc_client_free(void *object)
{
    rig_pb_rpc_client_t *client = object;

    c_return_if_fail(client->state != PB_RPC_CLIENT_STATE_DESTROYED);

    client->state = PB_RPC_CLIENT_STATE_DESTROYED;

    client_disconnect(client);

    c_slice_free(rig_pb_rpc_client_t, client);
}

static rut_type_t rig_pb_rpc_client_type;

static void
_rig_pb_rpc_client_init_type(void)
{
    rut_type_t *type = &rig_pb_rpc_client_type;
#define TYPE rig_pb_rpc_client_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_pb_rpc_client_free);

#undef TYPE
}

static void
error_handler(rig_pb_rpc_error_code_t code,
              const char *message,
              void *error_func_data)
{
    c_warning("PB RPC: %s: %s\n", (char *)error_func_data, message);
}

static void
client_throw_error(rig_pb_rpc_client_t *client,
                   rig_pb_rpc_error_code_t code,
                   const char *format, ...)
{
    va_list args;
    char *message;

    if (client->state == PB_RPC_CLIENT_STATE_FAILED)
        return;

    va_start(args, format);
    vasprintf(&message, format, args);
    va_end(args);

    client->state = PB_RPC_CLIENT_STATE_FAILED;

    /* take a transient reference in case the error handler tries to
     * destroy the client...  */
    rut_object_ref(client);

    if (client->error_handler)
        client->error_handler(code, message, client->error_handler_data);

    free(message);

    client_disconnect(client);

    rut_object_unref(client);
}

static void
client_set_state_connected(rig_pb_rpc_client_t *client)
{
    client->state = PB_RPC_CLIENT_STATE_CONNECTED;

    if (client->connect_handler)
        client->connect_handler(client, client->connect_handler_data);
}

static void
on_request_write_done_cb(rig_pb_stream_write_closure_t *req)
{
    c_free(req->buf.base);
    c_slice_free(rig_pb_stream_write_closure_t, req);
}

static void
enqueue_request(rig_pb_rpc_client_t *client,
                unsigned method_index,
                const ProtobufCMessage *input,
                ProtobufCClosure closure_callback,
                void *closure_user_data)
{
    struct {
        uint32_t method_index;
        uint32_t message_length;
        uint32_t request_id;
    } *header;
    size_t packed_size;
    uint8_t *packed_data;
    size_t buf_size;
    uint8_t *buf;
    rig_pb_rpc_request_closure_t *closure;
    rig_pb_stream_write_closure_t *stream_write_closure;
    const ProtobufCServiceDescriptor *desc = client->service.descriptor;
    const ProtobufCMethodDescriptor *method = desc->methods + method_index;

    c_return_if_fail(client->state == PB_RPC_CLIENT_STATE_CONNECTED);
    c_return_if_fail(method_index < desc->n_methods);

    /* FIXME: we should be able to push/pop an allocator for the
     * rig_pb_stream_t such that for frame based requests we can push
     * a rut_memory_stack_t allocator for the current frame which we
     * will only rewind once we know all writes for the frame have
     * finished.
     *
     * In between frames a default, malloc based allocator will likely
     * be fine.
     */
#warning "FIXME: avoid malloc in rpc enqueue_request()"

    c_assert(sizeof(*header) == 12);
    packed_size = protobuf_c_message_get_packed_size(input);

    buf_size = sizeof(*header) + packed_size;
    buf = c_malloc(buf_size);

    //c_debug("enqueue %d byte request", buf_size);

    header = (void *)buf;
    header->method_index = uint32_to_le(method_index);
    header->message_length = uint32_to_le(packed_size);
    header->request_id = client->next_request_id++;

    packed_data = buf + sizeof(*header);
    protobuf_c_message_pack(input, packed_data);

    closure = c_slice_new(rig_pb_rpc_request_closure_t);
    closure->request_id = header->request_id;
    closure->response_type = method->output;
    closure->callback = closure_callback;
    closure->user_data = closure_user_data;

    c_list_insert(client->request_closures.prev, &closure->link);

    stream_write_closure = c_slice_new(rig_pb_stream_write_closure_t);
    stream_write_closure->buf.base = (void *)buf;
    stream_write_closure->buf.len = buf_size;
    stream_write_closure->done_callback = on_request_write_done_cb;

    rig_pb_stream_write(client->stream, stream_write_closure);
}

static void
invoke_client_rpc(ProtobufCService *service,
                  unsigned method_index,
                  const ProtobufCMessage *input,
                  ProtobufCClosure closure,
                  void *closure_data)
{
    rig_pb_rpc_client_t *client =
        c_container_of(service, rig_pb_rpc_client_t, service);

    c_return_if_fail(service->invoke == invoke_client_rpc);

    switch (client->state) {
    case PB_RPC_CLIENT_STATE_INIT:
    case PB_RPC_CLIENT_STATE_CONNECTED:
        enqueue_request(client, method_index, input, closure, closure_data);
        break;

    case PB_RPC_CLIENT_STATE_FAILED:
    case PB_RPC_CLIENT_STATE_DESTROYED:
        closure(NULL, closure_data);
        break;
    }
}

rig_pb_rpc_client_t *
client_new(const ProtobufCServiceDescriptor *descriptor,
           rig_pb_stream_t *stream)
{
    rig_pb_rpc_client_t *client = rut_object_alloc0(rig_pb_rpc_client_t,
                                                 &rig_pb_rpc_client_type,
                                                 _rig_pb_rpc_client_init_type);

    client->service.descriptor = descriptor;
    client->service.invoke = invoke_client_rpc;
    client->service.destroy = NULL; /* we rely on ref-counting instead */

    client->stream = rut_object_ref(stream);

    client->allocator = stream->allocator;
    client->state = PB_RPC_CLIENT_STATE_INIT;
    client->error_handler = error_handler;
    client->error_handler_data = "protobuf-c rpc client";

    c_list_init(&client->request_closures);
    client->next_request_id = 1;

    return client;
}

ProtobufCService *
rig_pb_rpc_client_get_service(rig_pb_rpc_client_t *client)
{
    return &client->service;
}

bool
rig_pb_rpc_client_is_connected(rig_pb_rpc_client_t *client)
{
    return client->state == PB_RPC_CLIENT_STATE_CONNECTED;
}

void
rig_pb_rpc_client_set_error_handler(rig_pb_rpc_client_t *client,
                                    rig_pb_rpc_error_func_t func,
                                    void *func_data)
{
    client->error_handler = func;
    client->error_handler_data = func_data;
}

void
rig_pb_rpc_client_set_connect_handler(rig_pb_rpc_client_t *client,
                                      rig_pb_rpc_connect_func_t func,
                                      void *func_data)
{
    client->connect_handler = func;
    client->connect_handler_data = func_data;
}

static void
server_connection_close(rig_pb_rpc_server_connection_t *conn)
{
    server_request_t *req, *tmp;
    rig_pb_stream_t *stream = conn->stream;

    /* Check if already closed... */
    if (!stream)
        return;

    /* Make sure we don't recurse... */
    conn->stream = NULL;

    if (conn->server->client_close_handler) {
        conn->server->client_close_handler(
            conn->server, conn, conn->server->client_close_handler_data);
    }

    if (conn->close_handler)
        conn->close_handler(conn, conn->close_handler_data);

    rig_pb_stream_disconnect(stream);
    rut_object_unref(stream);

    /* disassocate all the requests from the connection */
    c_list_for_each_safe(req, tmp, &conn->pending_requests, link) {
        c_list_remove(&req->link);
        req->conn = NULL;
    }

    /* disassocate the connection from the server */
    c_list_remove(&conn->link);
    conn->server = NULL;
}

static void
server_shutdown(rig_pb_rpc_server_t *server)
{
    rig_pb_rpc_server_connection_t *conn, *tmp;

    if (server->has_shutdown)
        return;

    server->has_shutdown = true;

    c_list_for_each_safe(conn, tmp, &server->connections, link) {
        /* XXX: server_connection_close will unlink the connection
         * and unref it too */
        server_connection_close(conn);
    }
}

static void
_rig_pb_rpc_server_free(void *object)
{
    rig_pb_rpc_server_t *server = object;

    /* We assume that a server service is declared statically with no
     * destroy callback... */
    c_warn_if_fail(server->service->destroy == NULL);

    server_shutdown(server);

    c_slice_free(rig_pb_rpc_server_t, server);
}

static rut_type_t rig_pb_rpc_server_type;

static void
_rig_pb_rpc_server_init_type(void)
{
    rut_type_t *type = &rig_pb_rpc_server_type;
#define TYPE rig_pb_rpc_server_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_pb_rpc_server_free);

#undef TYPE
}

static void
server_throw_error(rig_pb_rpc_server_t *server,
                   rig_pb_rpc_error_code_t code,
                   const char *message)
{
    if (server->error_handler != NULL)
        server->error_handler(code, message, server->error_handler_data);
}

static void
server_connection_throw_error(rig_pb_rpc_server_connection_t *conn,
                              rig_pb_rpc_error_code_t code,
                              const char *format, ...)
{
    rig_pb_rpc_server_t *server = conn->server;
    va_list args;
    char *message;

    if (conn->state == PB_RPC_CONNECTION_STATE_FAILED)
        return;

    va_start(args, format);
    vasprintf(&message, format, args);
    va_end(args);

    conn->state = PB_RPC_CONNECTION_STATE_FAILED;

    /* In case the connection's error handler tries to clean up the
     * connection we take a reference on the server and connection
     * until we are done... */
    rut_object_ref(server);
    rut_object_ref(conn);

    if (conn->error_handler)
        conn->error_handler(conn, code, message, conn->error_handler_data);

    server_throw_error(server, code, message);

    /* Explicitly disconnect, in case the above error handler didn't
     * already.
     *
     * XXX: server_connection_close will unlink the connection from
     * the server and unref it too.
     */
    server_connection_close(conn);

    /* Drop our transient references (see above) */
    rut_object_ref(conn);
    rut_object_unref(server);

    free(message);
}

static void
server_request_free(rig_pb_rpc_server_t *server,
                    server_request_t *request)
{
    protobuf_c_message_free_unpacked(request->message, server->allocator);

    c_slice_free(server_request_t, request);

    rut_object_unref(server);
}

static server_request_t *
server_request_create(rig_pb_rpc_server_connection_t *conn,
                      uint32_t request_id,
                      uint32_t method_index,
                      ProtobufCMessage *message)
{
    server_request_t *req = c_slice_new(server_request_t);

    //c_debug("Server request: req-id = %" PRIu32 ", method-id=%" PRIu32 "\n");

    req->server = rut_object_ref(conn->server);
    req->conn = conn;
    req->request_id = request_id;
    req->method_index = method_index;
    req->message = message;

    return req;
}

static void
on_response_write_done_cb(rig_pb_stream_write_closure_t *closure)
{
    server_request_t *request =
        c_container_of(closure, server_request_t, response_write_closure);

    /* NB: the write_closure is embedded in the request so don't
     * free the request first. */
    c_free(closure->buf.base);

    server_request_free(request->server, request);
}

static void
server_connection_response_closure(const ProtobufCMessage *message,
                                   void *closure_data)
{
    server_request_t *request = closure_data;
    rig_pb_rpc_server_t *server = request->server;
    rig_pb_rpc_server_connection_t *conn = request->conn;
    size_t packed_size;
    uint8_t *packed_data;
    size_t buf_size;
    uint8_t *buf;
    struct {
        uint32_t method_index;
        uint32_t message_length;
        uint32_t request_id;
    } *header;

    c_return_if_fail(message != NULL);

    if (conn == NULL) {
        /* defunct request */
        server_request_free(server, request);
        return;
    }

    packed_size = protobuf_c_message_get_packed_size(message);

    buf_size = packed_size + 12;
    buf = c_malloc(buf_size);

    /* Note: The header[] for client replies have the same layout as the
     * headers for server requests except that that the method_index is
     * set to ~0 so requests and replies can be distinguished by peer to
     * peer clients.
     */
    header = (void *)buf;
    header->method_index = ~0;
    header->message_length = uint32_to_le(packed_size);
    header->request_id = request->request_id;

    packed_data = buf + 12;
    protobuf_c_message_pack(message, packed_data);

    request->response_write_closure.buf.base = (void *)buf;
    request->response_write_closure.buf.len = buf_size;
    request->response_write_closure.done_callback = on_response_write_done_cb;

    rig_pb_stream_write(conn->stream, &request->response_write_closure);

    /* disassociate the reqest from the connection */
    c_list_remove(&request->link);
    request->conn = NULL;
}

/* XXX: possibly optimise lookup by maintaining an array for
 * mapping request ids to closures. */
static rig_pb_rpc_request_closure_t *
lookup_request_closure(rig_pb_rpc_client_t *client,
                       uint32_t request_id)
{
    rig_pb_rpc_request_closure_t *closure;

    c_list_for_each(closure, &client->request_closures, link) {
        if (closure->request_id == request_id)
            return closure;
    }

    return NULL;
}

static bool
client_handle_reply(rig_pb_rpc_client_t *client,
                    const uint8_t *buf,
                    uint32_t message_length,
                    uint32_t request_id)
{
    rig_pb_rpc_request_closure_t *closure;
    ProtobufCMessage *msg;

    if (client->state != PB_RPC_CLIENT_STATE_CONNECTED) {
        client_throw_error(client,
                           RIG_PB_RPC_ERROR_CODE_CONNECTION_FAILED,
                           "can't handle client replies while disconnected");
        return false;
    }

    /* lookup request by id */
    closure = lookup_request_closure(client, request_id);
    if (!closure) {
        client_throw_error(client,
                           RIG_PB_RPC_ERROR_CODE_BAD_REQUEST,
                           "bad request-id in response from server: %d",
                           request_id);
        return false;
    }

    /* TODO: use fast temporary allocator */
    msg = protobuf_c_message_unpack(closure->response_type,
                                    client->allocator,
                                    message_length, buf);
    if (msg == NULL) {
        client_throw_error(client,
                           RIG_PB_RPC_ERROR_CODE_UNPACK_ERROR,
                           "failed to unpack message of length %u",
                           message_length);
        return false;
    }

    c_list_remove(&closure->link);

    closure->callback(msg, closure->user_data);

    c_slice_free(rig_pb_rpc_request_closure_t, closure);

    /* clean up */
    protobuf_c_message_free_unpacked(msg, client->allocator);

    return true;
}

static bool
server_connection_handle_request(rig_pb_rpc_server_connection_t *conn,
                                 const uint8_t *buf,
                                 uint32_t method_index,
                                 uint32_t message_length,
                                 uint32_t request_id)
{
    ProtobufCService *service = conn->server->service;
    ProtobufCAllocator *allocator = conn->server->allocator;
    ProtobufCMessage *message;
    server_request_t *server_request;

    if (conn->state != PB_RPC_CONNECTION_STATE_CONNECTED) {
        server_connection_throw_error(conn,
                                      RIG_PB_RPC_ERROR_CODE_CONNECTION_FAILED,
                                      "can't handle server request while "
                                      "disconnected");
        return false;
    }

    if ((method_index >= service->descriptor->n_methods)) {
        server_connection_throw_error(conn,
                                      RIG_PB_RPC_ERROR_CODE_BAD_REQUEST,
                                      "bad method_index %u",
                                      method_index);
        return false;
    }

    /* Unpack message */
    message = protobuf_c_message_unpack(
        service->descriptor->methods[method_index].input,
        allocator,
        message_length,
        buf);
    if (message == NULL) {
        server_connection_throw_error(conn,
                                      RIG_PB_RPC_ERROR_CODE_BAD_REQUEST,
                                      "error unpacking message");
        return false;
    }

    /* Invoke service (note that it may call back immediately) */
    server_request =
        server_request_create(conn, request_id, method_index, message);

    c_list_insert(conn->pending_requests.prev, &server_request->link);

    service->invoke(service,
                    method_index,
                    message,
                    server_connection_response_closure,
                    server_request);

    return true;
}

static void
read_message_header(const void *message,
                    uint32_t *method_index,
                    uint32_t *message_length,
                    uint32_t *request_id)
{
    const uint32_t *header = (uint32_t *)message;

    *method_index = uint32_from_le(header[0]);
    *message_length = uint32_from_le(header[1]);
    *request_id = header[2]; /* store in whatever endianness it comes in */
}

static void
handle_read_cb(rig_pb_stream_t *stream,
               const uint8_t *buf,
               size_t len,
               void *user_data)
{
    rig_pb_rpc_peer_t *peer = user_data;
    uint32_t message_length;

    /* NB: A peer may represents a server and a client, and it's only
     * once we've read the header for any messages that we can
     * determine whether we are dealing with a request to the server
     * or a reply back to the client....
     */

    rig_pb_rpc_server_connection_t *conn = peer->conn;
    rig_pb_rpc_client_t *client = peer->client;

    /* The scratch buffer lets us read a fragmented message into a
     * contiguous buffer where peer->scratch_array->len != 0
     * when the last read included an incomplete message fragment.
     *
     * If scratch_array->len == 12 then we don't know how long
     * the message will be yet so the scratch buffer is only
     * large enough to combine the rest of the message header
     * and can be resized once the message size is known.
     */
    if (peer->scratch_array->len == 12) {
        int missing = 12 - peer->scratch_offset;
        int copy_len = MIN(missing, len);
        uint32_t method_index, request_id;

        memcpy(peer->scratch_array->data + peer->scratch_offset, buf, copy_len);
        peer->scratch_offset += copy_len;

        /* Still more header to go */
        if (peer->scratch_offset != 12)
            return;

        read_message_header(buf, &method_index, &message_length, &request_id);
        c_array_set_size(peer->scratch_array, 12 + message_length);

        buf += copy_len;
        len -= copy_len;
    }

    if (peer->scratch_offset < peer->scratch_array->len) {
        int missing = peer->scratch_array->len - peer->scratch_offset;
        int copy_len = MIN(missing, len);

        memcpy(peer->scratch_array->data + peer->scratch_offset, buf, copy_len);
        peer->scratch_offset += copy_len;

        /* Still more to go */
        if (peer->scratch_offset != peer->scratch_array->len)
            return;

        /* Re-enter to handle the completed scratch buffer
         * message before continuing with the remainder */

        handle_read_cb(stream,
                       (uint8_t *)peer->scratch_array->data,
                       peer->scratch_offset,
                       user_data);

        c_array_set_size(peer->scratch_array, 0);
        peer->scratch_offset = 0;

        len -= copy_len;
        if (!len)
            return;

        buf += copy_len;
    }


    message_length = 0;
    while (len >= 12) {
        uint32_t method_index, request_id;

        read_message_header(buf, &method_index, &message_length, &request_id);

        if (len < (12 + message_length))
            break;

        len -= 12;
        buf += 12;

        /* XXX: it's possible we were sent a mallformed message
         * that looks like it's for a client or server but the
         * corresponding object is NULL...
         */

        if (conn && method_index != ~0) {
            if (!server_connection_handle_request(conn, buf, method_index,
                                                  message_length, request_id))
                return;
        } else if (client && method_index == ~0) {
            if (!client_handle_reply(client, buf, message_length, request_id))
                return;
        } else {
            if (conn)
                server_connection_throw_error(conn,
                                              RIG_PB_RPC_ERROR_CODE_BAD_REQUEST,
                                              "bad method_index %" PRIu32,
                                              method_index);
            else if (client)
                client_throw_error(client, RIG_PB_RPC_ERROR_CODE_BAD_REQUEST,
                                   "bad method_index in response from "
                                   "server: %" PRIu32, method_index);
            return;
        }

        len -= message_length;
        buf += message_length;
        message_length = 0;
    }

    /* add whatever is left to peer->scratch_array */
    if (len) {
        if (len < 12)
            c_array_set_size(peer->scratch_array, 12);
        else
            c_array_set_size(peer->scratch_array, 12 + message_length);

        memcpy(peer->scratch_array->data, buf, len);
        peer->scratch_offset = len;
    }
}

static void
_server_connection_free(void *object)
{
    rig_pb_rpc_server_connection_t *conn = object;

    conn->state = PB_RPC_CONNECTION_STATE_DESTROYED;
    server_connection_close(conn);

    rut_object_free(rig_pb_rpc_server_connection_t, conn);
}

static rut_type_t _server_connection_type;

static void
_server_connection_init_type(void)
{
    rut_type_init(&_server_connection_type,
                  "rig_pb_rpc_server_connection_t",
                  _server_connection_free);
}

static rig_pb_rpc_server_connection_t *
server_connection_new(rig_pb_rpc_server_t *server, rig_pb_stream_t *stream)
{
    rig_pb_rpc_server_connection_t *conn =
        rut_object_alloc0(rig_pb_rpc_server_connection_t,
                          &_server_connection_type,
                          _server_connection_init_type);

    conn->state = PB_RPC_CONNECTION_STATE_INIT;
    conn->server = server;
    conn->stream = rut_object_ref(stream);

    c_list_init(&conn->pending_requests);

    return conn;
}

static rig_pb_rpc_server_connection_t *
server_add_connection_with_stream(rig_pb_rpc_server_t *server, rig_pb_stream_t *stream)
{
    rig_pb_rpc_server_connection_t *conn = server_connection_new(server, stream);

    c_list_insert(server->connections.prev, &conn->link);

    conn->state = PB_RPC_CONNECTION_STATE_CONNECTED;

    if (server->client_connect_handler) {
        server->client_connect_handler(
            server, conn, server->client_connect_handler_data);
    }

    return conn;
}

static rig_pb_rpc_server_t *
server_new(rut_shell_t *shell,
           ProtobufCService *service,
           ProtobufCAllocator *allocator)
{
    rig_pb_rpc_server_t *server = rut_object_alloc0(rig_pb_rpc_server_t,
                                                 &rig_pb_rpc_server_type,
                                                 _rig_pb_rpc_server_init_type);

    server->has_shutdown = false;
    server->shell = shell;
    server->allocator = allocator;
    server->service = service;
    server->error_handler = error_handler;
    server->error_handler_data = "protobuf-c rpc server";

    c_list_init(&server->connections);

    return server;
}

void
rig_pb_rpc_server_set_client_connect_handler(rig_pb_rpc_server_t *server,
                                             rig_pb_rpc_client_t_Connect_Func func,
                                             void *user_data)
{
    server->client_connect_handler = func;
    server->client_connect_handler_data = user_data;
}

void
rig_pb_rpc_server_set_client_close_handler(
    rig_pb_rpc_server_t *server, rig_pb_rpc_client_t_Close_Func func, void *user_data)
{
    server->client_close_handler = func;
    server->client_close_handler_data = user_data;
}

void
rig_pb_rpc_server_connection_set_close_handler(
    rig_pb_rpc_server_connection_t *conn,
    rig_pb_rpc_server_connection_t_Close_Func func,
    void *user_data)
{
    conn->close_handler = func;
    conn->close_handler_data = user_data;
}

void
rig_pb_rpc_server_connection_set_error_handler(
    rig_pb_rpc_server_connection_t *conn,
    rig_pb_rpc_server_connection_t_Error_Func func,
    void *user_data)
{
    conn->error_handler = func;
    conn->error_handler_data = user_data;
}

void
rig_pb_rpc_server_set_error_handler(rig_pb_rpc_server_t *server,
                                    rig_pb_rpc_error_func_t func,
                                    void *error_func_data)
{
    server->error_handler = func;
    server->error_handler_data = error_func_data;
}

void
rig_pb_rpc_server_connection_set_data(rig_pb_rpc_server_connection_t *conn,
                                      void *user_data)
{
    conn->user_data = user_data;
}

void *
rig_pb_rpc_closure_get_connection_data(void *closure_data)
{
    server_request_t *request = closure_data;
    return request->conn->user_data;
}

static void
_rig_pb_rpc_peer_free(void *object)
{
    rig_pb_rpc_peer_t *peer = object;

    if (peer->connect_idle) {
        rut_poll_shell_remove_idle_FIXME(peer->stream->shell, peer->connect_idle);
        peer->connect_idle = NULL;
    }

    server_shutdown(peer->server);
    rut_object_unref(peer->server);
    peer->server = NULL;

    client_disconnect(peer->client);
    rut_object_unref(peer->client);
    peer->client = NULL;

    rut_object_unref(peer->stream);

    rut_object_free(rig_pb_rpc_peer_t, peer);
}

static rut_type_t rig_pb_rpc_peer_type;

static void
_rig_pb_rpc_peer_init_type(void)
{
    rut_type_init(
        &rig_pb_rpc_peer_type, "rig_pb_rpc_peer_t", _rig_pb_rpc_peer_free);
}

static void
peer_connected_idle_cb(void *user_data)
{
    rig_pb_rpc_peer_t *peer = user_data;
    rig_pb_stream_t *stream = peer->stream;
    rig_pb_rpc_server_t *server = peer->server;
    rig_pb_rpc_client_t *client = peer->client;

    rut_poll_shell_remove_idle_FIXME(peer->stream->shell, peer->connect_idle);
    peer->connect_idle = NULL;

    rig_pb_stream_set_read_callback(stream,
                                    handle_read_cb,
                                    peer); /* user data */

    peer->conn = server_add_connection_with_stream(server, stream);

    c_warn_if_fail(client->state == PB_RPC_CLIENT_STATE_INIT);
    client_set_state_connected(client);
}

static void
handle_stream_connect_cb(rig_pb_stream_t *stream,
                         void *user_data)
{
    rig_pb_rpc_peer_t *peer = user_data;

    c_return_if_fail(peer->connect_idle == NULL);

    /* Note: we defer marking the peer as connected so that in the
     * case where the stream passed to rig_pb_rpc_peer_new() is
     * already connected and we immediately call through to here,
     * we don't try to notify the peer is connected before giving an
     * opportunity to actually set on_connect and on_error callbacks
     * for the peer.
     */
    peer->connect_idle =
        rut_poll_shell_add_idle_FIXME(stream->shell,
                                peer_connected_idle_cb,
                                peer,
                                NULL); /* destroy */
}

static void
handle_stream_error_cb(rig_pb_stream_t *stream,
                       void *user_data)
{
    rig_pb_rpc_peer_t *peer = user_data;

    if (peer->client)
        client_throw_error(peer->client,
                           RIG_PB_RPC_ERROR_CODE_IO_ERROR,
                           "Stream error");

    /* XXX: conn->stream will be NULL if we are in the process of
     * closing the connection and in this case the "error" will simply
     * correspond to the stream disconnection.
     */
    if (peer->conn && peer->conn->stream != NULL) {
        server_connection_throw_error(peer->conn,
                                      RIG_PB_RPC_ERROR_CODE_IO_ERROR,
                                      "Stream error");
    }
}

rig_pb_rpc_peer_t *
rig_pb_rpc_peer_new(rig_pb_stream_t *stream,
                    ProtobufCService *server_service,
                    const ProtobufCServiceDescriptor *client_descriptor)
{
    rig_pb_rpc_peer_t *peer = rut_object_alloc0(
        rig_pb_rpc_peer_t, &rig_pb_rpc_peer_type, _rig_pb_rpc_peer_init_type);

    peer->stream = rut_object_ref(stream);

    rig_pb_stream_add_on_connect_callback(peer->stream,
                                          handle_stream_connect_cb,
                                          peer, /* user data */
                                          NULL); /* destroy */
    rig_pb_stream_add_on_error_callback(peer->stream,
                                        handle_stream_error_cb,
                                        peer, /* user data */
                                        NULL); /* destroy */

    peer->server = server_new(stream->shell, server_service, stream->allocator);
    peer->client = client_new(client_descriptor, peer->stream);

    peer->scratch_array = c_array_new(false /* zero terminated */,
                                      false /* clear */,
                                      1 /* element size */);

    if (peer->stream->type != STREAM_TYPE_DISCONNECTED)
        handle_stream_connect_cb(peer->stream, peer);

    return peer;
}

rig_pb_stream_t *
rig_pb_rpc_peer_get_stream(rig_pb_rpc_peer_t *peer)
{
    return peer->stream;
}

rig_pb_rpc_server_t *
rig_pb_rpc_peer_get_server(rig_pb_rpc_peer_t *peer)
{
    return peer->server;
}

rig_pb_rpc_client_t *
rig_pb_rpc_peer_get_client(rig_pb_rpc_peer_t *peer)
{
    return peer->client;
}
