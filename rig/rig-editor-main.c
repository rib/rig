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
#include <glib.h>

#include <rut.h>
#include <cogl-gst/cogl-gst.h>

#include "rig-editor.h"

static char **_rig_editor_remaining_args = NULL;

static const GOptionEntry _rig_editor_entries[] =
{
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY,
    &_rig_editor_remaining_args, "Project" },
  { 0 }
};

int
main (int argc, char **argv)
{
  GOptionContext *context = g_option_context_new (NULL);
  RigEditor *editor;
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

  editor = rig_editor_new (_rig_editor_remaining_args[0]);

  rig_editor_run (editor);

  rut_object_unref (editor);

  return 0;
}
