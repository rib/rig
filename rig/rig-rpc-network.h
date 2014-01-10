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
