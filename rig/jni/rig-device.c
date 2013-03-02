#include "config.h"

#include <glib.h>
#include <rut.h>
#include <rig-data.h>
#include <rig-engine.h>
#include <rig-avahi.h>

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
  RigData *data = user_data;

  rig_engine_init (shell, data);
}

int
main (int argc, char **argv)
{
  RigData data;
  GOptionContext *context = g_option_context_new (NULL);
  GError *error = NULL;
  char *assets_location;

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

  memset (&data, 0, sizeof (RigData));

  data.ui_filename = g_strdup (_rig_editor_remaining_args[0]);

  _rig_in_device_mode = TRUE;

  data.shell = rut_shell_new (rig_editor_init,
                              rig_engine_fini,
                              rig_engine_paint,
                              &data);

  data.ctx = rut_context_new (data.shell);

  rut_context_init (data.ctx);

  rut_shell_add_input_callback (data.shell,
                                rig_engine_input_handler,
                                &data, NULL);

  assets_location = g_path_get_dirname (data.ui_filename);
  rut_set_assets_location (data.ctx, assets_location);

  rut_shell_main (data.shell);

  return 0;
}
