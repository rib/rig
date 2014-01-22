#include <config.h>

#include <stdlib.h>
#include <glib.h>

#include <rut.h>
#include <rig-engine.h>
#include <rig-engine.h>
#include <rig-avahi.h>
#include <rig-rpc-network.h>
#include <cogl-gst/cogl-gst.h>

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


typedef struct _RigSlave
{
  RutShell *shell;
  RutContext *ctx;

  RigFrontend *frontend;
  RigEngine *engine;

} RigSlave;

static void
slave__test (Rig__Slave_Service *service,
             const Rig__Query *query,
             Rig__TestResult_Closure closure,
             void *closure_data)
{
  Rig__TestResult result = RIG__TEST_RESULT__INIT;
  //RigSlave *slave = rig_pb_rpc_closure_get_connection_data (closure_data);

  g_return_if_fail (query != NULL);

  g_print ("Test Query\n");

  closure (&result, closure_data);
}

static void
slave__load (Rig__Slave_Service *service,
             const Rig__UI *pb_ui,
             Rig__LoadResult_Closure closure,
             void *closure_data)
{
  Rig__LoadResult result = RIG__LOAD_RESULT__INIT;
  RigSlave *slave = rig_pb_rpc_closure_get_connection_data (closure_data);
  RigEngine *engine = slave->engine;
  float width, height;
  RigPBUnSerializer unserializer;
  RigUI *ui;

  g_return_if_fail (pb_ui != NULL);

  g_print ("UI Load Request\n");

  rig_pb_unserializer_init (&unserializer, engine,
                            true, /* with id-map */
                            true);  /* rewind memory stack */

  ui = rig_pb_unserialize_ui (&unserializer, pb_ui);

  rig_pb_unserializer_destroy (&unserializer);

  rig_engine_set_play_mode_ui (engine, ui);

  if (option_width > 0 && option_height > 0)
    {
      width = option_width;
      height = option_height;
    }
  else if (option_scale)
    {
      width = engine->device_width * option_scale;
      height = engine->device_height * option_scale;
    }
  else
    {
      width = engine->device_width / 2;
      height = engine->device_height / 2;
    }

  rig_engine_set_onscreen_size (engine, width, height);

  closure (&result, closure_data);
}


static Rig__Slave_Service rig_slave_service =
  RIG__SLAVE__INIT(slave__);

static void
client_close_handler (PB_RPC_ServerConnection *conn,
                      void *user_data)
{
  g_warning ("slave master disconnected %p", conn);
}

static void
new_client_handler (PB_RPC_Server *server,
                    PB_RPC_ServerConnection *conn,
                    void *user_data)
{
  RigSlave *slave = user_data;
  //RigEngine *engine = slave->engine;

  rig_pb_rpc_server_connection_set_close_handler (conn,
                                                  client_close_handler,
                                                  slave);

  rig_pb_rpc_server_connection_set_data (conn, slave);

  g_message ("slave master connected %p", conn);
}

static void
server_error_handler (PB_RPC_Error_Code code,
                      const char *message,
                      void *user_data)
{
  RigSlave *slave = user_data;
  RigEngine *engine = slave->engine;

  g_warning ("Server error: %s", message);

  rig_avahi_unregister_service (engine);

  rig_rpc_server_shutdown (engine->slave_service);

  rut_object_unref (engine->slave_service);
  engine->slave_service = NULL;
}

void
rig_slave_init (RutShell *shell, void *user_data)
{
  RigSlave *slave = user_data;
  RigEngine *engine;

  slave->frontend = rig_frontend_new (shell,
                                      RIG_FRONTEND_ID_SLAVE,
                                      NULL);

  engine = slave->frontend->engine;
  slave->engine = slave->engine;

  /* TODO: move from engine to frontend */

  engine->slave_service = rig_rpc_server_new (&rig_slave_service.base,
                                              server_error_handler,
                                              new_client_handler,
                                              slave);

  rig_avahi_register_service (engine);

  rut_shell_add_input_callback (slave->shell,
                                rig_engine_input_handler,
                                engine, NULL);
}

void
rig_slave_fini (RutShell *shell, void *user_data)
{
  RigSlave *slave = user_data;
  RigEngine *engine = slave->engine;

  /* TODO: move to frontend */
  rig_avahi_unregister_service (engine);
  rig_rpc_server_shutdown (engine->slave_service);

  slave->engine = NULL;

  rut_object_unref (slave->frontend);
  slave->frontend = NULL;
}

static void
rig_slave_paint (RutShell *shell, void *user_data)
{
  RigSlave *slave = user_data;
  RigEngine *engine = slave->engine;

  rut_shell_start_redraw (shell);

  rut_shell_update_timelines (shell);

  rut_shell_run_pre_paint_callbacks (shell);

  rut_shell_run_start_paint_callbacks (shell);

  rut_shell_dispatch_input_events (shell);

  rig_engine_paint (engine);

  rut_shell_run_post_paint_callbacks (shell);

  rut_shell_end_redraw (shell);

  if (rut_shell_check_timelines (shell))
    rut_shell_queue_redraw (shell);
}

#ifdef __ANDROID__

void
android_main (struct android_app *application)
{
  RigSlave slave;

  /* Make sure glue isn't stripped */
  app_dummy ();

  g_android_init ();

  memset (&slave, 0, sizeof (RigSlave));
  slave.engine = &engine;

  slave.app = application;

  slave.shell = rut_android_shell_new (application,
                                       rig_slave_init,
                                       rig_slave_fini,
                                       rig_slave_paint,
                                       &slave);

  slave.ctx = rut_context_new (engine.shell);
  gst_init (&argc, &argv);

  rut_context_init (slave.ctx);

  rut_shell_main (slave.shell);
}

#else

int
main (int argc, char **argv)
{
  RigSlave slave;
  GOptionContext *context = g_option_context_new (NULL);
  GError *error = NULL;

  gst_init (&argc, &argv);

  g_option_context_add_main_entries (context, rig_slave_entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      fprintf (stderr, "option parsing failed: %s\n", error->message);
      exit (EXIT_FAILURE);
    }

  memset (&slave, 0, sizeof (RigSlave));

  slave.shell = rut_shell_new (false, /* not headless */
                               rig_slave_init,
                               rig_slave_fini,
                               rig_slave_paint,
                               &slave);

  slave.ctx = rut_context_new (slave.shell);

  rut_context_init (slave.ctx);

  rut_shell_main (slave.shell);

  rut_object_unref (slave.ctx);
  rut_object_unref (slave.shell);

  return 0;
}

#endif
