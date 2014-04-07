/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013,2014  Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#include <config.h>

#include <stdlib.h>
#include <glib.h>

#ifdef USE_GSTREAMER
#include <cogl-gst/cogl-gst.h>
#endif

#include <rut.h>

#include "rig-slave.h"
#include "rig-engine.h"
#include "rig-avahi.h"
#include "rig-rpc-network.h"
#include "rig-pb.h"

#include "rig.pb-c.h"

static int option_width;
static int option_height;
static double option_scale;

static const GOptionEntry rig_slave_entries[] =
{
  {
    "width", 'w', 0, G_OPTION_ARG_INT, &option_width,
    "Width of slave window", NULL
  },
  {
    "height", 'h', 0, G_OPTION_ARG_INT, &option_width,
    "Height of slave window", NULL
  },
  {
    "scale", 's', 0, G_OPTION_ARG_DOUBLE, &option_scale,
    "Scale factor for slave window based on default device dimensions", NULL
  },

  { 0 }
};


int
main (int argc, char **argv)
{
  RigSlave *slave;
  GOptionContext *context = g_option_context_new (NULL);
  GError *error = NULL;

  rut_init_tls_state ();

#ifdef USE_GSTREAMER
  gst_init (&argc, &argv);
#endif

  g_option_context_add_main_entries (context, rig_slave_entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      fprintf (stderr, "option parsing failed: %s\n", error->message);
      exit (EXIT_FAILURE);
    }

  slave = rig_slave_new (option_width,
                         option_height,
                         option_scale);

  rig_slave_run (slave);

  rut_object_unref (slave);

  return 0;
}
