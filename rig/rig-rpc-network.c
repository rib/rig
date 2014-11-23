/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013,2014  Intel Corporation
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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include <clib.h>

#include "protobuf-c-rpc/rig-protobuf-c-rpc.h"

#include "rig-rpc-network.h"
#include "rig.pb-c.h"

void
rig_rpc_server_shutdown(rig_rpc_server_t *server)
{
    c_warning("Stopping RPC server");

#warning "todo: explicitly shutdown via new rig_pb_rpc_server_shutdown() api"
    rut_object_unref(server->pb_rpc_server);
    server->pb_rpc_server = NULL;
}

static void
_rig_rpc_server_free(void *object)
{
    rig_rpc_server_t *server = object;

    rig_rpc_server_shutdown(server);

    rut_object_free(rig_rpc_server_t, server);
}

static rut_type_t rig_rpc_server_type;

static void
_rig_rpc_server_init_type(void)
{

    rut_type_t *type = &rig_rpc_server_type;
#define TYPE rig_rpc_server_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_rpc_server_free);

#undef TYPE
}

rig_rpc_server_t *
rig_rpc_server_new(rut_shell_t *shell,
                   const char *name,
                   int listening_fd,
                   ProtobufCService *service,
                   PB_RPC_Error_Func server_error_handler,
                   pb_rpc__client_t_Connect_Func new_client_handler,
                   void *user_data)
{
    rig_rpc_server_t *server = rut_object_alloc0(
        rig_rpc_server_t, &rig_rpc_server_type, _rig_rpc_server_init_type);
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    server->pb_rpc_server =
        rig_pb_rpc_server_new(shell, name, listening_fd, service);

    getsockname(listening_fd, (struct sockaddr *)&addr, &addr_len);

    if (addr.sin_family == AF_INET)
        server->port = ntohs(addr.sin_port);
    else
        server->port = 0;

    rig_pb_rpc_server_set_error_handler(
        server->pb_rpc_server, server_error_handler, user_data);

    rig_pb_rpc_server_set_client_connect_handler(
        server->pb_rpc_server, new_client_handler, user_data);

    return server;
}

static void
_rig_rpc_client_free(void *object)
{
    rig_rpc_client_t *rpc_client = object;

    rig_rpc_client_disconnect(rpc_client);

    c_free(rpc_client->hostname);

    rut_object_free(rig_rpc_client_t, rpc_client);
}

static rut_type_t rig_rpc_client_type;

static void
_rig_rpc_client_init_type(void)
{

    rut_type_t *type = &rig_rpc_client_type;
#define TYPE rig_rpc_client_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_rpc_client_free);

#undef TYPE
}

rig_rpc_client_t *
rig_rpc_client_new(rut_shell_t *shell,
                   const char *hostname,
                   int port,
                   ProtobufCServiceDescriptor *descriptor,
                   PB_RPC_Error_Func client_error_handler,
                   PB_RPC_Connect_Func connect_handler,
                   void *user_data)
{
    rig_rpc_client_t *rpc_client = rut_object_alloc0(
        rig_rpc_client_t, &rig_rpc_client_type, _rig_rpc_client_init_type);
    char *addr_str = c_strdup_printf("%s:%d", hostname, port);
    rig_pb_stream_t *stream = rig_pb_stream_new(shell);
    pb_rpc__client_t *pb_client;

    rpc_client->hostname = c_strdup(hostname);
    rpc_client->port = port;

    pb_client = rig_pb_rpc_client_new(stream,
                                      PROTOBUF_C_RPC_ADDRESS_TCP,
                                      addr_str,
                                      descriptor);

    rut_object_unref(stream);

    rig_pb_rpc_client_set_connect_handler(
        pb_client, connect_handler, user_data);

    c_free(addr_str);

    rig_pb_rpc_client_set_error_handler(
        pb_client, client_error_handler, user_data);

    rpc_client->pb_rpc_client = pb_client;

    return rpc_client;
}

void
rig_rpc_client_disconnect(rig_rpc_client_t *rpc_client)
{
    if (!rpc_client->pb_rpc_client)
        return;

#warning "TODO: need explicit rig_pb_rpc_client_disconnect() api"
    rut_object_unref(rpc_client->pb_rpc_client);
    rpc_client->pb_rpc_client = NULL;
}

static void
_rig_rpc_peer_free(void *object)
{
    rig_rpc_peer_t *rpc_peer = object;

    rut_object_unref(rpc_peer->pb_rpc_peer);

    rut_object_unref(rpc_peer->stream);

    rut_object_free(rig_rpc_peer_t, rpc_peer);
}

static rut_type_t rig_rpc_peer_type;

static void
_rig_rpc_peer_init_type(void)
{

    rut_type_t *type = &rig_rpc_peer_type;
#define TYPE rig_rpc_peer_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_rpc_peer_free);

#undef TYPE
}

static void
server_connect_handler(pb_rpc__server_t *server,
                       pb_rpc__server_connection_t *conn,
                       void *user_data)
{
    rig_pb_rpc_server_connection_set_data(conn, user_data);
}

rig_rpc_peer_t *
rig_rpc_peer_new(rig_pb_stream_t *stream,
                 ProtobufCService *server_service,
                 ProtobufCServiceDescriptor *client_descriptor,
                 PB_RPC_Error_Func peer_error_handler,
                 PB_RPC_Connect_Func connect_handler,
                 void *user_data)
{
    rig_rpc_peer_t *rpc_peer = rut_object_alloc0(
        rig_rpc_peer_t, &rig_rpc_peer_type, _rig_rpc_peer_init_type);
    pb_rpc__peer_t *pb_peer;

    rpc_peer->stream = rut_object_ref(stream);

    pb_peer = rig_pb_rpc_peer_new(stream, server_service,
                                  client_descriptor);
    rpc_peer->pb_rpc_peer = pb_peer;

    rpc_peer->pb_rpc_client = rig_pb_rpc_peer_get_client(pb_peer);

    rig_pb_rpc_client_set_connect_handler(
        rpc_peer->pb_rpc_client, connect_handler, user_data);
    rig_pb_rpc_client_set_error_handler(
        rpc_peer->pb_rpc_client, peer_error_handler, user_data);

    rpc_peer->pb_rpc_server = rig_pb_rpc_peer_get_server(pb_peer);

    rig_pb_rpc_server_set_error_handler(
        rpc_peer->pb_rpc_server, peer_error_handler, user_data);

    rig_pb_rpc_server_set_client_connect_handler(
        rpc_peer->pb_rpc_server, server_connect_handler, user_data);

    return rpc_peer;
}
