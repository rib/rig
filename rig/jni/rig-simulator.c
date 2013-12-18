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
                                rig_engine_paint,
                                &engine);

  engine.ctx = rut_context_new (engine.shell);

  rut_context_init (engine.ctx);

  rut_shell_main (engine.shell);

  return 0;
}
