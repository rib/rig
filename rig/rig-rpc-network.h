/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013  Intel Corporation
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

#ifndef _RIG_RPC_NETWORK_H_
#define _RIG_RPC_NETWORK_H_

#include <clib.h>

#include <rut.h>

#include "protobuf-c-rpc/rig-protobuf-c-rpc.h"

typedef struct _rig_rpc_peer_t rig_rpc_peer_t;

#include "rig-types.h"

struct _rig_rpc_peer_t {
    rut_object_base_t _base;

    rig_pb_stream_t *stream;

    rig_pb_rpc_peer_t *pb_rpc_peer;
    rig_pb_rpc_server_t *pb_rpc_server;
    rig_pb_rpc_client_t *pb_rpc_client;
};

rig_rpc_peer_t *rig_rpc_peer_new(rig_pb_stream_t *stream,
                                 ProtobufCService *server_service,
                                 ProtobufCServiceDescriptor *client_descriptor,
                                 rig_pb_rpc_error_func_t peer_error_handler,
                                 rig_pb_rpc_connect_func_t connect_handler,
                                 void *user_data);

int
rig_rpc_peer_get_port(rig_rpc_peer_t *peer);

#endif /* _RIG_RPC_NETWORK_H_ */
