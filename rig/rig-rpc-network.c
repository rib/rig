/*
 * Copyright (c) 2008-2011, Dave Benson.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with
 * or without modification, are permitted provided that the
 * following conditions are met:
 *
 * Redistributions of source code must retain the above
 * copyright notice, this list of conditions and the following
 * disclaimer.

 * Redistributions in binary form must reproduce
 * the above copyright notice, this list of conditions and
 * the following disclaimer in the documentation and/or other
 * materials provided with the distribution.
 *
 * Neither the name
 * of "protobuf-c" nor the names of its contributors
 * may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Rig
 *
 * Copyright (C) 2013  Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
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
    rig_protobuf_c_dispatch_t *dispatch =
        rig_protobuf_c_dispatch_new(shell, &protobuf_c_default_allocator);
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    server->pb_rpc_server =
        rig_pb_rpc_server_new(name, listening_fd, service, dispatch);

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
    rig_protobuf_c_dispatch_t *dispatch;
    pb_rpc__client_t *pb_client;

    rpc_client->hostname = c_strdup(hostname);
    rpc_client->port = port;

    dispatch =
        rig_protobuf_c_dispatch_new(shell, &protobuf_c_default_allocator);

    pb_client = (pb_rpc__client_t *)rig_pb_rpc_client_new(
        PROTOBUF_C_RPC_ADDRESS_TCP, addr_str, descriptor, dispatch);

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
rig_rpc_peer_new(rut_shell_t *shell,
                 int fd,
                 ProtobufCService *server_service,
                 ProtobufCServiceDescriptor *client_descriptor,
                 PB_RPC_Error_Func peer_error_handler,
                 PB_RPC_Connect_Func connect_handler,
                 void *user_data)
{
    rig_rpc_peer_t *rpc_peer = rut_object_alloc0(
        rig_rpc_peer_t, &rig_rpc_peer_type, _rig_rpc_peer_init_type);
    rig_protobuf_c_dispatch_t *dispatch;
    rig_pb_stream_t *stream;
    pb_rpc__peer_t *pb_peer;

    rpc_peer->fd = fd;

    dispatch =
        rig_protobuf_c_dispatch_new(shell, &protobuf_c_default_allocator);

    stream = rig_pb_stream_new(dispatch, fd);
    pb_peer = rig_pb_rpc_peer_new(stream, server_service,
                                  client_descriptor, dispatch);
    rut_object_unref(stream);
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

void
rig_rpc_peer_set_other_end(rig_rpc_peer_t *peer,
                           rig_rpc_peer_t *other_end)
{
    rig_pb_stream_t *stream = rig_pb_rpc_peer_get_stream(peer->pb_rpc_peer);
    rig_pb_stream_t *other_end_stream =
        rig_pb_rpc_peer_get_stream(other_end->pb_rpc_peer);

    rig_pb_stream_set_other_end(stream, other_end_stream);
}
