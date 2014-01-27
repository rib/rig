/*
 * Rig
 *
 * Copyright (C) 2013,2014  Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <glib.h>

#include <rut.h>

#include "rig-simulator.h"


int
main (int argc, char **argv)
{
  RigSimulator simulator;
  const char *ipc_fd_str = getenv ("_RIG_IPC_FD");
  const char *frontend = getenv ("_RIG_FRONTEND");

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

  if (!frontend)
    {
      g_error ("Failed to determine frontend via _RIG_FRONTEND "
               "environment variable");
      return EXIT_FAILURE;
    }

  _rig_in_simulator_mode = true;

  memset (&simulator, 0, sizeof (RigSimulator));

  if (strcmp (frontend, "editor") == 0)
    {
      simulator.frontend_id = RIG_FRONTEND_ID_EDITOR;
      simulator.editable = true;
    }
  else if (strcmp (frontend, "slave") == 0)
    {
      simulator.frontend_id = RIG_FRONTEND_ID_SLAVE;
      simulator.editable = true;
    }
  else if (strcmp (frontend, "device") == 0)
    simulator.frontend_id = RIG_FRONTEND_ID_DEVICE;
  else
    {
      g_error ("Spurious _RIG_FRONTEND environment variable value");
      return EXIT_FAILURE;
    }

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
