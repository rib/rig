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

typedef struct _rig_rpc_server_t rig_rpc_server_t;
typedef struct _rig_rpc_peer_t rig_rpc_peer_t;

#include "rig-types.h"

struct _rig_rpc_server_t {
    rut_object_base_t _base;

    int port;

    pb_rpc__server_t *pb_rpc_server;
};

rig_rpc_server_t *
rig_rpc_server_new(rut_shell_t *shell,
                   const char *name,
                   int listening_fd,
                   ProtobufCService *service,
                   PB_RPC_Error_Func server_error_handler,
                   pb_rpc__client_t_Connect_Func new_client_handler,
                   void *user_data);

void rig_rpc_server_shutdown(rig_rpc_server_t *server);

typedef struct _rig_rpc_client_t {
    rut_object_base_t _base;

    char *hostname;
    int port;

    pb_rpc__client_t *pb_rpc_client;
} rig_rpc_client_t;

rig_rpc_client_t *rig_rpc_client_new(rut_shell_t *shell,
                                     const char *hostname,
                                     int port,
                                     ProtobufCServiceDescriptor *descriptor,
                                     PB_RPC_Error_Func client_error_handler,
                                     PB_RPC_Connect_Func connect_handler,
                                     void *user_data);

void rig_rpc_client_disconnect(rig_rpc_client_t *rpc_client);

struct _rig_rpc_peer_t {
    rut_object_base_t _base;

    int fd;

    pb_rpc__peer_t *pb_rpc_peer;
    pb_rpc__server_t *pb_rpc_server;
    pb_rpc__client_t *pb_rpc_client;
};

rig_rpc_peer_t *rig_rpc_peer_new(rut_shell_t *shell,
                                 int fd,
                                 ProtobufCService *server_service,
                                 ProtobufCServiceDescriptor *client_service,
                                 PB_RPC_Error_Func peer_error_handler,
                                 PB_RPC_Connect_Func connect_handler,
                                 void *user_data);

void
rig_rpc_peer_set_other_end(rig_rpc_peer_t *peer,
                           rig_rpc_peer_t *other_end);

#endif /* _RIG_RPC_NETWORK_H_ */
