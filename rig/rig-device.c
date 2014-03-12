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

#include <rut.h>
#ifdef USE_GSTREAMER
#include <cogl-gst/cogl-gst.h>
#endif

#include "rig-frontend.h"
#include "rig-engine.h"
#include "rig-pb.h"
#include "rig.pb-c.h"

static char **_rig_device_remaining_args = NULL;

typedef struct _RigDevice
{
  RutObjectBase _base;

  RutShell *shell;
  RutContext *ctx;
  RigFrontend *frontend;
  RigEngine *engine;

  char *ui_filename;

} RigDevice;

static const GOptionEntry _rig_device_entries[] =
{
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY,
    &_rig_device_remaining_args, "Project" },
  { 0 }
};

static void
rig_device_redraw (RutShell *shell, void *user_data)
{
  RigDevice *device = user_data;
  RigEngine *engine = device->engine;
  RigFrontend *frontend = engine->frontend;

  rut_shell_start_redraw (shell);

  /* XXX: we only kick off a new frame in the simulator if it's not
   * still busy... */
  if (!frontend->ui_update_pending)
    {
      RutInputQueue *input_queue = rut_shell_get_input_queue (shell);
      Rig__FrameSetup setup = RIG__FRAME_SETUP__INIT;
      RigPBSerializer *serializer;

      serializer = rig_pb_serializer_new (engine);

      setup.has_play_mode = true;
      setup.play_mode = engine->play_mode;

      setup.n_events = input_queue->n_events;
      setup.events =
        rig_pb_serialize_input_events (serializer, input_queue);

      if (frontend->has_resized)
        {
          setup.has_view_width = true;
          setup.view_width = engine->window_width;
          setup.has_view_height = true;
          setup.view_height = engine->window_height;
          frontend->has_resized = false;
        }

      setup.edit = NULL;

      rig_frontend_run_simulator_frame (frontend, serializer, &setup);

      rig_pb_serializer_destroy (serializer);

      rut_input_queue_clear (input_queue);

      rut_memory_stack_rewind (engine->sim_frame_stack);
    }

  rut_shell_update_timelines (shell);

  rut_shell_run_pre_paint_callbacks (shell);

  rut_shell_run_start_paint_callbacks (shell);

  rig_engine_paint (engine);

  rig_engine_garbage_collect (engine,
                              NULL, /* callback */
                              NULL); /* user_data */

  rut_shell_run_post_paint_callbacks (shell);

  rut_memory_stack_rewind (engine->frame_stack);

  rut_shell_end_redraw (shell);

  /* FIXME: we should hook into an asynchronous notification of
   * when rendering has finished for determining when a frame is
   * finished. */
  rut_shell_finish_frame (shell);

  if (rut_shell_check_timelines (shell))
    rut_shell_queue_redraw (shell);
}

static void
simulator_connected_cb (void *user_data)
{
  RigDevice *device = user_data;
  RigEngine *engine = device->engine;

  rig_frontend_reload_simulator_ui (device->frontend,
                                    engine->play_mode_ui,
                                    true); /* play mode ui */
}

static void
_rig_device_free (void *object)
{
  RigDevice *device = object;

  rut_object_unref (device->engine);

  rut_object_unref (device->ctx);
  rut_object_unref (device->shell);

  rut_object_free (RigDevice, device);
}

static RutType rig_device_type;

static void
_rig_device_init_type (void)
{
  rut_type_init (&rig_device_type, "RigDevice", _rig_device_free);
}

static RigDevice *
rig_device_new (const char *filename)
{
  RigDevice *device = rut_object_alloc0 (RigDevice,
                                         &rig_device_type,
                                         _rig_device_init_type);
  RigEngine *engine;
  char *assets_location;

  device->ui_filename = g_strdup (filename);

  device->shell = rut_shell_new (false, /* not headless */
                                 NULL, /* no init func */
                                 NULL, /* no fini func */
                                 rig_device_redraw,
                                 device);

  device->ctx = rut_context_new (device->shell);

  rut_context_init (device->ctx);

  assets_location = g_path_get_dirname (device->ui_filename);
  rut_set_assets_location (device->ctx, assets_location);
  g_free (assets_location);

  device->frontend = rig_frontend_new (device->shell,
                                       RIG_FRONTEND_ID_DEVICE,
                                       device->ui_filename,
                                       true); /* start in play mode */

  engine = device->frontend->engine;
  device->engine = engine;

  rig_frontend_set_simulator_connected_callback (device->frontend,
                                                 simulator_connected_cb,
                                                 device);

  rut_shell_add_input_callback (device->shell,
                                rig_engine_input_handler,
                                device->engine, NULL);

  return device;
}

int
main (int argc, char **argv)
{
  GOptionContext *context = g_option_context_new (NULL);
  RigDevice *device;
  GError *error = NULL;

#ifdef USE_GSTREAMER
  gst_init (&argc, &argv);
#endif

  g_option_context_add_main_entries (context, _rig_device_entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_error ("Option parsing failed: %s\n", error->message);
      return EXIT_FAILURE;
    }

  if (_rig_device_remaining_args == NULL ||
      _rig_device_remaining_args[0] == NULL)
    {
      g_error ("A filename argument for the UI description file is "
               "required. Pass a non-existing file to create it.\n");
      return EXIT_FAILURE;
    }

  device = rig_device_new (_rig_device_remaining_args[0]);

  rut_shell_main (device->shell);

  rut_object_unref (device);

  return 0;
}
