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

#include "protobuf-c/rig-protobuf-c-rpc.h"

#include "rig-engine.h"

void
rig_rpc_start_server (RigEngine *engine,
                      ProtobufCService *service,
                      PB_RPC_Error_Func server_error_handler,
                      PB_RPC_Client_Connect_Func new_client_handler,
                      void *user_data);

void
rig_rpc_stop_server (RigEngine *engine);

typedef struct _RigRPCClient
{
  RutObjectProps _parent;
  int ref_count;

  RigEngine *engine;

  char *hostname;
  int port;

  PB_RPC_Client *pb_rpc_client;
  GSource *protobuf_source;

  int source_id;

} RigRPCClient;

RigRPCClient *
rig_rpc_client_new (RigEngine *engine,
                    const char *hostname,
                    int port,
                    ProtobufCServiceDescriptor *descriptor,
                    PB_RPC_Error_Func client_error_handler,
                    PB_RPC_Connect_Func connect_handler,
                    void *user_data);

void
rig_rpc_client_disconnect (RigRPCClient *rpc_client);

#endif /* _RIG_RPC_NETWORK_H_ */
