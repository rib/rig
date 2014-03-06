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

#include <glib.h>

#include <rut.h>

#include "protobuf-c-rpc/rig-protobuf-c-rpc.h"

typedef struct _RigRPCServer RigRPCServer;
typedef struct _RigRPCPeer RigRPCPeer;

#include "rig-types.h"

struct _RigRPCServer
{
  RutObjectBase _base;

  int port;

  PB_RPC_Server *pb_rpc_server;

  GSource *protobuf_source;
  int source_id;

};

RigRPCServer *
rig_rpc_server_new (ProtobufCService *service,
                    PB_RPC_Error_Func server_error_handler,
                    PB_RPC_Client_Connect_Func new_client_handler,
                    void *user_data);

void
rig_rpc_server_shutdown (RigRPCServer *server);

typedef struct _RigRPCClient
{
  RutObjectBase _base;

  char *hostname;
  int port;

  PB_RPC_Client *pb_rpc_client;

  GSource *protobuf_source;
  int source_id;

} RigRPCClient;

RigRPCClient *
rig_rpc_client_new (const char *hostname,
                    int port,
                    ProtobufCServiceDescriptor *descriptor,
                    PB_RPC_Error_Func client_error_handler,
                    PB_RPC_Connect_Func connect_handler,
                    void *user_data);

void
rig_rpc_client_disconnect (RigRPCClient *rpc_client);

struct _RigRPCPeer
{
  RutObjectBase _base;

  int fd;

  PB_RPC_Peer *pb_rpc_peer;
  PB_RPC_Server *pb_rpc_server;
  PB_RPC_Client *pb_rpc_client;

  GSource *protobuf_source;
  int source_id;

};

RigRPCPeer *
rig_rpc_peer_new (int fd,
                  ProtobufCService *server_service,
                  ProtobufCServiceDescriptor *client_service,
                  PB_RPC_Error_Func peer_error_handler,
                  PB_RPC_Connect_Func connect_handler,
                  void *user_data);

#endif /* _RIG_RPC_NETWORK_H_ */
