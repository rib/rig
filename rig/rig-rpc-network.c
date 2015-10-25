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

#include <rig-config.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include <clib.h>

#include "protobuf-c-rpc/rig-protobuf-c-rpc.h"

#include "rig-rpc-network.h"
#include "rig.pb-c.h"

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
server_connect_handler(rig_pb_rpc_server_t *server,
                       rig_pb_rpc_server_connection_t *conn,
                       void *user_data)
{
    rig_pb_rpc_server_connection_set_data(conn, user_data);
}

rig_rpc_peer_t *
rig_rpc_peer_new(rig_pb_stream_t *stream,
                 ProtobufCService *server_service,
                 ProtobufCServiceDescriptor *client_descriptor,
                 rig_pb_rpc_error_func_t peer_error_handler,
                 rig_pb_rpc_connect_func_t connect_handler,
                 void *user_data)
{
    rig_rpc_peer_t *rpc_peer = rut_object_alloc0(
        rig_rpc_peer_t, &rig_rpc_peer_type, _rig_rpc_peer_init_type);
    rig_pb_rpc_peer_t *pb_peer;

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

int
rig_rpc_peer_get_port(rig_rpc_peer_t *peer)
{
    /* FIXME: we need to re-introduce tcp/ip support to the rpc layer
     * once the libuv integration is complete.
     */
    c_return_val_if_reached(-1);
}
