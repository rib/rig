#include "config.h"

#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <glib.h>

#include <rut.h>

#include "rig-engine.h"
#include "rig-pb.h"

#include "rig.pb-c.h"

#if 0
static char **_rig_editor_remaining_args = NULL;

static const GOptionEntry rut_editor_entries[] =
{
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY,
    &_rig_editor_remaining_args, "Project" },
  { 0 }
};
#endif

static RutMagazine *_rig_simulator_object_id_magazine = NULL;

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

static bool
register_object_cb (void *object,
                    uint64_t id,
                    void *user_data)
{
  RigSimulator *simulator = user_data;
  uint64_t *object_id =
    rut_magazine_chunk_alloc (_rig_simulator_object_id_magazine);

  *object_id = id;

  g_hash_table_insert (simulator->id_map, object, object_id);

  return false; /* also register normally in rig-pb.c */
}

static void
simulator__load (Rig__Simulator_Service *service,
                 const Rig__UI *ui,
                 Rig__LoadResult_Closure closure,
                 void *closure_data)
{
  Rig__LoadResult result = RIG__LOAD_RESULT__INIT;
  RigSimulator *simulator =
    rig_pb_rpc_closure_get_connection_data (closure_data);
  RigEngine *engine = simulator->engine;
  RigPBUnSerializer unserializer;

  g_return_if_fail (ui != NULL);

  g_print ("Simulator: UI Load Request\n");

  rig_pb_unserializer_init (&unserializer, engine,
                            true); /* with id-map */

  rig_pb_unserializer_set_object_register_callback (&unserializer,
                                                    register_object_cb,
                                                    simulator);

  rig_pb_unserialize_ui (&unserializer, ui, false);

  rig_pb_unserializer_destroy (&unserializer);

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
  int i;

  g_return_if_fail (setup != NULL);

  g_print ("Simulator: Run Frame Request: n_events = %d\n",
           setup->n_events);

  if (setup->has_width && setup->has_height &&
      (engine->width != setup->width ||
       engine->height != setup->height))
    {
      rig_engine_resize (engine, setup->width, setup->height);
    }

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
            event->pointer_move.x = pb_event->pointer_move->x;
          else
            {
              g_warn_if_reached ();
              event->pointer_move.x = 0;
            }

          if (pb_event->pointer_move->has_y)
            event->pointer_move.y = pb_event->pointer_move->y;
          else
            {
              g_warn_if_reached ();
              event->pointer_move.y = 0;
            }

          simulator->last_pointer_x = event->pointer_move.x;
          simulator->last_pointer_y = event->pointer_move.y;

          g_print ("Event: Pointer move (%f, %f)\n",
                   event->pointer_move.x, event->pointer_move.y);
          break;
        case RIG__EVENT__TYPE__POINTER_DOWN:
          event->type = RUT_STREAM_EVENT_POINTER_DOWN;
          simulator->button_state |= event->pointer_button.button;
          event->pointer_button.state |= event->pointer_button.button;
          g_print ("Event: Pointer down\n");
          break;
        case RIG__EVENT__TYPE__POINTER_UP:
          event->type = RUT_STREAM_EVENT_POINTER_UP;
          simulator->button_state &= ~event->pointer_button.button;
          event->pointer_button.state &= ~event->pointer_button.button;
          g_print ("Event: Pointer up\n");
          break;
        case RIG__EVENT__TYPE__KEY_DOWN:
          event->type = RUT_STREAM_EVENT_KEY_DOWN;
          g_print ("Event: Key down\n");
          break;
        case RIG__EVENT__TYPE__KEY_UP:
          event->type = RUT_STREAM_EVENT_KEY_UP;
          g_print ("Event: Key up\n");
          break;
        }

      rut_shell_handle_stream_event (engine->shell, event);
    }

  rut_shell_queue_redraw (engine->shell);

  closure (&ack, closure_data);
}

static Rig__Simulator_Service rig_simulator_service =
  RIG__SIMULATOR__INIT(simulator__);


static void
handle_frontend_test_response (const Rig__TestResult *result,
                                void *closure_data)
{
  g_print ("Renderer test response received\n");
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
  g_print ("Simulator peer connected\n");
}

static void
simulator_peer_error_handler (PB_RPC_Error_Code code,
                              const char *message,
                              void *user_data)
{
  RigSimulator *simulator = user_data;

  g_warning ("Simulator peer error: %s", message);

  rig_simulator_stop_service (simulator);
}

void
rig_simulator_start_service (RigSimulator *simulator)
{
  simulator->simulator_peer =
    rig_rpc_peer_new (simulator->fd,
                      &rig_simulator_service.base,
                      (ProtobufCServiceDescriptor *)&rig__frontend__descriptor,
                      simulator_peer_error_handler,
                      simulator_peer_connected,
                      simulator);
}

void
rig_simulator_stop_service (RigSimulator *simulator)
{
  rut_object_unref (simulator->simulator_peer);
  simulator->simulator_peer = NULL;

  /* For now we assume we would only stop the service due to an RPC
   * error and so we should quit this process... */
  exit (1);
}

static void
free_object_id (void *object_id)
{
  rut_magazine_chunk_free (_rig_simulator_object_id_magazine, object_id);
}

void
rig_simulator_init (RutShell *shell, void *user_data)
{
  RigSimulator *simulator = user_data;

  if (!_rig_simulator_object_id_magazine)
    {
      _rig_simulator_object_id_magazine =
        rut_magazine_new (sizeof (uint64_t), 1000);
    }

  simulator->id_map = g_hash_table_new_full (NULL, /* direct hash */
                                             NULL, /* direct key equal */
                                             NULL, /* key destroy */
                                             free_object_id);

  rig_simulator_start_service (simulator);

  simulator->engine = rig_engine_new_for_simulator (shell, simulator);
}

void
rig_simulator_fini (RutShell *shell, void *user_data)
{
  RigSimulator *simulator = user_data;
  RigEngine *engine = simulator->engine;

  rut_object_unref (engine);
  simulator->engine = NULL;

  g_hash_table_destroy (simulator->id_map);
  simulator->id_map = NULL;

  rig_simulator_stop_service (simulator);
}

static void
handle_update_ui_ack (const Rig__UpdateUIAck *result,
                      void *closure_data)
{
  g_print ("Simulator: UI Update ACK received\n");
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

uint64_t
get_object_id (RigSimulator *simulator, void *object)
{
  uint64_t *id_ptr = g_hash_table_lookup (simulator->id_map, object);

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
    return *id_ptr;
}

static void
stack_region_cb (uint8_t *data, size_t bytes, void *user_data)
{
  SerializeChangesState *state = user_data;
  RigSimulator *simulator = state->simulator;
  size_t step = sizeof (RutPropertyChange);
  size_t offset;
  int i;

  g_print ("Properties changed log %d:\n", bytes);

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
      pb_change->object_id = get_object_id (simulator, change->object);
      pb_change->has_property_id = true;
      pb_change->property_id = change->prop_id;
      rig_pb_property_value_init (state->serializer, pb_value, &change->boxed);

      g_print ("> %d: base = %p, offset = %d, obj id=%llu:%p:%s, prop id = %d\n",
               i,
               data,
               offset,
               (uint64_t)pb_change->object_id,
               change->object,
               rut_object_get_type_name (change->object),
               change->prop_id);
    }

  state->i = i;
}

static void
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

  /* Setup the property context to log all property changes so they
   * can be sent back to the frontend process each frame. */
  simulator->ctx->property_ctx.log = true;

  g_print ("Simulator: Start Frame\n");
  rut_shell_start_redraw (shell);

  rut_shell_update_timelines (shell);

  rut_shell_run_pre_paint_callbacks (shell);

  rut_shell_dispatch_input_events (shell);

  if (rut_shell_check_timelines (shell))
    rut_shell_queue_redraw (shell);

  g_print ("Simulator: Sending UI Update\n");

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
        rut_memory_stack_memalign (engine->serialization_stack,
                                   sizeof (Rig__PropertyChange) * n_changes,
                                   RUT_UTIL_ALIGNOF (Rig__PropertyChange));
      state.pb_values =
        rut_memory_stack_memalign (engine->serialization_stack,
                                   sizeof (Rig__PropertyValue) * n_changes,
                                   RUT_UTIL_ALIGNOF (Rig__PropertyValue));

      state.i = 0;
      state.n_changes = n_changes;

      rut_memory_stack_foreach_region (prop_ctx->change_log_stack,
                                       stack_region_cb,
                                       &state);

      ui_diff.property_changes =
        rut_memory_stack_memalign (engine->serialization_stack,
                                   sizeof (void *) * n_changes,
                                   RUT_UTIL_ALIGNOF (void *));

      for (i = 0; i < n_changes; i++)
        {
          ui_diff.property_changes[i] = &state.pb_changes[i];
          ui_diff.property_changes[i]->value = &state.pb_values[i];
        }
    }

  rig__frontend__update_ui (frontend_service,
                            &ui_diff,
                            handle_update_ui_ack,
                            NULL);

  rig_pb_serializer_destroy (serializer);

  rut_property_context_clear_log (prop_ctx);

  /* Stop logging property changes until the next frame. */
  simulator->ctx->property_ctx.log = false;

  rut_shell_run_post_paint_callbacks (shell);

  rut_shell_end_redraw (shell);
}

int
main (int argc, char **argv)
{
  RigSimulator simulator;
  const char *ipc_fd_str = getenv ("_RIG_IPC_FD");

#if 0
  GOptionContext *context = g_option_context_new (NULL);

  g_option_context_add_main_entries (context, rut_editor_entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_error ("Option parsing failed: %s\n", error->message);
      return EXIT_FAILURE;
    }
#endif

  if (!ipc_fd_str)
    {
      g_error ("Failed to find ipc file descriptor via _RIG_IPC_FD "
               "environment variable");
      return EXIT_FAILURE;
    }

  _rig_in_simulator_mode = true;

  memset (&simulator, 0, sizeof (RigSimulator));

  simulator.fd = strtol (ipc_fd_str, NULL, 10);

  simulator.shell = rut_shell_new (true, /* headless */
                                   rig_simulator_init,
                                   rig_simulator_fini,
                                   rig_simulator_run_frame,
                                   &simulator);

  simulator.ctx = rut_context_new (simulator.shell);

  rut_context_init (simulator.ctx);

  rut_shell_main (simulator.shell);

  rut_object_unref (simulator.ctx);
  rut_object_unref (simulator.shell);

  return 0;
}
