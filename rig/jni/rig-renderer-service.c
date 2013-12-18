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

#include <rut.h>

#include "rig-engine.h"
#include "rig-renderer-service.h"
#include "rig.pb-c.h"

static void
renderer__test (Rig__Renderer_Service *service,
                 const Rig__Query *query,
                 Rig__TestResult_Closure closure,
                 void *closure_data)
{
  Rig__TestResult result = RIG__TEST_RESULT__INIT;
  //RigRenderer *renderer = rig_pb_rpc_closure_get_connection_data (closure_data);

  g_return_if_fail (query != NULL);

  g_print ("Renderer Service: Test Query\n");

  closure (&result, closure_data);
}

static void
renderer__update_ui (Rig__Renderer_Service *service,
                     const Rig__UIDiff *ui_diff,
                     Rig__UpdateUIAck_Closure closure,
                     void *closure_data)
{
  Rig__UpdateUIAck ack = RIG__UPDATE_UIACK__INIT;
  RigEngine *engine =
    rig_pb_rpc_closure_get_connection_data (closure_data);

  g_return_if_fail (ui_diff != NULL);

  g_print ("Renderer: Update UI Request\n");

  closure (&ack, closure_data);
}

static Rig__Renderer_Service rig_renderer_service =
  RIG__RENDERER__INIT(renderer__);


static void
handle_simulator_test_response (const Rig__TestResult *result,
                                void *closure_data)
{
  g_print ("Simulator test response received\n");
}

static void
renderer_peer_connected (PB_RPC_Client *pb_client,
                         void *user_data)
{
  ProtobufCService *simulator_service =
    rig_pb_rpc_client_get_service (pb_client);
  Rig__Query query = RIG__QUERY__INIT;

  rig__simulator__test (simulator_service, &query,
                        handle_simulator_test_response, NULL);
  g_print ("Renderer peer connected\n");
}

static void
renderer_peer_error_handler (PB_RPC_Error_Code code,
                             const char *message,
                             void *user_data)
{
  RigEngine *engine = user_data;

  g_warning ("Renderer peer error: %s", message);

  rig_renderer_service_stop (engine);
}

void
rig_renderer_service_start (RigEngine *engine, int ipc_fd)
{
  engine->simulator_peer =
    rig_rpc_peer_new (engine,
                      ipc_fd,
                      &rig_renderer_service.base,
                      (ProtobufCServiceDescriptor *)&rig__simulator__descriptor,
                      renderer_peer_error_handler,
                      renderer_peer_connected,
                      engine);
}

void
rig_renderer_service_stop (RigEngine *engine)
{
  rut_refable_unref (engine->simulator_peer);
  engine->simulator_peer = NULL;
}
