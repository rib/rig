/*
 * Rig
 *
 * Copyright (C) 2013  Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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

#include <config.h>

#include <glib.h>

#include <rut.h>

#include "rig-engine.h"
#include "rig-simulator-service.h"
#include "rig-pb.h"
#include "rig.pb-c.h"

static void
simulator__test (Rig__Simulator_Service *service,
                 const Rig__Query *query,
                 Rig__TestResult_Closure closure,
                 void *closure_data)
{
  Rig__TestResult result = RIG__TEST_RESULT__INIT;
  //RigSimulator *simulator = rig_pb_rpc_closure_get_connection_data (closure_data);

  g_return_if_fail (query != NULL);

  g_print ("Simulator Service: Test Query\n");

  closure (&result, closure_data);
}

static void
simulator__load (Rig__Simulator_Service *service,
                 const Rig__UI *ui,
                 Rig__LoadResult_Closure closure,
                 void *closure_data)
{
  Rig__LoadResult result = RIG__LOAD_RESULT__INIT;
  RigEngine *engine =
    rig_pb_rpc_closure_get_connection_data (closure_data);

  g_return_if_fail (ui != NULL);

  g_print ("Simulator: UI Load Request\n");

  rig_pb_unserialize_ui (engine, ui);

  closure (&result, closure_data);
}

static void
simulator__run_frame (Rig__Simulator_Service *service,
                      const Rig__FrameSetup *setup,
                      Rig__RunFrameAck_Closure closure,
                      void *closure_data)
{
  Rig__RunFrameAck ack = RIG__RUN_FRAME_ACK__INIT;
  RigEngine *engine =
    rig_pb_rpc_closure_get_connection_data (closure_data);

  g_return_if_fail (setup != NULL);

  g_print ("Simulator: Run Frame Request\n");

  closure (&ack, closure_data);
}

static Rig__Simulator_Service rig_simulator_service =
  RIG__SIMULATOR__INIT(simulator__);


static void
handle_renderer_test_response (const Rig__TestResult *result,
                                void *closure_data)
{
  g_print ("Renderer test response received\n");
}

static void
simulator_peer_connected (PB_RPC_Client *pb_client,
                          void *user_data)
{
  ProtobufCService *renderer_service =
    rig_pb_rpc_client_get_service (pb_client);
  Rig__Query query = RIG__QUERY__INIT;

  rig__renderer__test (renderer_service, &query,
                       handle_renderer_test_response, NULL);
  g_print ("Simulator peer connected\n");
}

static void
simulator_peer_error_handler (PB_RPC_Error_Code code,
                              const char *message,
                              void *user_data)
{
  RigEngine *engine = user_data;

  g_warning ("Simulator peer error: %s", message);

  rig_simulator_service_stop (engine);
}


void
rig_simulator_service_start (RigEngine *engine, int ipc_fd)
{
  engine->simulator_peer =
    rig_rpc_peer_new (engine,
                      ipc_fd,
                      &rig_simulator_service.base,
                      (ProtobufCServiceDescriptor *)&rig__renderer__descriptor,
                      simulator_peer_error_handler,
                      simulator_peer_connected,
                      engine);
}

void
rig_simulator_service_stop (RigEngine *engine)
{
  rut_refable_unref (engine->simulator_peer);
  engine->simulator_peer = NULL;
}
