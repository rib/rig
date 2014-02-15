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
register_object_cb (void *object,
                    uint64_t id,
                    void *user_data)
{
  RigFrontend *frontend = user_data;

  /* If the ID is an odd number that implies it is a temporary ID that
   * we need to be able map... */
  if (id & 1)
    {
      void *id_ptr = (void *)(uintptr_t)id;
      g_hash_table_insert (frontend->tmp_id_to_object_map, id_ptr, object);
    }
}

static void *
lookup_object (RigFrontend *frontend, uint64_t id)
{
  /* If the ID is an odd number that implies it is a temporary ID that
   * needs mapping... */
  if (id & 1)
    {
      void *id_ptr = (void *)(uintptr_t)id;
      return g_hash_table_lookup (frontend->tmp_id_to_object_map, id_ptr);
    }
  else /* Otherwise we can assume the ID corresponds to an object pointer */
    return (void *)(intptr_t)id;
}

static void *
lookup_object_cb (uint64_t id, void *user_data)
{
  RigFrontend *frontend = user_data;
  return lookup_object (frontend, id);
}

static void
apply_property_change (RigFrontend *frontend,
                       RigPBUnSerializer *unserializer,
                       Rig__PropertyChange *pb_change)
{
  void *object;
  RutProperty *property;
  RutBoxed boxed;

  if (!pb_change->has_object_id ||
      pb_change->object_id == 0 ||
      !pb_change->has_property_id ||
      !pb_change->value)
    {
      g_warning ("Frontend: Invalid property change received");
      return;
    }

  object = lookup_object (frontend, pb_change->object_id);

#if 1
  g_print ("Frontend: PropertyChange: %p(%s) prop_id=%d\n",
           object,
           rut_object_get_type_name (object),
           pb_change->property_id);
#endif

  property =
    rut_introspectable_get_property (object, pb_change->property_id);
  if (!property)
    {
      g_warning ("Frontend: Failed to find object property by id");
      return;
    }

  /* XXX: ideally we shouldn't need to init a RutBoxed and set
   * that on a property, and instead we can just directly
   * apply the value to the property we have. */
  rig_pb_init_boxed_value (unserializer,
                           &boxed,
                           property->spec->type,
                           pb_change->value);

  //#warning "XXX: frontend updates are disabled"
  rut_property_set_boxed  (&frontend->engine->ctx->property_ctx,
                           property, &boxed);
}

static void
queue_delete_object_cb (uint64_t id, void *user_data)
{
  RigFrontend *frontend = user_data;
  void *object;

  /* If the ID is an odd number that implies it is a temporary ID that
   * needs mapping... */
  if (id & 1)
    {
      void *id_ptr = (void *)(uintptr_t)id;
      object = g_hash_table_lookup (frontend->tmp_id_to_object_map, id_ptr);

      /* Remove the mapping immediately */
      g_hash_table_remove (frontend->tmp_id_to_object_map, id_ptr);
    }
  else
    object = (void *)(intptr_t)id;

  rig_engine_queue_delete (frontend->engine, object);
}

static void
frontend__update_ui (Rig__Frontend_Service *service,
                     const Rig__UIDiff *ui_diff,
                     Rig__UpdateUIAck_Closure closure,
                     void *closure_data)
{
  Rig__UpdateUIAck ack = RIG__UPDATE_UIACK__INIT;
  RigFrontend *frontend =
    rig_pb_rpc_closure_get_connection_data (closure_data);
  RigEngine *engine = frontend->engine;
  int i, j;
  int n_changes;
  int n_actions;
  RigPBUnSerializer *unserializer;
  RutBoxed boxed;
  RigEngineOpApplyContext *apply_op_ctx;

  //g_print ("Frontend: Update UI Request\n");

  frontend->ui_update_pending = false;

  g_return_if_fail (ui_diff != NULL);

  n_changes = ui_diff->n_property_changes;

  apply_op_ctx = &frontend->apply_op_ctx;
  unserializer = fronted->prop_change_unserializer;

  /* For compactness, property changes are serialized separately from
   * more general UI edit operations and so we need to take care that
   * we apply property changes and edit operations in the correct
   * order, using the operation sequences to relate to the sequence
   * of property changes.
   */
  j = 0;
  for (i = 0; i < pb_ui_edit->n_ops; i++)
    {
      Rig__Operation *pb_op = pb_ui_edit->ops[i];
      int until = pb_op->sequence;

      for (; j < until; j++)
        {
          Rig__PropertyChange *pb_change = ui_diff->property_changes[j];
          apply_property_change (frontend, unserializer, pb_change);
        }

      status = _rig_engine_ops[pb_op->type].apply_op (apply_op_ctx, pb_op);

      g_warn_if_fail (status);
    }

  for (; j < ui_diff->n_changes; j++)
    {
      Rig__PropertyChange *pb_change = ui_diff->property_changes[j];
      apply_property_change (frontend, unserializer, pb_change);
    }

  n_actions = ui_diff->n_actions;
  for (i = 0; i < n_actions; i++)
    {
      Rig__SimulatorAction *pb_action = ui_diff->actions[i];
      switch (pb_action->type)
        {
#if 0
        case RIG_SIMULATOR_ACTION_TYPE_SET_PLAY_MODE:
          rig_camera_view_set_play_mode_enabled (engine->main_camera_view,
                                                 pb_action->set_play_mode->enabled);
          break;
#endif
        case RIG_SIMULATOR_ACTION_TYPE_SELECT_OBJECT:
          {
            Rig__SimulatorAction__SelectObject *pb_select_object =
              pb_action->select_object;

            RutObject *object = (void *)(intptr_t)pb_select_object->object_id;
            rig_select_object (engine,
                               object,
                               pb_select_object->action);
            _rig_engine_update_inspector (engine);
            break;
          }
        }
    }

  if (ui_diff->has_queued_frame)
    rut_shell_queue_redraw (engine->shell);

  closure (&ack, closure_data);

  /* XXX: The current use case we have forui update callbacks
   * requires that the frontend be in-sync with the simulator so
   * we invoke them after we have applied all the operations from
   * the simulator */
  rut_closure_list_invoke (&frontend->ui_update_cb_list,
                           RigFrontendUIUpdateCallback,
                           frontend);
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
  g_print ("Frontend: UI loaded response received from simulator\n");
}

void
rig_frontend_forward_simulator_ui (RigFrontend *frontend,
                                   Rig__UI *pb_ui,
                                   bool play_mode)
{
  ProtobufCService *simulator_service;

  if (!frontend->connected)
    return;

  simulator_service =
    rig_pb_rpc_client_get_service (frontend->frontend_peer->pb_rpc_client);

  rig__simulator__load (simulator_service, pb_ui,
                        handle_load_response,
                        NULL);
}

void
rig_frontend_reload_simulator_ui (RigFrontend *frontend,
                                  RigUI *ui,
                                  bool play_mode)
{
  RigEngine *engine;
  RigPBSerializer *serializer;
  ProtobufCService *simulator_service;
  Rig__UI *pb_ui;

  if (!frontend->connected)
    return;

  engine = frontend->engine;
  serializer = rig_pb_serializer_new (engine);

  rig_pb_serializer_set_use_pointer_ids_enabled (serializer, true);

  rig_pb_serializer_set_asset_filter (serializer,
                                      asset_filter_cb,
                                      NULL);

  pb_ui = rig_pb_serialize_ui (serializer, play_mode, ui);

  rig_frontend_forward_simulator_ui (frontend, pb_ui, play_mode);

  rig_pb_serialized_ui_destroy (pb_ui);

  rig_pb_serializer_destroy (serializer);
}

static void
frontend_peer_connected (PB_RPC_Client *pb_client,
                         void *user_data)
{
  RigFrontend *frontend = user_data;

  frontend->connected = true;

  if (frontend->simulator_connected_callback)
    {
      void *user_data = frontend->simulator_connected_data;
      frontend->simulator_connected_callback (user_data);
    }

#if 0
  Rig__Query query = RIG__QUERY__INIT;


  rig__simulator__test (simulator_service, &query,
                        handle_simulator_test_response, NULL);
#endif

  g_print ("Frontend peer connected\n");
}

void
rig_frontend_set_simulator_connected_callback (RigFrontend *frontend,
                                               void (*callback) (void *user_data),
                                               void *user_data)
{
  frontend->simulator_connected_callback = callback;
  frontend->simulator_connected_data = user_data;
}

/* TODO: should support a destroy_notify callback */
void
rig_frontend_sync (RigFrontend *frontend,
                   void (*synchronized) (const Rig__SyncAck *result,
                                         void *user_data),
                   void *user_data)
{
  ProtobufCService *simulator_service;
  Rig__Sync sync = RIG__SYNC__INIT;

  if (!frontend->connected)
    return;

  simulator_service =
    rig_pb_rpc_client_get_service (frontend->frontend_peer->pb_rpc_client);

  rig__simulator__synchronize (simulator_service,
                               &sync,
                               synchronized,
                               user_data);
}

static void
frame_running_ack (const Rig__RunFrameAck *ack,
                   void *closure_data)
{
  g_print ("Frontend: Run Frame ACK received from simulator\n");
}

typedef struct _RegistrationState
{
  int index;
  Rig__ObjectRegistration *pb_registrations;
  Rig__ObjectRegistration **object_registrations;
} RegistrationState;

static void
register_temporary_cb (gpointer key,
                       gpointer value,
                       gpointer user_data)
{
  RegistrationState *state = user_data;
  Rig__ObjectRegistration *pb_registration = state->registrations[state->index];
  uint64_t id = (uint64_t)(uintptr_t)key;
  void *object = value;

  rig__object_registration__init (pb_registration);
  pb_registration->temp_id = id;
  pb_registration->real_id = (uint64_t)(uintptr_t)object;

  state->object_registrations[state->index++] = pb_registration;
}

void
rig_frontend_run_simulator_frame (RigFrontend *frontend,
                                  Rig__FrameSetup *setup)
{
  ProtobufCService *simulator_service;
  int n_temps;

  if (!frontend->connected)
    return;

  simulator_service =
    rig_pb_rpc_client_get_service (frontend->frontend_peer->pb_rpc_client);

  /* When UI logic in the simulator creates objects, they are
   * initially given a temporary ID until the corresponding object
   * has been created in the frontend. Before running the
   * next simulator frame we send it back the real IDs that have been
   * registered to replace those temporary IDs...
   */
  n_temps = g_hash_table_size (frontend->tmp_id_to_object_map);
  if (n_temps)
    {
      RegistrationState state;

      setup->n_object_registrations = n_temps;
      setup->object_registrations =
        rut_memory_stack_memalign (engine->frame_stack,
                                   sizeof (void *) * n_temps,
                                   RUT_UTIL_ALIGNOF (void *));

      state.index = 0;
      state.object_registrations = setup->object_registrations;
      state.pb_registrations =
        rut_memory_stack_memalign (engine->frame_stack,
                                   sizeof (Rig__ObjectRegistration) * n_temps,
                                   RUT_UTIL_ALIGNOF (Rig__ObjectRegistration));

      g_hash_table_foreach_remove (frontend->tmp_id_to_object_map,
                                   register_temporary_cb,
                                   &state);
    }

  rig__simulator__run_frame (simulator_service,
                             &setup,
                             frame_running_ack,
                             NULL); /* user data */
  frontend->ui_update_pending = true;
}

static void
_rig_frontend_free (void *object)
{
  RigFrontend *frontend = object;

  rig_engine_op_apply_context_destroy (&frontend->apply_op_ctx);
  rig_pb_unserializer_destroy (frontend->prop_change_unserializer);

  rut_closure_list_disconnect_all (&frontend->ui_update_cb_list);

  rig_frontend_stop_service (frontend);

  rut_object_unref (frontend->engine);

  g_hash_table_destroy (frontend->tmp_id_to_object_map);

  rut_object_free (RigFrontend, object);
}

RutType rig_frontend_type;

static void
_rig_frontend_init_type (void)
{
  rut_type_init (&rig_frontend_type, "RigFrontend", _rig_frontend_free);
}

void
rig_frontend_spawn_simulator (RigFrontend *frontend)
{
  pid_t pid;
  int sp[2];

  g_return_if_fail (frontend->connected == false);

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

      switch (frontend->id)
        {
        case RIG_FRONTEND_ID_EDITOR:
          setenv ("_RIG_FRONTEND", "editor", true);
          break;
        case RIG_FRONTEND_ID_SLAVE:
          setenv ("_RIG_FRONTEND", "slave", true);
          break;
        case RIG_FRONTEND_ID_DEVICE:
          setenv ("_RIG_FRONTEND", "device", true);
          break;
        }

#ifdef RIG_ENABLE_DEBUG
      if (getenv ("RIG_SIMULATOR"))
        path = getenv ("RIG_SIMULATOR");
#endif

      if (execl (path, path, NULL) < 0)
        g_error ("Failed to run simulator process");

      return;
    }

  frontend->simulator_pid = pid;
  frontend->fd = sp[0];

  rig_frontend_start_service (frontend);
}

RigFrontend *
rig_frontend_new (RutShell *shell,
                  RigFrontendID id,
                  const char *ui_filename)
{
  RigFrontend *frontend = rut_object_alloc0 (RigFrontend,
                                             &rig_frontend_type,
                                             _rig_frontend_init_type);
  RigPBUnSerializer *unserializer;

  frontend->id = id;

  frontend->tmp_id_to_object_map = g_hash_table_new (NULL, NULL);

  rut_list_init (&frontend->ui_update_cb_list);

  rig_engine_op_apply_context_init (&frontend->apply_op_ctx,
                                    register_object_cb,
                                    lookup_object_cb,
                                    queue_delete_object_cb,
                                    frontend);

  rig_frontend_spawn_simulator (frontend);

  frontend->engine =
    rig_engine_new_for_frontend (shell, frontend, ui_filename);

  unserializer = rig_pb_unserializer_new (frontend->engine);
  /* Just to make sure we don't mistakenly use this unserializer to
   * register any objects... */
  rig_pb_unserializer_set_object_register_callback (unserializer, NULL, NULL);
  rig_pb_unserializer_set_id_to_object_callback (unserializer,
                                                 lookup_object_cb,
                                                 frontend);
  frontend->prop_change_unserializer = unserializer;

  return frontend;
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
  rut_object_unref (frontend->frontend_peer);
  frontend->frontend_peer = NULL;
  frontend->connected = false;
}

RutClosure *
rig_frontend_add_ui_update_callback (RigFrontend *frontend,
                                     RigFrontendUIUpdateCallback callback,
                                     void *user_data,
                                     RutClosureDestroyCallback destroy)
{
  return rut_closure_list_add (&frontend->ui_update_cb_list,
                               callback,
                               user_data,
                               destroy);
}
