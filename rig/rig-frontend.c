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

#include <stdlib.h>
#include <sys/socket.h>

#include <rut.h>

#include "rig-engine.h"
#include "rig-frontend.h"
#include "rig-pb.h"

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


#if 0
static void
handle_simulator_test_response (const Rig__TestResult *result,
                                void *closure_data)
{
  g_print ("Simulator test response received\n");
}
#endif

typedef struct _LoadState
{
  GList *required_assets;
} LoadState;

bool
asset_filter_cb (RutAsset *asset,
                 void *user_data)
{
   switch (rut_asset_get_type (asset))
    {
    case RUT_ASSET_TYPE_BUILTIN:
    case RUT_ASSET_TYPE_TEXTURE:
    case RUT_ASSET_TYPE_NORMAL_MAP:
    case RUT_ASSET_TYPE_ALPHA_MASK:
      return false; /* these assets aren't needed in the simulator */
    case RUT_ASSET_TYPE_PLY_MODEL:
      return true; /* keep mesh assets for picking */
      g_print ("Serialization requires asset %s\n",
               rut_asset_get_path (asset));
      break;
    }

   g_warn_if_reached ();
   return false;
}

static void
handle_load_response (const Rig__LoadResult *result,
                      void *closure_data)
{
  g_print ("Simulator: UI loaded\n");
}

static void
frontend_peer_connected (PB_RPC_Client *pb_client,
                         void *user_data)
{
  RigPBSerializer *serializer;
  RigFrontend *frontend = user_data;
  ProtobufCService *simulator_service =
    rig_pb_rpc_client_get_service (pb_client);
  Rig__UI *ui;

  serializer = rig_pb_serializer_new (frontend->engine);

  rig_pb_serializer_set_asset_filter (serializer,
                                      asset_filter_cb,
                                      NULL);

  ui = rig_pb_serialize_ui (serializer);

  rig__simulator__load (simulator_service, ui,
                        handle_load_response,
                        NULL);

  rig_pb_serialized_ui_destroy (ui);

#if 0
  Rig__Query query = RIG__QUERY__INIT;


  rig__simulator__test (simulator_service, &query,
                        handle_simulator_test_response, NULL);
#endif

  rig_pb_serializer_destroy (serializer);

  g_print ("Frontend peer connected\n");
}

static void
_rig_frontend_free (void *object)
{
  RigFrontend *frontend = object;

  rig_frontend_stop_service (frontend);

  rut_refable_unref (frontend->engine);

  rig_frontend_stop_service (frontend);

  g_slice_free (RigFrontend, object);
}

RutType rig_frontend_type;

static void
_rig_frontend_init_type (void)
{
  RutType *type = &rig_frontend_type;
#define TYPE RigFrontend

  rut_type_init (type, G_STRINGIFY (TYPE));
  rut_type_add_refable (type, ref_count, _rig_frontend_free);

#undef TYPE
}

RigFrontend *
rig_frontend_new (RutShell *shell,
                  const char *ui_filename)
{
  pid_t pid;
  int sp[2];

  /*
   * Spawn a simulator process...
   */

  if (socketpair (AF_UNIX, SOCK_STREAM, 0, sp) < 0)
    g_error ("Failed to open simulator ipc");

  pid = fork ();
  if (pid == 0)
    {
      char fd_str[10];
      char *path = RIG_BIN_DIR "rig-simulator";

      /* child - simulator process */
      close (sp[0]);

      if (snprintf (fd_str, sizeof (fd_str), "%d", sp[1]) >= sizeof (fd_str))
        g_error ("Failed to setup environment for simulator process");

      setenv ("_RIG_IPC_FD", fd_str, true);

#ifdef RIG_ENABLE_DEBUG
      if (getenv ("RIG_SIMULATOR"))
        path = getenv ("RIG_SIMULATOR");
#endif

      if (execl (path, path, NULL) < 0)
        g_error ("Failed to run simulator process");

      return NULL;
    }
  else
    {
      RigFrontend *frontend = rut_object_alloc0 (RigFrontend,
                                                 &rig_frontend_type,
                                                 _rig_frontend_init_type);

      frontend->ref_count = 1;

      frontend->simulator_pid = pid;
      frontend->fd = sp[0];

      rig_frontend_start_service (frontend);

      frontend->engine =
        rig_engine_new_for_frontend (shell, frontend, ui_filename);

      return frontend;
    }
}

static void
frontend_peer_error_handler (PB_RPC_Error_Code code,
                             const char *message,
                             void *user_data)
{
  RigFrontend *frontend = user_data;

  g_warning ("Frontend peer error: %s", message);

  rig_frontend_stop_service (frontend);
}

void
rig_frontend_start_service (RigFrontend *frontend)
{
  frontend->frontend_peer =
    rig_rpc_peer_new (frontend->fd,
                      &rig_frontend_service.base,
                      (ProtobufCServiceDescriptor *)&rig__simulator__descriptor,
                      frontend_peer_error_handler,
                      frontend_peer_connected,
                      frontend);
}

void
rig_frontend_stop_service (RigFrontend *frontend)
{
  rut_refable_unref (frontend->frontend_peer);
  frontend->frontend_peer = NULL;
}
