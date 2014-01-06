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

void
rig_simulator_init (RutShell *shell, void *user_data)
{
  RigSimulator *simulator = user_data;
  simulator->engine = rig_engine_new_for_simulator (shell, simulator);
}

void
rig_simulator_fini (RutShell *shell, void *user_data)
{
  RigSimulator *simulator= user_data;
  RigEngine *engine = simulator->engine;

  rut_refable_unref (engine);
  simulator->engine = NULL;
}

static void
handle_update_ui_ack (const Rig__UpdateUIAck *result,
                      void *closure_data)
{
  g_print ("Simulator: UI Update ACK received\n");
}

static size_t
align (size_t base, int alignment)
{
  return (base + alignment - 1) & ~(alignment - 1);
}

typedef struct _SerializeChangesState
{
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
      pb_change->object_id = (uint64_t)change->object;
      pb_change->has_property_id = true;
      pb_change->property_id = change->prop_id;
      rig_pb_property_value_init (state->serializer, pb_value, &change->boxed);

      g_print ("> %d: base = %p, offset = %d, obj = %p(%s), prop id = %d\n",
               i,
               data,
               offset,
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

      state.serializer = serializer;

      state.pb_changes =
        rut_memory_stack_alloc (engine->serialization_stack,
                                sizeof (Rig__PropertyChange) * n_changes,
                                RUT_UTIL_ALIGNOF (Rig__PropertyChange));
      state.pb_values =
        rut_memory_stack_alloc (engine->serialization_stack,
                                sizeof (Rig__PropertyValue) * n_changes,
                                RUT_UTIL_ALIGNOF (Rig__PropertyValue));

      state.i = 0;
      state.n_changes = n_changes;

      rut_memory_stack_foreach_region (prop_ctx->change_log_stack,
                                       stack_region_cb,
                                       &state);

      ui_diff.property_changes =
        rut_memory_stack_alloc (engine->serialization_stack,
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
}

int
main (int argc, char **argv)
{
  RigSimulator simulator;

#if 0
  GOptionContext *context = g_option_context_new (NULL);

  g_option_context_add_main_entries (context, rut_editor_entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_error ("Option parsing failed: %s\n", error->message);
      return EXIT_FAILURE;
    }
#endif

  memset (&simulator, 0, sizeof (RigSimulator));

  _rig_in_simulator_mode = true;

  simulator.shell = rut_shell_new (true, /* headless */
                                   rig_simulator_init,
                                   rig_simulator_fini,
                                   rig_simulator_run_frame,
                                   &simulator);

  simulator.ctx = rut_context_new (simulator.shell);

  rut_context_init (simulator.ctx);

  /* Setup the property context to log all property changes so they
   * can be sent back to the frontend process each frame. */
  simulator.ctx->property_ctx.log = true;

  rut_shell_main (simulator.shell);

  rut_refable_unref (simulator.ctx);
  rut_refable_unref (simulator.shell);

  return 0;
}
