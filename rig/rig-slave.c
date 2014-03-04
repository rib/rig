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

  RigPBUnSerializer *ui_unserializer;

  RigEngineOpMapContext map_op_ctx;
  RigEngineOpApplyContext apply_op_ctx;

  RutQueue *edits_for_sim;
  RutClosure *ui_update_closure;
  RutQueue *pending_edits;

  RutClosure *ui_load_closure;
  const Rig__UI *pending_ui_load;
  Rig__LoadResult_Closure pending_ui_load_closure;
  void *pending_ui_load_closure_data;

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
  rut_magazine_chunk_free (_rig_slave_object_id_magazine, id);
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
register_object_cb (void *object,
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
load_ui (RigSlave *slave)
{
  Rig__LoadResult result = RIG__LOAD_RESULT__INIT;
  RigEngine *engine = slave->engine;
  float width, height;
  RigPBUnSerializer *unserializer;
  const Rig__UI *pb_ui = slave->pending_ui_load;
  RigUI *ui;

  g_return_if_fail (pb_ui != NULL);

  if (slave->edit_id_to_play_object_map)
    {
      rig_engine_set_play_mode_ui (engine, NULL);

      g_hash_table_destroy (slave->edit_id_to_play_object_map);
      slave->edit_id_to_play_object_map = NULL;
      g_hash_table_destroy (slave->play_object_to_edit_id_map);
      slave->play_object_to_edit_id_map = NULL;
    }

  slave->edit_id_to_play_object_map =
    g_hash_table_new_full (g_int64_hash,
                           g_int64_equal, /* key equal */
                           free_object_id, /* key destroy */
                           NULL); /* value destroy */

  /* Note: we don't have a free function for the value because we
   * share object_ids between both hash-tables and need to be
   * careful not to double free them. */
  slave->play_object_to_edit_id_map =
    g_hash_table_new (NULL, /* direct hash */
                      NULL); /* direct key equal */

  unserializer = rig_pb_unserializer_new (engine);

  rig_pb_unserializer_set_object_register_callback (unserializer,
                                                    register_object_cb,
                                                    slave);

  rig_pb_unserializer_set_id_to_object_callback (unserializer,
                                                 lookup_object_cb,
                                                 slave);

  ui = rig_pb_unserialize_ui (unserializer, pb_ui);

  rig_pb_unserializer_destroy (unserializer);

  rig_engine_set_play_mode_ui (engine, ui);

  rig_frontend_forward_simulator_ui (slave->frontend, pb_ui,
                                     true); /* play mode */

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

  rig_engine_op_apply_context_set_ui (&slave->apply_op_ctx, ui);

  slave->pending_ui_load_closure (&result, slave->pending_ui_load_closure_data);

  slave->pending_ui_load = NULL;
  slave->pending_ui_load_closure = NULL;
  slave->pending_ui_load_closure_data = NULL;
}

/* When this is called we know that the frontend is in sync with the
 * simulator which has just sent the frontend a ui-update that has
 * been applied and so we can apply edits without fear of conflicting
 * with the simulator...
 */
static void
ui_load_cb (RigFrontend *frontend, void *user_data)
{
  RigSlave *slave = user_data;

  rut_closure_disconnect (slave->ui_load_closure);
  slave->ui_load_closure = NULL;
  load_ui (slave);
}

typedef struct _PendingEdit
{
  RigSlave *slave;
  RutClosure *ui_update_closure;

  Rig__UIEdit *edit;
  Rig__UIEditResult_Closure closure;
  void *closure_data;

  bool status;
} PendingEdit;


static void
slave__load (Rig__Slave_Service *service,
             const Rig__UI *pb_ui,
             Rig__LoadResult_Closure closure,
             void *closure_data)
{
  RigSlave *slave = rig_pb_rpc_closure_get_connection_data (closure_data);
  RigFrontend *frontend = slave->frontend;
  RutQueueItem *item;

  g_print ("Slave: UI Load Request\n");

  /* Discard any previous pending ui load, since it's now redundant */
  if (slave->pending_ui_load)
    {
      Rig__LoadResult result = RIG__LOAD_RESULT__INIT;
      slave->pending_ui_load_closure (&result,
                                      slave->pending_ui_load_closure_data);
    }

  slave->pending_ui_load = pb_ui;
  slave->pending_ui_load_closure = closure;
  slave->pending_ui_load_closure_data = closure_data;

  /* Discard any pending edit, since it's now redundant... */
  rut_list_for_each (item, &slave->pending_edits->items, list_node)
    {
      Rig__UIEditResult result = RIG__UIEDIT_RESULT__INIT;
      PendingEdit *pending_edit = item->data;

      pending_edit->closure (&result, pending_edit->closure_data);

      g_slice_free (PendingEdit, pending_edit);
    }

  /* XXX: If the simulator is busy we need to synchonize with it
   * before applying any edits... */
  if (!frontend->ui_update_pending)
    load_ui (slave);
  else
    {
      slave->ui_load_closure =
        rig_frontend_add_ui_update_callback (frontend,
                                             ui_load_cb,
                                             slave,
                                             NULL); /* destroy */
    }
}

static void
apply_pending_edits (RigSlave *slave)
{
  RutQueueItem *item;

  rut_list_for_each (item, &slave->pending_edits->items, list_node)
    {
      PendingEdit *pending_edit = item->data;

      /* Note: Since a slave device is effectively always running in
       * play-mode the state of the UI is unpredictable and it's
       * always possible that edits made in an editor can no longer be
       * applied to the current state of a slave device (for example
       * an object being edited may have been deleted by some UI
       * logic)
       *
       * We apply edits on a best-effort basis, and if they fail we
       * report that status back to the editor so that it can inform
       * the user who can choose to reset the slave.
       */
      if (!rig_engine_map_pb_ui_edit (&slave->map_op_ctx,
                                      &slave->apply_op_ctx,
                                      pending_edit->edit))
        {
          pending_edit->status = false;
        }

      /* The next step is to forward the edits to the simulator the
       * next time we enter rig_slave_paint() and setup the next
       * frame for the simulator.
       *
       * XXX: the pending_edit->closure() will be called in
       * rig_slave_paint().
       */
      rut_queue_push_tail (slave->edits_for_sim, pending_edit);
    }
  rut_queue_clear (slave->pending_edits);

  /* Ensure we will forward edits to the simulator in
   * rig_slave_paint()... */
  rut_shell_queue_redraw (slave->engine->shell);
}

/* When this is called we know that the frontend is in sync with the
 * simulator which has just sent the frontend a ui-update that has
 * been applied and so we can apply edits without fear of conflicting
 * with the simulator...
 */
static void
ui_updated_cb (RigFrontend *frontend, void *user_data)
{
  RigSlave *slave = user_data;

  rut_closure_disconnect (slave->ui_update_closure);
  slave->ui_update_closure = NULL;

  apply_pending_edits (slave);
}

static void
slave__edit (Rig__Slave_Service *service,
             const Rig__UIEdit *pb_ui_edit,
             Rig__UIEditResult_Closure closure,
             void *closure_data)
{
  RigSlave *slave = rig_pb_rpc_closure_get_connection_data (closure_data);
  PendingEdit *pending_edit = g_slice_new0 (PendingEdit);
  RigFrontend *frontend = slave->frontend;

  g_print ("Slave: UI Edit Request\n");

  pending_edit->slave = slave;
  pending_edit->edit = (Rig__UIEdit *)pb_ui_edit;
  pending_edit->status = true;
  pending_edit->closure = closure;
  pending_edit->closure_data = closure_data;

  rut_queue_push_tail (slave->pending_edits, pending_edit);

  /* XXX: If the simulator is busy we need to synchonize with it
   * before applying any edits... */
  if (!frontend->ui_update_pending)
    apply_pending_edits (slave);
  else
    {
      if (!slave->ui_update_closure)
        {
          slave->ui_update_closure =
            rig_frontend_add_ui_update_callback (frontend,
                                                 ui_updated_cb,
                                                 slave,
                                                 NULL); /* destroy */
        }
    }
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

static uint64_t
map_edit_id_to_play_object_cb (uint64_t edit_id,
                               void *user_data)
{
  void *play_object = lookup_object (user_data, edit_id);
  return (uint64_t)(uintptr_t)play_object;
}

void
rig_slave_init (RutShell *shell, void *user_data)
{
  RigSlave *slave = user_data;
  RigEngine *engine;

  slave->ui_update_closure = NULL;

  slave->frontend = rig_frontend_new (shell,
                                      RIG_FRONTEND_ID_SLAVE,
                                      NULL,
                                      true); /* start in play mode */

  engine = slave->frontend->engine;
  slave->engine = engine;

  _rig_slave_object_id_magazine = engine->object_id_magazine;

  rig_engine_op_map_context_init (&slave->map_op_ctx,
                                  engine,
                                  map_edit_id_to_play_object_cb,
                                  slave); /* user data */

  rig_engine_op_apply_context_init (&slave->apply_op_ctx,
                                    engine,
                                    register_object_cb,
                                    NULL, /* unregister id */
                                    slave); /* user data */

  slave->pending_edits = rut_queue_new ();
  slave->edits_for_sim = rut_queue_new ();

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
  RutQueueItem *item;

  if (slave->ui_update_closure)
    {
      rut_closure_disconnect (slave->ui_update_closure);
      slave->ui_update_closure = NULL;
    }

  rut_list_for_each (item, &slave->pending_edits->items, list_node)
    g_slice_free (PendingEdit, item->data);
  rut_queue_free (slave->pending_edits);

  rut_list_for_each (item, &slave->edits_for_sim->items, list_node)
    g_slice_free (PendingEdit, item->data);
  rut_queue_free (slave->edits_for_sim);

  rig_engine_op_map_context_destroy (&slave->map_op_ctx);
  rig_engine_op_apply_context_destroy (&slave->apply_op_ctx);

  /* TODO: move to frontend */
  rig_avahi_unregister_service (engine);
  rig_rpc_server_shutdown (engine->slave_service);

  slave->engine = NULL;

  rut_object_unref (slave->frontend);
  slave->frontend = NULL;
}

/* Note: here we have to consider objects that are deleted via edit
 * operations (where we can expect corresponding entries in
 * play_object_to_edit_id_map and edit_id_to_play_object_map) and
 * objects deleted via a ui_update from the simulator, due to some ui
 * logic (where the deleted play-mode object may not have a
 * corresponding edit-mode id).
 */
static void
object_delete_cb (RutObject *object,
                  void *user_data)
{
  RigSlave *slave = user_data;
  uint64_t *object_id =
    g_hash_table_lookup (slave->play_object_to_edit_id_map, object);

  if (object_id)
    {
      g_hash_table_remove (slave->edit_id_to_play_object_map, object_id);
      g_hash_table_remove (slave->play_object_to_edit_id_map, object);
    }
}

static void
rig_slave_paint (RutShell *shell, void *user_data)
{
  RigSlave *slave = user_data;
  RigEngine *engine = slave->engine;
  RigFrontend *frontend = engine->frontend;

  rut_shell_start_redraw (shell);

  /* XXX: we only kick off a new frame in the simulator if it's not
   * still busy... */
  if (!frontend->ui_update_pending)
    {
      RutInputQueue *input_queue = rut_shell_get_input_queue (shell);
      Rig__FrameSetup setup = RIG__FRAME_SETUP__INIT;
      RigPBSerializer *serializer;
      PendingEdit *pending_edit = NULL;

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

      /* Forward any received edits to the simulator too.
       *
       * Note: Although we may have a backlog of edits from the
       * editor, we can currently only send one Rig__UIEdit per
       * frame...
       */
      if (slave->edits_for_sim->len)
        {
          pending_edit = rut_queue_pop_head (slave->edits_for_sim);

          /* Note: we disregard whether we failed to apply the edits
           * in the frontend, since some of the edit operations may
           * succeed, and as long as we can report the error to the
           * user they can decided if they want to reset the slave
           * device.
           */
          /* FIXME: protoc-c should declare this member as const */
          setup.edit = (Rig__UIEdit *)pending_edit->edit;
        }

      rig_frontend_run_simulator_frame (frontend, serializer, &setup);

      if (pending_edit)
        {
          Rig__UIEditResult result = RIG__UIEDIT_RESULT__INIT;

          if (pending_edit->status == false)
            {
              result.has_status = true;
              result.status = false;
            }

          pending_edit->closure (&result, pending_edit->closure_data);

          g_slice_free (PendingEdit, pending_edit);
        }

      rig_pb_serializer_destroy (serializer);

      rut_input_queue_clear (input_queue);

      rut_memory_stack_rewind (engine->sim_frame_stack);
    }

  rut_shell_update_timelines (shell);

  rut_shell_run_pre_paint_callbacks (shell);

  rut_shell_run_start_paint_callbacks (shell);

  rig_engine_paint (engine);

  rig_engine_garbage_collect (engine,
                              object_delete_cb,
                              slave);

  rut_shell_run_post_paint_callbacks (shell);

  rut_memory_stack_rewind (engine->frame_stack);

  rut_shell_end_redraw (shell);

  /* XXX: It would probably be better if we could send multiple
   * Rig__UIEdits in one go when setting up a simulator frame so we
   * wouldn't need to use this trick of continuously queuing redraws
   * to flush the edits through to the simulator.
   */
  if (rut_shell_check_timelines (shell) || slave->edits_for_sim->len)
    {
      rut_shell_queue_redraw (shell);
    }
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
