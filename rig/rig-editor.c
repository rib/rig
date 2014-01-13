#include "config.h"

#include <stdlib.h>
#include <glib.h>

#include <rut.h>
#include <rig-engine.h>
#include <rig-engine.h>
#include <rig-avahi.h>
#include <cogl-gst/cogl-gst.h>

static char **_rig_editor_remaining_args = NULL;

static const GOptionEntry _rig_editor_entries[] =
{
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY,
    &_rig_editor_remaining_args, "Project" },
  { 0 }
};

typedef struct _RigEditor
{
  RutShell *shell;
  RutContext *ctx;

  RigFrontend *frontend;
  RigEngine *engine;

  char *ui_filename;

} RigEditor;


void
rig_editor_init (RutShell *shell, void *user_data)
{
  RigEditor *editor = user_data;
  RigEngine *engine;

  editor->frontend = rig_frontend_new (shell,
                                       RIG_FRONTEND_ID_EDITOR,
                                       editor->ui_filename);

  engine = editor->frontend->engine;
  editor->engine = engine;

  /* TODO move into editor */
  rig_avahi_run_browser (engine);

  rut_shell_add_input_callback (editor->shell,
                                rig_engine_input_handler,
                                engine, NULL);
}

void
rig_editor_fini (RutShell *shell, void *user_data)
{
  /* NOP: (We currently free the engine when necessary in main()
   * due to the way we check for new files to open) */
}

static void
handle_run_frame_ack (const Rig__RunFrameAck *ack,
                      void *closure_data)
{
  RutShell *shell = closure_data;

  rut_shell_finish_frame (shell);

  g_print ("Editor: Run Frame ACK received\n");
}

static void
rig_editor_paint (RutShell *shell, void *user_data)
{
  RigEditor *editor = user_data;
  RigEngine *engine = editor->engine;
  RigFrontend *frontend = engine->frontend;
  ProtobufCService *simulator_service =
    rig_pb_rpc_client_get_service (frontend->frontend_peer->pb_rpc_client);
  RutInputQueue *input_queue = engine->simulator_input_queue;
  Rig__FrameSetup setup = RIG__FRAME_SETUP__INIT;
  RigPBSerializer *serializer;
  float x, y, z;

  rut_shell_start_redraw (shell);

  rut_shell_update_timelines (shell);

  /* XXX: These are a bit of a misnomer, since they happen before
   * input handling. Typical pre-paint callbacks are allocation
   * callbacks which we want run before painting and since we want
   * input to be consistent with what we paint we want to make sure
   * allocations are also up to date before input handling.
   */
  rut_shell_run_pre_paint_callbacks (shell);

  /* Again we are immediately about to start painting but this is
   * another set of callbacks that can hook into the start of
   * processing a frame with the different (compared to pre-paint
   * callbacks) that they aren't unregistered each frame and they
   * aren't sorted with respect to a node in a graph.
   */
  rut_shell_run_start_paint_callbacks (shell);

  rut_shell_dispatch_input_events (shell);

  serializer = rig_pb_serializer_new (engine);

  setup.n_events = input_queue->n_events;
  setup.events =
    rig_pb_serialize_input_events (serializer, input_queue);

  if (frontend->has_resized)
    {
      setup.has_view_width = true;
      setup.view_width = frontend->pending_width;
      setup.has_view_height = true;
      setup.view_height = frontend->pending_height;
      frontend->has_resized = false;
    }

  /* Inform the simulator of the offset position of the main camera
   * view so that it can transform its input events accordingly...
   */
  x = y = z = 0;
  rut_graphable_fully_transform_point (engine->main_camera_view,
                                       engine->camera_2d,
                                       &x, &y, &z);
  setup.has_view_x = true;
  setup.view_x = RUT_UTIL_NEARBYINT (x);

  setup.has_view_y = true;
  setup.view_y = RUT_UTIL_NEARBYINT (y);

  setup.has_play_mode = true;
  setup.play_mode = engine->play_mode;

  rig__simulator__run_frame (simulator_service,
                             &setup,
                             handle_run_frame_ack,
                             shell);

  rig_pb_serializer_destroy (serializer);

  rut_input_queue_clear (input_queue);

  rig_engine_paint (engine);

  rut_shell_run_post_paint_callbacks (shell);

  rut_shell_end_redraw (shell);

  if (rut_shell_check_timelines (shell))
    rut_shell_queue_redraw (shell);
}

int
main (int argc, char **argv)
{
  RigEditor editor;
  GOptionContext *context = g_option_context_new (NULL);
  GError *error = NULL;

  gst_init (&argc, &argv);

  g_option_context_add_main_entries (context, _rig_editor_entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      fprintf (stderr, "option parsing failed: %s\n", error->message);
      exit (EXIT_FAILURE);
    }

  if (_rig_editor_remaining_args == NULL ||
      _rig_editor_remaining_args[0] == NULL)
    {
      fprintf (stderr,
               "A filename argument for the UI description file is required. "
               "Pass a non-existing file to create it.\n");
      exit (EXIT_FAILURE);
    }

  memset (&editor, 0, sizeof (RigEditor));

  editor.ui_filename = g_strdup (_rig_editor_remaining_args[0]);

  editor.shell = rut_shell_new (false, /* not headless */
                                rig_editor_init,
                                rig_editor_fini,
                                rig_editor_paint,
                                &editor);

  editor.ctx = rut_context_new (editor.shell);

  rut_context_init (editor.ctx);

  _rig_in_editor_mode = true;

  /* XXX: This is a rather hacky way of handling opening new files due
   * to how bad our resource management used be that it was easier to
   * completely tear down the whole engine and start a-fresh. I think
   * now it wouldn't be too tricky to support opening new files
   * without using such a big hammer...
   */
  while (TRUE)
    {
      char *assets_location = g_path_get_dirname (editor.ui_filename);
      rut_set_assets_location (editor.ctx, assets_location);
      g_free (assets_location);

      rut_shell_main (editor.shell);

      if (editor.engine->next_ui_filename)
        {
          g_free (editor.ui_filename);
          editor.ui_filename = editor.engine->next_ui_filename;
          editor.engine->next_ui_filename = NULL;

          rut_object_unref (editor.frontend);
          editor.frontend = NULL;
          editor.engine = NULL;
        }
      else
        break;
    }

  rut_object_unref (editor.frontend);
  rut_object_unref (editor.ctx);
  rut_object_unref (editor.shell);

  return 0;
}
