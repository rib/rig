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

  GHashTable *edit_id_to_play_object_map;
  GHashTable *play_object_to_edit_id_map;

  RutQueue *queued_deletes;

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

static RutMagazine *_rig_slave_object_id_magazine = NULL;

static void
free_object_id (void *id)
{
  rut_magazine_chunk_free (_rig_slave_object_id_magazine,, id);
}

static void *
lookup_object_cb (uint64_t id,
                  void *user_data)
{
  RigSlave *slave = user_data;
  return g_hash_table_lookup (slave->edit_id_to_play_object_map, &id);
}

static void *
lookup_object (RigSlave *slave, uint64_t id)
{
  return lookup_object_cb (id, slave);
}

static void
register_object_cb (void *play_mode_object,
                    uint64_t edit_mode_id,
                    void *user_data)
{
  RigSlave *slave = user_data;
  uint64_t *object_id;

  if (lookup_object (slave, edit_mode_id))
    {
      g_critical ("Tried to re-register object");
      return;
    }

  /* XXX: We need a mechanism for hooking into frontend edits that
   * happen as a result of UI logic so we can make sure to unregister
   * objects that might be deleted by UI logic. */

  object_id = rut_magazine_chunk_alloc (_rig_slave_object_id_magazine);
  *object_id = edit_mode_id;

  g_hash_table_insert (slave->edit_id_to_play_object_map, object_id, object);
  g_hash_table_insert (slave->play_object_to_edit_id_map, object, object_id);
}

static void
queue_delete_object_cb (uint64_t id, void *user_data)
{
  RigSlave *slave = user_data;
  void *object = (void *)(intptr_t)id;
  rut_queue_push_tail (slave->queued_deletes, object);
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

  if (slave->edit_id_to_play_object_map)
    {
      rig_engine_set_play_mode_ui (engine, NULL);

      g_hash_table_destroy (slave->edit_id_to_play_object_map);
      slave->edit_id_to_play_object_map = NULL;
      g_hash_table_destroy (slave->play_object_to_edit_id_map);
      slave->play_object_to_edit_id_map = NULL;
    }

  slave->edit_id_to_play_object_map =
    g_hash_table_new (g_int64_hash,
                      g_int64_equal, /* key equal */
                      free_object_id, /* key destroy */
                      NULL); /* value destroy */

  /* Note: we don't have a free function for the value because we
   * share object_ids between both hash-tables and need to be
   * careful not to double free them. */
  slave->play_object_to_edit_id_map =
    g_hash_table_new (NULL, /* direct hash */
                      NULL); /* direct key equal */

  rig_pb_unserializer_init (&unserializer, engine);

  rig_pb_unserializer_set_object_register_callback (&unserializer,
                                                    register_object_cb,
                                                    slave);

  rig_pb_unserializer_set_id_to_object_callback (&unserializer,
                                                 lookup_object_cb,
                                                 slave);

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

static void
slave__edit (Rig__Slave_Service *service,
             const Rig__UIEdit *pb_ui_edit,
             Rig__UIEditResult_Closure closure,
             void *closure_data)
{
  Rig__UIEditResult result = RIG__UI_EDIT_RESULT__INIT;
  RigSlave *slave = rig_pb_rpc_closure_get_connection_data (closure_data);
  RigEngine *engine = slave->engine;
  RutQueueItem *item;

  if (!rig_engine_apply_pb_ui_edit (engine,
                                    pb_ui_edit,
                                    register_object_cb,
                                    lookup_object_cb,
                                    queue_delete_object_cb
                                    slave))
    {
      result.has_status = true;
      result.status = false;
    }
  /* XXX: we need to synchonize with the simulator, and then forward
   * edits to the simulator too. */
#error todo: forward edits to simulator too

  /* Lazily delete objects... */
  rut_list_for_each (item, &simulator->queued_deletes->items, list_node)
    {
      uint64_t *object_id =
        g_hash_table_lookup (slave->play_object_to_edit_id_map, item->data);

      g_hash_table_remove (slave->edit_id_to_play_object_map, object_id);
      g_hash_table_remove (slave->play_object_to_edit_id_map, item->data);

      rut_object_unref (item->data);
    }
  rut_queue_clear (simulator->queued_deletes);

  rut_shell_queue_redraw (engine->shell);

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

  slave->queued_deletes = rut_queue_new ();

  slave->frontend = rig_frontend_new (shell,
                                      RIG_FRONTEND_ID_SLAVE,
                                      NULL);

  engine = slave->frontend->engine;
  slave->engine = engine;

  _rig_slave_object_id_magazine = engine->object_id_magazine;

  /* TODO: move from engine to slave */
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

  _rig_simulator_object_id_magazine = NULL;

  slave->engine = NULL;

  rut_object_unref (slave->frontend);
  slave->frontend = NULL;

  rut_queue_free (slave->queued_deletes);
  slave->queued_deletes = NULL;
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

  rut_memory_stack_rewind (engine->frame_stack);

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
