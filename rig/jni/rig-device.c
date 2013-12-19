#include <config.h>

#include <stdlib.h>

#include <glib.h>

#include <rut.h>
#include <rig-engine.h>
#include <cogl-gst/cogl-gst.h>

static char **_rig_editor_remaining_args = NULL;

static const GOptionEntry rut_editor_entries[] =
{
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY,
    &_rig_editor_remaining_args, "Project" },
  { 0 }
};

void
rig_device_init (RutShell *shell, void *user_data)
{
  RigEngine *engine = user_data;

  rig_engine_init (engine, shell);
}

static void
rig_device_paint (RutShell *shell, void *user_data)
{
  RigEngine *engine = user_data;

  rut_shell_start_redraw (shell);

  rut_shell_update_timelines (shell);

  rut_shell_run_pre_paint_callbacks (shell);

  rig_engine_paint (engine);

  if (rut_shell_check_timelines (shell))
    rut_shell_queue_redraw (shell);
}

int
main (int argc, char **argv)
{
  GOptionContext *context = g_option_context_new (NULL);
  RigEngine engine;
  GError *error = NULL;
  char *assets_location;

  memset (&engine, 0, sizeof (RigEngine));

  gst_init (&argc, &argv);

  g_option_context_add_main_entries (context, rut_editor_entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_error ("Option parsing failed: %s\n", error->message);
      return EXIT_FAILURE;
    }

  if (_rig_editor_remaining_args == NULL ||
      _rig_editor_remaining_args[0] == NULL)
    {
      g_error ("A filename argument for the UI description file is "
               "required. Pass a non-existing file to create it.\n");
      return EXIT_FAILURE;
    }

  engine.ui_filename = g_strdup (_rig_editor_remaining_args[0]);

  engine.shell = rut_shell_new (false, /* not headless */
                                rig_device_init,
                                rig_engine_fini,
                                rig_device_paint,
                                &engine);

  engine.ctx = rut_context_new (engine.shell);

  rut_context_init (engine.ctx);

  rut_shell_add_input_callback (engine.shell,
                                rig_engine_input_handler,
                                &engine, NULL);

  assets_location = g_path_get_dirname (engine.ui_filename);
  rut_set_assets_location (engine.ctx, assets_location);

  rut_shell_main (engine.shell);

  return 0;
}
