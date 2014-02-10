#include <config.h>

#include <stdlib.h>

#include <glib.h>

#include <rut.h>
#include <cogl-gst/cogl-gst.h>

#include "rig-frontend.h"
#include "rig-engine.h"
#include "rig-pb.h"
#include "rig.pb-c.h"

static char **_rig_device_remaining_args = NULL;

typedef struct _RigDevice
{
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

void
rig_device_init (RutShell *shell, void *user_data)
{
  RigDevice *device = user_data;

  device->frontend = rig_frontend_new (shell,
                                       RIG_FRONTEND_ID_DEVICE,
                                       device->ui_filename);
  device->engine = device->frontend->engine;

  rut_shell_add_input_callback (device->shell,
                                rig_engine_input_handler,
                                device->engine, NULL);
}

void
rig_device_fini (RutShell *shell, void *user_data)
{
  RigDevice *device = user_data;
  RigEngine *engine = device->engine;

  rut_object_unref (engine);
  device->engine = NULL;
}

static void
rig_device_paint (RutShell *shell, void *user_data)
{
  RigDevice *device = user_data;
  RigEngine *engine = device->engine;
  RigFrontend *frontend = engine->frontend;
  ProtobufCService *simulator_service =
    rig_pb_rpc_client_get_service (frontend->frontend_peer->pb_rpc_client);

  rut_shell_start_redraw (shell);

  rut_shell_update_timelines (shell);

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

      rig_frontend_run_simulator_frame (frontend, &setup);

      rig_pb_serializer_destroy (serializer);

      rut_input_queue_clear (input_queue);
    }

  rut_shell_run_pre_paint_callbacks (shell);

  rut_shell_run_start_paint_callbacks (shell);

  rig_engine_paint (engine);

  rut_shell_run_post_paint_callbacks (shell);

  /* XXX: If the simulator is running slowly then it's possible that
   * some state needs to be maintained for longer than one frontend
   * frame, so make sure we aren't going to trash state we need to
   * send to the simulator here... */
#error "XXX: do we ever queue up input events/stuff destined for the sim using the frame stack?"
  rut_memory_stack_rewind (engine->frame_stack);

  rut_shell_end_redraw (shell);

  /* FIXME: we should hook into an asynchronous notification of
   * when rendering has finished for determining when a frame is
   * finished. */
  rut_shell_finish_frame (shell);

  if (rut_shell_check_timelines (shell))
    rut_shell_queue_redraw (shell);
}

int
main (int argc, char **argv)
{
  GOptionContext *context = g_option_context_new (NULL);
  RigDevice device;
  GError *error = NULL;
  char *assets_location;

  gst_init (&argc, &argv);

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

  memset (&device, 0, sizeof (RigDevice));

  device.ui_filename = g_strdup (_rig_device_remaining_args[0]);

  device.shell = rut_shell_new (false, /* not headless */
                                rig_device_init,
                                rig_device_fini,
                                rig_device_paint,
                                &device);

  device.ctx = rut_context_new (device.shell);

  rut_context_init (device.ctx);

  assets_location = g_path_get_dirname (device.ui_filename);
  rut_set_assets_location (device.ctx, assets_location);

  rut_shell_main (device.shell);

  rut_object_unref (device.ctx);
  rut_object_unref (device.shell);

  return 0;
}
