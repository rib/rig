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
#include "rig-frontend-service.h"
#include "rig.pb-c.h"

static void
frontend__test (Rig__Frontend_Service *service,
                 const Rig__Query *query,
                 Rig__TestResult_Closure closure,
                 void *closure_data)
{
  Rig__TestResult result = RIG__TEST_RESULT__INIT;
  //RigFrontend *frontend = rig_pb_rpc_closure_get_connection_data (closure_data);

  g_return_if_fail (query != NULL);

  g_print ("Frontend Service: Test Query\n");

  closure (&result, closure_data);
}

static void
frontend__update_ui (Rig__Frontend_Service *service,
                     const Rig__UIDiff *ui_diff,
                     Rig__UpdateUIAck_Closure closure,
                     void *closure_data)
{
  Rig__UpdateUIAck ack = RIG__UPDATE_UIACK__INIT;
  //RigFrontend *frontend =
  //  rig_pb_rpc_closure_get_connection_data (closure_data);

  g_return_if_fail (ui_diff != NULL);

  g_print ("Frontend: Update UI Request\n");

  closure (&ack, closure_data);
}

static Rig__Frontend_Service rig_frontend_service =
  RIG__FRONTEND__INIT(frontend__);


static void
handle_simulator_test_response (const Rig__TestResult *result,
                                void *closure_data)
{
  g_print ("Simulator test response received\n");
}

static void
frontend_peer_connected (PB_RPC_Client *pb_client,
                         void *user_data)
{
  ProtobufCService *simulator_service =
    rig_pb_rpc_client_get_service (pb_client);
  Rig__Query query = RIG__QUERY__INIT;

  rig__simulator__test (simulator_service, &query,
                        handle_simulator_test_response, NULL);
  g_print ("Frontend peer connected\n");
}

static void
frontend_peer_error_handler (PB_RPC_Error_Code code,
                             const char *message,
                             void *user_data)
{
  RigFrontend *frontend = user_data;

  g_warning ("Frontend peer error: %s", message);

  rig_frontend_service_stop (frontend);
}

void
rig_frontend_service_start (RigFrontend *frontend)
{
  frontend->frontend_peer =
    rig_rpc_peer_new (frontend->engine,
                           frontend->fd,
                           &rig_frontend_service.base,
                           (ProtobufCServiceDescriptor *)&rig__simulator__descriptor,
                           frontend_peer_error_handler,
                           frontend_peer_connected,
                           frontend);
}

void
rig_frontend_service_stop (RigFrontend *frontend)
{
  rut_refable_unref (frontend->frontend_peer);
  frontend->frontend_peer = NULL;
}
