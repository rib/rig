#include "config.h"

#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <glib.h>

#include <rut.h>
#include <rig-engine.h>
#include <rig-engine.h>

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

static void
rig_simulator_run_frame (RutShell *shell, void *user_data)
{
  RigSimulator *simulator = user_data;
  RigEngine *engine = simulator->engine;
  ProtobufCService *frontend_service =
    rig_pb_rpc_client_get_service (simulator->simulator_peer->pb_rpc_client);
  Rig__UIDiff ui_diff;

  g_print ("Simulator: Start Frame\n");
  rut_shell_start_redraw (shell);

  rut_shell_update_timelines (shell);

  rut_shell_run_pre_paint_callbacks (shell);

  rut_shell_dispatch_input_events (shell);

  if (rut_shell_check_timelines (shell))
    rut_shell_queue_redraw (shell);

  g_print ("Simulator: Sending UI Update\n");

  rig__uidiff__init (&ui_diff);
  rig__frontend__update_ui (frontend_service,
                            &ui_diff,
                            handle_update_ui_ack,
                            NULL);
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

  rut_shell_main (simulator.shell);

  rut_refable_unref (simulator.ctx);
  rut_refable_unref (simulator.shell);

  return 0;
}
