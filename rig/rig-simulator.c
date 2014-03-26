/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013,2014  Intel Corporation.
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


#include "config.h"

#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <glib.h>

#include <rut.h>

#include "rig-engine.h"
#include "rig-engine-op.h"
#include "rig-pb.h"
#include "rig-ui.h"

#include "rig.pb-c.h"


typedef struct _RigSimulatorAction
{
  RigSimulatorActionType type;
  RutList list_node;
  union {
    struct {
      RutObject *object;
      RutSelectAction action;
    } select_object;
  };
} RigSimulatorAction;


#if 0
static char **_rig_editor_remaining_args = NULL;

static const GOptionEntry rut_editor_entries[] =
{
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY,
    &_rig_editor_remaining_args, "Project" },
  { 0 }
};
#endif

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

void
rig_simulator_action_select_object (RigSimulator *simulator,
                                    RutObject *object,
                                    RutSelectAction select_action)
{
  RigSimulatorAction *action = g_slice_new (RigSimulatorAction);

  action->type = RIG_SIMULATOR_ACTION_TYPE_SELECT_OBJECT;
  if (object)
    action->select_object.object = rut_object_ref (object);
  else
    action->select_object.object = NULL;
  action->select_object.action = select_action;

  rut_list_insert (simulator->actions.prev, &action->list_node);
  simulator->n_actions++;
}

static void
rig_simulator_action_report_edit_failure (RigSimulator *simulator)
{
  RigSimulatorAction *action = g_slice_new (RigSimulatorAction);

  action->type = RIG_SIMULATOR_ACTION_TYPE_REPORT_EDIT_FAILURE;

  rut_list_insert (simulator->actions.prev, &action->list_node);
  simulator->n_actions++;
}

static void
clear_actions (RigSimulator *simulator)
{
  RigSimulatorAction *action, *tmp;

  rut_list_for_each_safe (action, tmp, &simulator->actions, list_node)
    {
      switch (action->type)
        {
#if 0
        case RIG_SIMULATOR_ACTION_TYPE_SET_PLAY_MODE:
          break;
#endif
        case RIG_SIMULATOR_ACTION_TYPE_SELECT_OBJECT:
          if (action->select_object.object)
            rut_object_unref (action->select_object.object);
          break;
        case RIG_SIMULATOR_ACTION_TYPE_REPORT_EDIT_FAILURE:
          break;
        }

      rut_list_remove (&action->list_node);
      g_slice_free (RigSimulatorAction, action);
    }

  simulator->n_actions = 0;
}

static RutObject *
lookup_object (RigSimulator *simulator,
               uint64_t id)
{
  void *id_ptr = (void *)(intptr_t)id;
  return g_hash_table_lookup (simulator->id_to_object_map, id_ptr);
}

static void
register_object_cb (void *object,
                    uint64_t id,
                    void *user_data)
{
  RigSimulator *simulator = user_data;
  void *id_ptr;

  g_return_val_if_fail (id != 0, true);

  /* Assets can be shared between edit and play mode UIs so we
   * don't want to complain if we detect them being registered
   * multiple times
   */
  if (rut_object_get_type (object) == &rig_asset_type &&
      lookup_object (simulator, id))
    return;

  /* NB: We can assume that all IDs fit in a native pointer since IDs
   * sent to a simulator currently always correspond to pointers in
   * the frontend which has to be running on the same machine.
   */

  id_ptr = (void *)(intptr_t)id;

  g_hash_table_insert (simulator->object_to_id_map, object, id_ptr);
  g_hash_table_insert (simulator->id_to_object_map, id_ptr, object);
}

static void
unregister_object_cb (void *object, void *user_data)
{
  RigSimulator *simulator = user_data;
  void *id_ptr = g_hash_table_lookup (simulator->object_to_id_map, object);

  g_hash_table_remove (simulator->object_to_id_map, object);
  g_hash_table_remove (simulator->id_to_object_map, id_ptr);
}

static void *
unregister_id (RigSimulator *simulator, uint64_t id)
{
  void *id_ptr = (void *)(uintptr_t)id;
  void *object = g_hash_table_lookup (simulator->id_to_object_map, id_ptr);

  g_hash_table_remove (simulator->object_to_id_map, object);
  g_hash_table_remove (simulator->id_to_object_map, id_ptr);

  return object;
}

static void *
lookup_object_cb (uint64_t id,
                  void *user_data)
{
  return lookup_object (user_data, id);
}

static void
simulator__load (Rig__Simulator_Service *service,
                 const Rig__UI *pb_ui,
                 Rig__LoadResult_Closure closure,
                 void *closure_data)
{
  Rig__LoadResult result = RIG__LOAD_RESULT__INIT;
  RigSimulator *simulator =
    rig_pb_rpc_closure_get_connection_data (closure_data);
  RigEngine *engine = simulator->engine;
  RigUI *ui;

  g_return_if_fail (pb_ui != NULL);

  //g_print ("Simulator: UI Load Request\n");

  /* First make sure to cleanup the current ui  */
  if (pb_ui->mode == RIG__UI__MODE__EDIT)
    rig_engine_set_edit_mode_ui (engine, NULL);
  else
    rig_engine_set_play_mode_ui (engine, NULL);

  /* Kick garbage collection now so that all the objects being
   * replaced are unregistered before before we load the new UI.
   */
  rig_engine_garbage_collect (engine,
                              unregister_object_cb,
                              simulator);

  ui = rig_pb_unserialize_ui (simulator->ui_unserializer, pb_ui);

  g_warn_if_fail (pb_ui->has_mode);
  if (pb_ui->mode == RIG__UI__MODE__EDIT)
    rig_engine_set_edit_mode_ui (engine, ui);
  else
    rig_engine_set_play_mode_ui (engine, ui);

  rut_object_unref (ui);

  rig_engine_op_apply_context_set_ui (&simulator->apply_op_ctx, ui);

  closure (&result, closure_data);
}

static void
simulator__run_frame (Rig__Simulator_Service *service,
                      const Rig__FrameSetup *setup,
                      Rig__RunFrameAck_Closure closure,
                      void *closure_data)
{
  Rig__RunFrameAck ack = RIG__RUN_FRAME_ACK__INIT;
  RigSimulator *simulator =
    rig_pb_rpc_closure_get_connection_data (closure_data);
  RigEngine *engine = simulator->engine;
  int n_object_registrations;
  int i;

  g_return_if_fail (setup != NULL);

  /* Update all of our temporary IDs to real IDs given to us by the
   * frontend. */
  n_object_registrations = setup->n_object_registrations;
  if (n_object_registrations)
    {
      for (i = 0; i < n_object_registrations; i++)
        {
          Rig__ObjectRegistration *pb_registration =
            setup->object_registrations[i];
          void *object = unregister_id (simulator, pb_registration->temp_id);
          register_object_cb (object, pb_registration->real_id, simulator);
        }
    }

  /* Reset our temporary ID counter.
   *
   * Note: Since we know that the frontend will always allocate aligned
   * pointers as IDs we can use any odd number as a temporary ID... */
  simulator->next_tmp_id = 1;


  if (setup->has_dso)
    {
      rig_code_update_dso (engine, setup->dso.data, setup->dso.len);
    }

  //g_print ("Simulator: Run Frame Request: n_events = %d\n",
  //         setup->n_events);

  if (setup->has_view_width && setup->has_view_height &&
      (engine->window_width != setup->view_width ||
       engine->window_height != setup->view_height))
    {
      rig_engine_resize (engine, setup->view_width, setup->view_height);
    }

  if (setup->has_view_x)
    simulator->view_x = setup->view_x;
  if (setup->has_view_y)
    simulator->view_y = setup->view_y;

  if (setup->has_play_mode)
    rig_engine_set_play_mode_enabled (engine, setup->play_mode);

  for (i = 0; i < setup->n_events; i++)
    {
      Rig__Event *pb_event = setup->events[i];
      RutStreamEvent *event;

      if (!pb_event->has_type)
        {
          g_warning ("Event missing type");
          continue;
        }

      event = g_slice_new (RutStreamEvent);


      switch (pb_event->type)
        {
        case RIG__EVENT__TYPE__POINTER_MOVE:
          event->pointer_move.state = simulator->button_state;
          break;

        case RIG__EVENT__TYPE__POINTER_DOWN:
        case RIG__EVENT__TYPE__POINTER_UP:

          event->pointer_button.state = simulator->button_state;

          event->pointer_button.x = simulator->last_pointer_x;
          event->pointer_button.y = simulator->last_pointer_y;

          if (pb_event->pointer_button->has_button)
            event->pointer_button.button = pb_event->pointer_button->button;
          else
            {
              g_warn_if_reached ();
              event->pointer_button.button = RUT_BUTTON_STATE_1;
            }
          break;

        case RIG__EVENT__TYPE__KEY_DOWN:
        case RIG__EVENT__TYPE__KEY_UP:

          if (pb_event->key->has_keysym)
            event->key.keysym = pb_event->key->keysym;
          else
            {
              g_warn_if_reached ();
              event->key.keysym = RUT_KEY_a;
            }

          if (pb_event->key->has_mod_state)
            event->key.mod_state = pb_event->key->mod_state;
          else
            {
              g_warn_if_reached ();
              event->key.mod_state = 0;
            }
          break;
        }

      switch (pb_event->type)
        {
        case RIG__EVENT__TYPE__POINTER_MOVE:
          event->type = RUT_STREAM_EVENT_POINTER_MOVE;

          if (pb_event->pointer_move->has_x)
            {
              /* Note: we can translate all simulator events to
               * account for the position of a RigCameraView in
               * an editor. */
              event->pointer_move.x =
                pb_event->pointer_move->x - simulator->view_x;
            }
          else
            {
              g_warn_if_reached ();
              event->pointer_move.x = 0;
            }

          if (pb_event->pointer_move->has_y)
            {
              event->pointer_move.y =
                pb_event->pointer_move->y - simulator->view_y;
            }
          else
            {
              g_warn_if_reached ();
              event->pointer_move.y = 0;
            }

          simulator->last_pointer_x = event->pointer_move.x;
          simulator->last_pointer_y = event->pointer_move.y;

          //g_print ("Simulator: Read Event: Pointer move (%f, %f)\n",
          //         event->pointer_move.x, event->pointer_move.y);
          break;
        case RIG__EVENT__TYPE__POINTER_DOWN:
          event->type = RUT_STREAM_EVENT_POINTER_DOWN;
          simulator->button_state |= event->pointer_button.button;
          event->pointer_button.state |= event->pointer_button.button;
          //g_print ("Simulator: Read Event: Pointer down\n");
          break;
        case RIG__EVENT__TYPE__POINTER_UP:
          event->type = RUT_STREAM_EVENT_POINTER_UP;
          simulator->button_state &= ~event->pointer_button.button;
          event->pointer_button.state &= ~event->pointer_button.button;
          //g_print ("Simulator: Read Event: Pointer up\n");
          break;
        case RIG__EVENT__TYPE__KEY_DOWN:
          event->type = RUT_STREAM_EVENT_KEY_DOWN;
          //g_print ("Simulator: Read Event: Key down\n");
          break;
        case RIG__EVENT__TYPE__KEY_UP:
          event->type = RUT_STREAM_EVENT_KEY_UP;
          //g_print ("Simulator: Read Event: Key up\n");
          break;
        }

      rut_shell_handle_stream_event (engine->shell, event);
    }

  /*
   * Apply UI edit operations immediately
   */

  if (setup->play_edit)
    {
      if (!rig_engine_map_pb_ui_edit (&simulator->map_to_sim_objects_op_ctx,
                                      &simulator->apply_op_ctx,
                                      setup->play_edit))
        {
          rig_simulator_action_report_edit_failure (simulator);
        }
    }

  if (setup->edit)
    {
      bool status =
        rig_engine_map_pb_ui_edit (&simulator->map_to_sim_objects_op_ctx,
                                   &simulator->apply_op_ctx,
                                   setup->edit);

      g_warn_if_fail (status);
    }

  rut_shell_queue_redraw_real (engine->shell);

  closure (&ack, closure_data);
}

static void
simulator__synchronize (Rig__Simulator_Service *service,
                        const Rig__Sync *sync,
                        Rig__SyncAck_Closure closure,
                        void *closure_data)
{
  Rig__SyncAck ack = RIG__SYNC_ACK__INIT;

  /* XXX: currently we can assume that frames are processed
   * synchronously and so there are implicitly no outstanding
   * frames to process.
   */
  closure (&ack, closure_data);
}

static Rig__Simulator_Service rig_simulator_service =
  RIG__SIMULATOR__INIT(simulator__);


static void
handle_frontend_test_response (const Rig__TestResult *result,
                                void *closure_data)
{
  //g_print ("Renderer test response received\n");
}

static void
simulator_peer_connected (PB_RPC_Client *pb_client,
                          void *user_data)
{
  ProtobufCService *frontend_service =
    rig_pb_rpc_client_get_service (pb_client);
  Rig__Query query = RIG__QUERY__INIT;

  rig__frontend__test (frontend_service, &query,
                       handle_frontend_test_response, NULL);
  //g_print ("Simulator peer connected\n");
}

static void
simulator_stop_service (RigSimulator *simulator)
{
  rut_object_unref (simulator->simulator_peer);
  simulator->simulator_peer = NULL;

  /* For now we assume we would only stop the service due to an RPC
   * error and so we should quit this process... */
  exit (1);
}

static void
simulator_peer_error_handler (PB_RPC_Error_Code code,
                              const char *message,
                              void *user_data)
{
  RigSimulator *simulator = user_data;

  g_warning ("Simulator peer error: %s", message);

  simulator_stop_service (simulator);
}

static void
simulator_start_service (RutShell *shell, RigSimulator *simulator)
{
  simulator->simulator_peer =
    rig_rpc_peer_new (shell,
                      simulator->fd,
                      &rig_simulator_service.base,
                      (ProtobufCServiceDescriptor *)&rig__frontend__descriptor,
                      simulator_peer_error_handler,
                      simulator_peer_connected,
                      simulator);
}

static void
apply_op_cb (Rig__Operation *pb_op, void *user_data)
{
  RigSimulator *simulator = user_data;
  RutPropertyContext *prop_ctx = &simulator->engine->ctx->property_ctx;
  bool status;

  /* We sequence all operations relative to the property updates that
   * are being logged, so that the frontend will be able to replay
   * operation and property updates in the same order.
   */
  pb_op->has_sequence = true;
  pb_op->sequence = prop_ctx->log_len;

  status = rig_engine_pb_op_apply (&simulator->apply_op_ctx, pb_op);
  g_warn_if_fail (status);

  rut_queue_push_tail (simulator->ops, pb_op);
}

static uint64_t
temporarily_register_object_cb (void *object, void *user_data)
{
  RigSimulator *simulator = user_data;

  /* XXX: Since we know that the frontend will always allocate aligned
   * pointers as IDs we can use any odd number as a temporary ID */
  uint64_t id = simulator->next_tmp_id += 2;

  //void *id_ptr = (void *)(intptr_t)id;

  register_object_cb (object, id, simulator);

  //g_hash_table_insert (simulator->object_to_tmp_id_map, object, id_ptr);

  return id;
}

static uint64_t
lookup_object_id (RigSimulator *simulator, void *object)
{
  void *id_ptr = g_hash_table_lookup (simulator->object_to_id_map, object);

  if (G_UNLIKELY (id_ptr == NULL))
    {
      const char *label = "";
      if (rut_object_is (object, RUT_TRAIT_ID_INTROSPECTABLE))
        {
          RutProperty *label_prop =
            rut_introspectable_lookup_property (object, "label");
          if (label_prop)
            label = rut_property_get_text (label_prop);
        }
      g_warning ("Can't find an ID for unregistered object %p(%s,label=\"%s\")",
                 object,
                 rut_object_get_type_name (object),
                 label);
      return 0;
    }
  else
    return (uint64_t)(uintptr_t)id_ptr;
}

static uint64_t
lookup_object_id_cb (void *object, void *user_data)
{
  RigSimulator *simulator = user_data;
  return lookup_object_id (simulator, object);

#if 0
  void *id_ptr = g_hash_table_lookup (simulator->object_to_id_map, object);
  if (id_ptr)
    return (uint64_t)(intptr_t)id_ptr;

  /* If we didn't find an id in object_to_id_map that implies the
   * object is currently associated with a temporary id... */

  id_ptr = g_hash_table_lookup (simulator->object_to_tmp_id_map, object);
  return (uint64_t)(intptr_t)id_ptr;
#endif
}

static void
_rig_simulator_free (void *object)
{
  RigSimulator *simulator = object;

  clear_actions (simulator);

  rig_pb_unserializer_destroy (simulator->ui_unserializer);

  g_hash_table_destroy (simulator->object_to_id_map);

  g_hash_table_destroy (simulator->id_to_object_map);

  //g_hash_table_destroy (simulator->object_to_tmp_id_map);

  rig_engine_op_apply_context_destroy (&simulator->apply_op_ctx);

  rut_object_unref (simulator->engine);

  rut_object_unref (simulator->simulator_peer);

  rut_object_unref (simulator->ctx);
  rut_object_unref (simulator->shell);

  rut_object_free (RigSimulator, simulator);
}

RutType rig_simulator_type;

static void
_rig_simulator_init_type (void)
{
  rut_type_init (&rig_simulator_type, "RigSimulator", _rig_simulator_free);
}

static uint64_t
map_id_to_sim_object_cb (uint64_t id, void *user_data)
{
  RigSimulator *simulator = user_data;
  void *object = lookup_object (simulator, id);
  return (uint64_t)(uintptr_t)object;
}

static uint64_t
map_id_to_frontend_id_cb (uint64_t id, void *user_data)
{
  RigSimulator *simulator = user_data;
  void *object = (void *)(uintptr_t)id;
  return lookup_object_id (simulator, object);
}

static uint64_t
direct_object_id_cb (void *object, void *user_data)
{
  return (uint64_t)(uintptr_t)object;
}

RigSimulator *
rig_simulator_new (RigFrontendID frontend_id,
                   int fd)
{
  RigSimulator *simulator = rut_object_alloc0 (RigSimulator,
                                               &rig_simulator_type,
                                               _rig_simulator_init_type);
  RigPBUnSerializer *ui_unserializer;
  RigEngine *engine;

  simulator->frontend_id = frontend_id;

  switch (frontend_id)
    {
    case RIG_FRONTEND_ID_EDITOR:
      simulator->editable = true;
      break;
    case RIG_FRONTEND_ID_SLAVE:
      simulator->editable = true;
      break;
    case RIG_FRONTEND_ID_DEVICE:
      simulator->editable = false;
      break;
    }

  simulator->fd = fd;

  simulator->shell = rut_shell_new (true, /* headless */
                                    NULL, /* no init callback */
                                    NULL, /* no fini callback */
                                    rig_simulator_run_frame,
                                    simulator);

  rut_shell_set_queue_redraw_callback (simulator->shell,
                                       rig_simulator_queue_redraw_hook,
                                       simulator);

  simulator->ctx = rut_context_new (simulator->shell);

  rut_context_init (simulator->ctx);

  simulator->redraw_queued = false;

  /* XXX: Since we know that the frontend will always allocate aligned
   * pointers as IDs we can use any odd number as a temporary ID */
  simulator->next_tmp_id = 1;

  /* NB: We can assume that all IDs fit in a native pointer since IDs
   * sent to a simulator currently always correspond to pointers in
   * the frontend which has to be running on the same machine.
   */

  simulator->object_to_id_map =
    g_hash_table_new (NULL, /* direct hash */
                      NULL); /* direct key equal */

  simulator->id_to_object_map =
    g_hash_table_new (NULL, /* direct hash */
                      NULL); /* direct key equal */

#if 0
  /* When we create new objects in the simulator they are given a
   * temporary ID that we know won't collide with IDs from the
   * frontend... */
  simulator->object_to_tmp_id_map =
    g_hash_table_new (NULL, /* direct hash */
                      NULL); /* direct key equal */
#endif

  simulator->ops = rut_queue_new ();

  rut_list_init (&simulator->actions);

  simulator_start_service (simulator->shell, simulator);

  simulator->engine =
    rig_engine_new_for_simulator (simulator->shell, simulator, false);
  engine = simulator->engine;

  /*
   * This unserializer is used to unserialize UIs in simulator__load
   * for example...
   */
  ui_unserializer = rig_pb_unserializer_new (engine);
  rig_pb_unserializer_set_object_register_callback (ui_unserializer,
                                                    register_object_cb,
                                                    simulator);
  rig_pb_unserializer_set_id_to_object_callback (ui_unserializer,
                                                 lookup_object_cb,
                                                 simulator);
  simulator->ui_unserializer = ui_unserializer;



  rig_engine_op_apply_context_init (&simulator->apply_op_ctx,
                                    engine,
                                    register_object_cb,
                                    NULL, /* unregister id */
                                    simulator); /* user data */

  rig_engine_set_apply_op_callback (engine, apply_op_cb, simulator);

  rig_engine_op_map_context_init (&simulator->map_to_sim_objects_op_ctx,
                                  engine,
                                  map_id_to_sim_object_cb,
                                  simulator); /* user data */

  rig_engine_op_map_context_init (&simulator->map_to_frontend_ids_op_ctx,
                                  engine,
                                  map_id_to_frontend_id_cb,
                                  simulator); /* user data */

  /* The ops_serializer is used to serialize operations generated by
   * UI logic in the simulator that will be forwarded to the frontend.
   */
  rig_pb_serializer_set_object_register_callback (engine->ops_serializer,
                                                  temporarily_register_object_cb,
                                                  simulator);
  rig_pb_serializer_set_object_to_id_callback (engine->ops_serializer,
                                               direct_object_id_cb,
                                               simulator);

  return simulator;
}

void
rig_simulator_run (RigSimulator *simulator)
{
  rut_shell_main (simulator->shell);
}

static void
handle_update_ui_ack (const Rig__UpdateUIAck *result,
                      void *closure_data)
{
  //g_print ("Simulator: UI Update ACK received\n");
}

typedef struct _SerializeChangesState
{
  RigSimulator *simulator;
  RigPBSerializer *serializer;
  Rig__PropertyChange *pb_changes;
  Rig__PropertyValue *pb_values;
  int n_changes;
  int i;
} SerializeChangesState;

static void
stack_region_cb (uint8_t *data, size_t bytes, void *user_data)
{
  SerializeChangesState *state = user_data;
  RigSimulator *simulator = state->simulator;
  size_t step = sizeof (RutPropertyChange);
  size_t offset;
  int i;

  //g_print ("Properties changed log %d bytes:\n", bytes);

  for (i = state->i, offset = 0;
       i < state->n_changes && (offset + step) <= bytes;
       i++, offset += step)
    {
      RutPropertyChange *change =
        (RutPropertyChange *)(data + offset);
      Rig__PropertyChange *pb_change = &state->pb_changes[i];
      Rig__PropertyValue *pb_value = &state->pb_values[i];

      rig__property_change__init (pb_change);
      rig__property_value__init (pb_value);

      pb_change->has_object_id = true;
      pb_change->object_id = lookup_object_id (simulator, change->object);
      pb_change->has_property_id = true;
      pb_change->property_id = change->prop_id;
      rig_pb_property_value_init (state->serializer, pb_value, &change->boxed);

#if 1
      g_print ("> %d: base = %p, offset = %d, obj id=%llu:%p:%s, prop id = %d\n",
               i,
               data,
               offset,
               (uint64_t)pb_change->object_id,
               change->object,
               rut_object_get_type_name (change->object),
               change->prop_id);
#endif
    }

  state->i = i;
}

void
rig_simulator_run_frame (RutShell *shell, void *user_data)
{
  RigSimulator *simulator = user_data;
  RigEngine *engine = simulator->engine;
  ProtobufCService *frontend_service =
    rig_pb_rpc_client_get_service (simulator->simulator_peer->pb_rpc_client);
  Rig__UIDiff ui_diff;
  RutPropertyContext *prop_ctx = &engine->ctx->property_ctx;
  int n_changes;
  RigPBSerializer *serializer;

  simulator->redraw_queued = false;

  /* Setup the property context to log all property changes so they
   * can be sent back to the frontend process each frame. */
  simulator->ctx->property_ctx.log = true;

  //g_print ("Simulator: Start Frame\n");
  rut_shell_start_redraw (shell);

  rut_shell_update_timelines (shell);

  rut_shell_run_pre_paint_callbacks (shell);

  rut_shell_run_start_paint_callbacks (shell);

  rut_shell_dispatch_input_events (shell);

  if (engine->play_mode)
    {
      static int counter = 0;

      if (counter++ > 100)
        {
          RutObject *entity = rig_ui_find_entity (engine->current_ui,
                                                  "pinwheel");
          RutProperty *scale_prop;
          RutBoxed boxed;

          scale_prop = rut_introspectable_get_property (entity,
                                                        RUT_ENTITY_PROP_SCALE);
#if 1
          boxed.type = RUT_PROPERTY_TYPE_FLOAT;
          boxed.d.float_val = counter;
          rig_engine_op_set_property (engine,
                                      scale_prop,
                                      &boxed);
#else
          rut_property_set_float (&engine->ctx->property_ctx,
                                  scale_prop,
                                  counter);
#endif
        }

      g_print ("Frame = %d\n", counter);
    }


  if (rut_shell_check_timelines (shell))
    rut_shell_queue_redraw (shell);

  //g_print ("Simulator: Sending UI Update\n");

  n_changes = prop_ctx->log_len;
  serializer = rig_pb_serializer_new (engine);

  rig__uidiff__init (&ui_diff);

  ui_diff.n_property_changes = n_changes;
  if (n_changes)
    {
      SerializeChangesState state;
      int i;

      state.simulator = simulator;
      state.serializer = serializer;

      state.pb_changes =
        rut_memory_stack_memalign (engine->frame_stack,
                                   sizeof (Rig__PropertyChange) * n_changes,
                                   RUT_UTIL_ALIGNOF (Rig__PropertyChange));
      state.pb_values =
        rut_memory_stack_memalign (engine->frame_stack,
                                   sizeof (Rig__PropertyValue) * n_changes,
                                   RUT_UTIL_ALIGNOF (Rig__PropertyValue));

      state.i = 0;
      state.n_changes = n_changes;

      rut_memory_stack_foreach_region (prop_ctx->change_log_stack,
                                       stack_region_cb,
                                       &state);

      ui_diff.property_changes =
        rut_memory_stack_memalign (engine->frame_stack,
                                   sizeof (void *) * n_changes,
                                   RUT_UTIL_ALIGNOF (void *));

      for (i = 0; i < n_changes; i++)
        {
          ui_diff.property_changes[i] = &state.pb_changes[i];
          ui_diff.property_changes[i]->value = &state.pb_values[i];
        }
    }

  ui_diff.edit = rig_pb_new (engine->ops_serializer,
                             Rig__UIEdit,
                             rig__uiedit__init);
  ui_diff.edit->ops =
    rig_pb_serialize_ops_queue (engine->ops_serializer, simulator->ops);
  rut_queue_clear (simulator->ops);

  rig_engine_map_pb_ui_edit (&simulator->map_to_frontend_ids_op_ctx,
                             NULL, /* no apply ctx, since ops already applied */
                             ui_diff.edit);

  ui_diff.n_actions = simulator->n_actions;
  if (ui_diff.n_actions)
    {
      Rig__SimulatorAction *pb_actions;
      RigSimulatorAction *action, *tmp;
      int i;

      ui_diff.actions =
        rut_memory_stack_memalign (engine->frame_stack,
                                   sizeof (void *) * ui_diff.n_actions,
                                   RUT_UTIL_ALIGNOF (void *));
      pb_actions =
        rut_memory_stack_memalign (engine->frame_stack,
                                   sizeof (Rig__SimulatorAction) *
                                   ui_diff.n_actions,
                                   RUT_UTIL_ALIGNOF (Rig__SimulatorAction));

      i = 0;
      rut_list_for_each_safe (action, tmp, &simulator->actions, list_node)
        {
          Rig__SimulatorAction *pb_action = &pb_actions[i];

          rig__simulator_action__init (pb_action);

          pb_action->type = action->type;

          switch (action->type)
            {
            case RIG_SIMULATOR_ACTION_TYPE_REPORT_EDIT_FAILURE:
              pb_action->report_edit_failure =
                rig_pb_new (serializer,
                            Rig__SimulatorAction__ReportEditFailure,
                            rig__simulator_action__report_edit_failure__init);
              break;
            case RIG_SIMULATOR_ACTION_TYPE_SELECT_OBJECT:
              pb_action->select_object =
                rig_pb_new (serializer,
                            Rig__SimulatorAction__SelectObject,
                            rig__simulator_action__select_object__init);
              if (action->select_object.object)
                {
                  pb_action->select_object->object_id =
                    lookup_object_id (simulator, action->select_object.object);
                }
              else
                pb_action->select_object->object_id = 0;
              pb_action->select_object->action = action->select_object.action;
              break;
            }

          ui_diff.actions[i] = pb_action;

          i++;
        }
    }

  clear_actions (simulator);


  rig__frontend__update_ui (frontend_service,
                            &ui_diff,
                            handle_update_ui_ack,
                            NULL);

  rig_pb_serializer_destroy (serializer);

  rut_property_context_clear_log (prop_ctx);

  /* Stop logging property changes until the next frame. */
  simulator->ctx->property_ctx.log = false;

  rut_shell_run_post_paint_callbacks (shell);

  /* Garbage collect deleted objects
   *
   * XXX: We defer the freeing of objects until we have finished a
   * frame so that we can send our ui update back to the frontend
   * faster and handle freeing while we wait for new work from the
   * frontend.
   */
  rig_engine_garbage_collect (engine,
                              unregister_object_cb,
                              simulator);

  rut_memory_stack_rewind (engine->frame_stack);

  rut_shell_end_redraw (shell);
}

/* Redrawing in the simulator is driven by the frontend issuing
 * RunFrame requests, so we hook into rut_shell_queue_redraw()
 * just to record that we have work to do, but still wait for
 * a request from the frontend.
 */
void
rig_simulator_queue_redraw_hook (RutShell *shell, void *user_data)
{
  RigSimulator *simulator = user_data;

  simulator->redraw_queued = true;
}
