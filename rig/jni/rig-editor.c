#include "config.h"

#include <stdlib.h>
#include <glib.h>

#include <rut.h>
#include <rig-engine.h>
#include <rig-engine.h>
#include <rig-avahi.h>
#include <cogl-gst/cogl-gst.h>

static char **_rig_editor_remaining_args = NULL;

static const GOptionEntry rut_editor_entries[] =
{
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY,
    &_rig_editor_remaining_args, "Project" },
  { 0 }
};

void
rig_editor_init (RutShell *shell, void *user_data)
{
  RigEngine *engine = user_data;

  rig_avahi_run_browser (engine);

  rig_engine_init (engine, shell);
}

static void
rig_editor_paint (RutShell *shell, void *user_data)
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
  RigEngine engine;
  GOptionContext *context = g_option_context_new (NULL);
  GError *error = NULL;

  gst_init (&argc, &argv);

  g_option_context_add_main_entries (context, rut_editor_entries, NULL);

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

  memset (&engine, 0, sizeof (RigEngine));

  engine.ui_filename = g_strdup (_rig_editor_remaining_args[0]);

  engine.shell = rut_shell_new (false, /* not headless */
                                rig_editor_init,
                                rig_engine_fini,
                                rig_editor_paint,
                                &engine);

  engine.ctx = rut_context_new (engine.shell);

  rut_context_init (engine.ctx);

  rut_shell_add_input_callback (engine.shell,
                                rig_engine_input_handler,
                                &engine, NULL);

  _rig_in_editor_mode = true;

  while (TRUE)
    {
      char *assets_location = g_path_get_dirname (engine.ui_filename);
      rut_set_assets_location (engine.ctx, assets_location);
      g_free (assets_location);

      engine.selected_controller = NULL;

      rut_shell_main (engine.shell);

      if (engine.next_ui_filename)
        {
          g_free (engine.ui_filename);
          engine.ui_filename = engine.next_ui_filename;
          engine.next_ui_filename = NULL;
        }
      else
        break;
    }

  return 0;
}
