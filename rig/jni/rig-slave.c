#include "config.h"

#include <glib.h>
#include <rut.h>
#include <rig-data.h>
#include <rig-engine.h>
#include <rig-avahi.h>
#include <rig-rpc-network.h>

#if 0
static const GOptionEntry rut_handset_entries[] =
{
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY,
    &_rig_preview_remaining_args, "Project" },
  { 0 }
};
#endif

void
rig_preview_init (RutShell *shell, void *user_data)
{
  RigData *data = user_data;

  rig_start_rpc_server (data, 52637);

  rig_engine_init (shell, data);
}

#ifdef __ANDROID__

void
android_main (struct android_app *application)
{
  RigData data;

  /* Make sure glue isn't stripped */
  app_dummy ();

  g_android_init ();

  memset (&data, 0, sizeof (RigData));
  data.app = application;

  data.shell = rut_android_shell_new (application,
                                      rig_preview_init,
                                      rig_engine_fini,
                                      rig_engine_paint,
                                      &data);

  data.ctx = rut_context_new (data.shell);

  rut_context_init (data.ctx);

  rut_shell_set_input_callback (data.shell,
                                rig_engine_input_handler,
                                &data);

  _rig_in_device_mode = TRUE;

  rut_shell_main (data.shell);
}

#else

int
main (int argc, char **argv)
{
  RigData data;
  GOptionContext *context = g_option_context_new (NULL);
  GError *error = NULL;

  //g_option_context_add_main_entries (context, rut_handset_entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      fprintf (stderr, "option parsing failed: %s\n", error->message);
      exit (EXIT_FAILURE);
    }

  memset (&data, 0, sizeof (RigData));

  data.shell = rut_shell_new (rig_preview_init,
                              rig_engine_fini,
                              rig_engine_paint,
                              &data);

  data.ctx = rut_context_new (data.shell);

  rut_context_init (data.ctx);

  rut_shell_add_input_callback (data.shell,
                                rig_engine_input_handler,
                                &data, NULL);

  _rig_in_device_mode = TRUE;

  rut_shell_main (data.shell);

  return 0;
}

#endif
