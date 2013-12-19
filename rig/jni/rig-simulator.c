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
  RigEngine *engine = user_data;

  rig_engine_init (engine, shell);
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
  RigEngine *engine = user_data;
  ProtobufCService *service =
    rig_pb_rpc_client_get_service (engine->simulator_peer->pb_rpc_client);
  Rig__UIDiff ui_diff;

  rut_shell_start_redraw (shell);

  rut_shell_update_timelines (shell);

  rut_shell_run_pre_paint_callbacks (shell);

  /* TODO: handle input */

  if (rut_shell_check_timelines (shell))
    rut_shell_queue_redraw (shell);

  rig__uidiff__init (&ui_diff);
  rig__renderer__update_ui (service,
                            &ui_diff,
                            handle_update_ui_ack,
                            NULL);
}

int
main (int argc, char **argv)
{
  RigEngine engine;
#if 0
  GOptionContext *context = g_option_context_new (NULL);

  g_option_context_add_main_entries (context, rut_editor_entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_error ("Option parsing failed: %s\n", error->message);
      return EXIT_FAILURE;
    }
#endif

  memset (&engine, 0, sizeof (RigEngine));

  _rig_in_simulator_mode = true;

  engine.shell = rut_shell_new (true, /* headless */
                                rig_simulator_init,
                                rig_engine_fini,
                                rig_simulator_run_frame,
                                &engine);

  engine.ctx = rut_context_new (engine.shell);

  rut_context_init (engine.ctx);

  rut_shell_main (engine.shell);

  return 0;
}
