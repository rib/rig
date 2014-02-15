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

#include "rig-engine.h"
#include "rig-avahi.h"

static char **_rig_editor_remaining_args = NULL;

static const GOptionEntry _rig_editor_entries[] =
{
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY,
    &_rig_editor_remaining_args, "Project" },
  { 0 }
};

struct _RigEditor
{
  RutShell *shell;
  RutContext *ctx;

  RigFrontend *frontend;
  RigEngine *engine;

  char *ui_filename;

  /* The edit_to_play_object_map hash table lets us map an edit-mode
   * object into a corresponding play-mode object so we can make best
   * effort attempts to apply edit operations to the play-mode ui.
   */
  GHashTable *edit_to_play_object_map;
  GHashTable *play_to_edit_object_map;

  GList *suspended_controllers;

  RutQueue *edit_ops;

  /* Ops queued here will only be sent to the simulator and
   * they won't be mapped from edit-mode to play-mode.
   *
   * These are used for example to tell the simulator to suspend
   * controllers when switching into edit-mode.
   */
  RutQueue *sim_only_ops;

  RigEngineOpApplyContext apply_op_ctx;
};

static void
nop_register_id_cb (void *object,
                    uint64_t id,
                    void *user_data)
{
  /* NOP */
}

static void *
nop_id_cast_cb (uint64_t id, void *user_data)
{
  return (void *)(uintptr_t)id;
}

static void
nop_unregister_id_cb (uint64_t id, void *user_data)
{
  /* NOP */
}

static void
queue_delete_edit_mode_object_cb (uint64_t edit_mode_id, void *user_data)
{
  RigEditor *editor = user_data;
  void *edit_mode_object = (void *)(intptr_t)play_mode_id;
  rig_engine_queue_delete (editor->engine, edit_mode_object);
}

static void
apply_edit_op_cb (Rig__Operation *pb_op,
                  void *user_data)
{
  RigEditor *editor = user_data;
  RigEngine *engine = editor->engine;

  bool status = rig_engine_pb_op_apply (&editor->apply_op_ctx, pb_op);
  g_warn_if_fail (status);

  rut_queue_push_tail (editor->edit_ops, pb_op);

#if 0
  switch (pb_op->type)
    {
    case RIG_ENGINE_OP_TYPE_SET_PLAY_MODE:
      break;
    default:
      play_mode_op = rig_engine_pb_op_map ();
      rut_queue_push_tail (editor->play_ops, play_mode_op);
      break;
    }
#endif
}

static void
simulator_connected_cb (void *user_data)
{
  RigEditor *editor = user_data;
  RigEngine *engine = editor->engine;
  RigFrontend *frontend = editor->frontend;

  /* Note: as opposed to letting the simulator copy the edit mode
   * UI itself to create a play mode UI we explicitly serialize
   * both the edit and play mode UIs so we can forward pointer ids
   * for all objects in both UIs...
   */

  rig_frontend_reload_simulator_uis (frontend,
                                     engine->edit_mode_ui,
                                     false);
  rig_frontend_reload_simulator_uis (frontend,
                                     engine->play_mode_ui,
                                     true);
}

static void *
lookup_play_mode_object_cb (uint64_t edit_mode_id,
                            void *user_data)
{
  RigEngine *engine = user_data;
  return g_hash_table_lookup (engine->edit_to_play_object_map, &edit_mode_id);
}

static void
register_play_mode_object_cb (void *play_mode_object,
                              uint64_t edit_mode_id,
                              void *user_data)
{
  RigEngine *engine = user_data;
  void *edit_mode_object = (void *)(intptr_t)edit_mode_id;

  /* NB: in this case we know the ids fit inside a pointer and
   * the hash table keys are pointers
   */

  g_hash_table_insert (engine->edit_to_play_object_map,
                       edit_mode_object, play_mode_object);
  g_hash_table_insert (engine->play_to_edit_object_map,
                       play_mode_object, edit_mode_object);
}

static RutAsset *
share_asset_cb (RigPBUnSerializer *unserializer,
                Rig__Asset *pb_asset,
                void *user_data)
{
  RutObject *obj = (RutObject *)(intptr_t)pb_asset->id;
  return rut_object_ref (obj);
}

static RigUI *
derive_play_mode_ui (RigEditor *editor)
{
  RigEngine *engine = editor->engine;
  RigUI *src_ui = engine->edit_mode_ui;
  RigPBSerializer *serializer;
  Rig__UI *pb_ui;
  RigPBUnSerializer *unserializer;
  RigUI *copy;

  rig_engine_set_play_mode_ui (engine, NULL);

  g_warn_if_fail (editor->edit_to_play_object_map == NULL);
  g_warn_if_fail (editor->play_to_edit_object_map == NULL);

  editor->edit_to_play_object_map =
    g_hash_table_new (NULL, /* direct hash */
                      NULL); /* direct key equal */
  editor->play_to_edit_object_map =
    g_hash_table_new (NULL, /* direct hash */
                      NULL); /* direct key equal */

  /* For simplicity we use a serializer and unserializer to
   * duplicate the UI, though potentially in the future we may
   * want a more direct way of handling this.
   */
  serializer = rig_pb_serializer_new (engine);

  /* We want to share references to assets between the two UIs
   * since they should be immutable and so we make sure to
   * only keep track of the ids (pointers to assets used) and
   * we will also hook into the corresponding unserialize below
   * to simply return the same objects. */
  rig_pb_serializer_set_only_asset_ids_enabled (serializer, true);

  /* By using pointers instead of an incrementing integer for the
   * object IDs when serializing we can map assets back to the
   * original asset which doesn't need to be copied. */
  rig_pb_serializer_set_use_pointer_ids_enabled (serializer, true);

  pb_ui = rig_pb_serialize_ui (serializer,
                               false, /* play mode */
                               src_ui);

  unserializer = rig_pb_unserializer_new (engine);

  rig_pb_unserializer_set_object_register_callback (unserializer,
                                                    register_play_mode_object_cb,
                                                    editor);

  rig_pb_unserializer_set_id_to_object_callback (unserializer,
                                                 lookup_play_mode_object_cb,
                                                 editor);

  rig_pb_unserializer_set_asset_unserialize_callback (unserializer,
                                                      share_asset_cb,
                                                      NULL);

  copy = rig_pb_unserialize_ui (unserializer, pb_ui);

  rig_pb_unserializer_destroy (unserializer);

  rig_pb_serialized_ui_destroy (pb_ui);

  rig_pb_serializer_destroy (serializer);

  return copy;
}

static void
reset_play_mode_ui (RigEditor *editor)
{
  RigEngine *engine = editor->engine;
  RigUI *play_mode_ui;

  if (editor->edit_to_play_object_map)
    {
      g_hash_table_destroy (editor->edit_to_play_object_map);
      g_hash_table_destroy (editor->play_to_edit_object_map);
      editor->edit_to_play_object_map = NULL;
      editor->play_to_edit_object_map = NULL;
    }

  play_mode_ui = derive_play_mode_ui (engine);
  rig_engine_set_play_mode_ui (engine, play_mode_ui);
  rut_object_unref (play_mode_ui);

  rig_frontend_reload_simulator_uis (engine->frontend);
}

static void
on_ui_load_cb (RigEditor *editor)
{
  RigEngine *engine = editor->engine;

  /* TODO: move controller_view into RigEditor */

  rig_controller_view_update_controller_list (engine->controller_view);

  rig_controller_view_set_controller (engine->controller_view,
                                      ui->controllers->data);

  engine->grid_prim = rut_create_create_grid (engine->ctx,
                                              engine->device_width,
                                              engine->device_height,
                                              100,
                                              100);

  rig_load_asset_list (engine);

  add_light_handle (engine, ui);
  add_play_camera_handle (engine, ui);

  /* Whenever we replace the edit mode graph that implies we need
   * to scrap and update the play mode graph, with a snapshot of
   * the new edit mode graph.
   */
  reset_play_mode_ui (editor);
}

void
rig_editor_init (RutShell *shell, void *user_data)
{
  RigEditor *editor = user_data;
  RigEngine *engine;

  editor->edit_ops = rut_queue_new ();
  editor->sim_only_ops = rut_queue_new ();

  /* TODO: RigFrontend should be a trait of the engine */
  editor->frontend = rig_frontend_new (shell,
                                       RIG_FRONTEND_ID_EDITOR,
                                       editor->ui_filename);

  engine = editor->frontend->engine;
  editor->engine = engine;

  /* TODO: RigEditor should be a trait of the engine */
  engine->editor = editor;


  rig_frontend_set_simulator_connected_callback (editor->frontend,
                                                 simulator_connected_cb,
                                                 editor);

  rig_engine_set_apply_op_callback (engine, apply_edit_op_cb, editor);
  rig_engine_set_ui_load_callback (engine, on_ui_load_cb, editor);

  rig_engine_op_apply_context_init (&editor->apply_op_ctx,
                                    nop_register_id_cb,
                                    nop_id_cast_cb, /* ids == obj ptrs */
                                    queue_delete_edit_mode_object_cb,
                                    editor); /* user data */

  /* TODO move into editor */
  rig_avahi_run_browser (engine);

  rut_shell_add_input_callback (editor->shell,
                                rig_engine_input_handler,
                                engine, NULL);
}

void
rig_editor_fini (RutShell *shell, void *user_data)
{
  RigEditor *editor = user_data;
  RigEngine *engine = editor->engine;
  GList *l;

  rig_engine_op_apply_context_destroy (&editor->apply_op_ctx);

  for (l = editor->suspend_play_mode_controllers; l; l = l->next)
    rut_object_unref (l->data);
  g_list_free (editor->suspend_play_mode_controllers);
  editor->suspend_play_mode_controllers = NULL;

  rut_queue_clear (editor->edit_ops);
  rut_queue_free (editor->edit_ops);
  editor->edit_ops = NULL;

  rut_queue_clear (editor->sim_only_ops);
  rut_queue_free (editor->sim_only_ops);
  editor->sim_only_ops = NULL;

  rut_object_unref (engine);
  editor->engine = NULL;
}

static void
handle_run_frame_ack (const Rig__RunFrameAck *ack,
                      void *closure_data)
{
  RutShell *shell = closure_data;

  rut_shell_finish_frame (shell);

  g_print ("Editor: Run Frame ACK received\n");
}

#if 0
static bool
object_deleted_cb (uint63_t id, void *user_data)
{
  RigEngine *engine = user_data;
  void *object (void *)(intptr_t)id;

  g_hash_table_remove (engine->edit_to_play_object_map, object);
}
#endif

static void
register_play_mode_object (RigEngine *engine,
                           uint64_t edit_mode_id,
                           void *play_mode_object)
{
  void *edit_mode_object = (void *)(intptr_t)edit_mode_id;

  g_hash_table_insert (engine->edit_to_play_object_map,
                       edit_mode_object, play_mode_object);
  g_hash_table_insert (engine->play_to_edit_object_map,
                       play_mode_object, edit_mode_object);
}

static void
register_play_mode_object_cb (void *play_mode_object,
                              uint64_t edit_mode_id,
                              void *user_data)
{
  register_play_mode_object (user_data, edit_mode_id, play_mode_object);
}

static void
queue_delete_play_mode_object_cb (uint64_t play_mode_id, void *user_data)
{
  RigEditor *editor = user_data;
  void *play_mode_object = (void *)(intptr_t)play_mode_id;
  rig_engine_queue_delete (editor->engine, play_mode_object);
}

static uint64_t
edit_id_to_play_id (RigEngine *engine, uint64_t edit_id)
{
  void *ptr_edit_id = (void *)(intptr_t)edit_id;
  void *ptr_play_id =
    g_hash_table_lookup (engine->edit_to_play_object_map,
                         ptr_edit_id);

  return (uint64_t)(intptr_t)ptr_play_id;
}

static uint64_t
map_id_cb (uint64_t id, void *user_data)
{
  RigEngine *engine = user_data;
  uint64_t id;

  id = edit_id_to_play_id (engine, id);
#error "isn't it expected that this can fail, since ui logic can delete objects from the play-mode ui?"
  g_warn_if_fail (id);

  return id;
}

static Rig__UIEdit *
serialize_ops (RigEditor *editor)
{
  Rig__Operation **pb_ops;
  RutQueueItem *item;
  int n_ops = editor->edit_ops->len + editor->sim_only_ops->len;
  int i;

  if (!n_ops)
    return NULL;

  pb_ops =
    rut_memory_stack_memalign (serializer->stack,
                               sizeof (void *) * n_ops,
                               RUT_UTIL_ALIGNOF (void *));

  i = 0;
  rut_list_for_each (item, &editor->edit_ops->items, list_node)
    {
      pb_ops[i++] = item->data;
    }
  rut_list_for_each (item, &editor->sim_only_ops->items, list_node)
    {
      pb_ops[i++] = item->data;
    }
#error "what was the decision about sim_only_ops?"
  return pb_ops;
}

static void
handle_edit_operations (RigEditor *editor,
                        RigPBSerializer *serializer,
                        Rig__FrameSetup *setup)
{
  RigEngine *engine = editor->engine;
  Rig__UIEdit *play_edits;
  GList *l;
  RutQueueItem *item;

  /* XXX: we have some ops relating to entering play-mode that we need
   * to make sure happen after editing the play-mode and edit-mode uis
   *
   * Options:
   * We add a hook into the engine_op_ functions for mapping at the
   * same time as we apply edits, and queues into a separate play_ops
   * queue.
   *
   * Operations that don't make sense to map wouldn't be.
   *
   * In the simulator we can always apply play-mode ops first, which
   * means if we have ops relating to switching to/from play-mode then
   * they will be applied last.
   */
#error "having separate edit_ops and play_ops and sim_only_ops is a bit ugly"
  setup->edit = rig_pb_new (serializer, Rig__UIEdit, rig__ui_edit__init);
  setup->edit->n_ops = editor->edit_ops->len + editor->sim_only_ops->len;
  if (setup->edit->n_ops)
    pb_frame_setup->edit->ops = serialize_ops (editor);

  setup->play_edit = NULL;

  /* XXX:
   * Edit operations are applied as they are made so we don't need to
   * apply them here.
   */

  /* Note: that operations that modify existing objects will
   * refer to play-mode objects after this mapping, but operations
   * that create new objects will use the original edit-mode ids.
   *
   * This allows us to maintain a mapping from edit-mode objects
   * to new play-mode objects via the register/unregister callbacks
   * given when applying these operations to the play-mode ui
   */
  play_edits = rig_engine_map_pb_ui_edit (engine,
                                          setup->edit,
                                          map_id_cb,
                                          register_play_mode_object_cb,
                                          nop_id_cast_cb, /* ids == obj ptrs */
                                          queue_delete_play_mode_object_cb,
                                          engine); /* user data */

  /* Forward both sets of edits to the simulator... */

  if (play_edits)
    setup.play_edit = play_edits;
  else
    {
      /* Note: it's always possible that applying edits directly to
       * the play-mode ui can fail and in that case we simply reset
       * the play mode ui...
       */
      reset_play_mode_ui (editor);
    }

  /* Forward edits to all slaves... */
  for (l = engine->slave_masters; l; l = l->next)
    rig_slave_master_forward_pb_ui_edit (l->data, setup->edit);

  rut_queue_clear (editor->edit_ops);
}

static void
delete_object_cb (RutObject *object, void *user_data)
{
  RigEditor *editor = user_data;
  void *edit_mode_object;
  void *play_mode_object;

  edit_mode_object =
    g_hash_table_lookup (editor->play_to_edit_object_map, object);
  if (edit_mode_object)
    play_mode_object = object;
  else
    {
      play_mode_object =
        g_hash_table_lookup (editor->edit_to_play_object_map, object);

      g_warn_if_fail (play_mode_object);

      edit_mode_object = object;
    }

  g_hash_table_remove (editor->edit_to_play_object_map, edit_mode_object);
  g_hash_table_remove (editor->play_to_edit_object_map, play_mode_object);
}

void
rig_editor_paint (RutShell *shell, void *user_data)
{
  RigEditor *editor = user_data;
  RigEngine *engine = editor->engine;
  RigFrontend *frontend = engine->frontend;
  ProtobufCService *simulator_service =
    rig_pb_rpc_client_get_service (frontend->frontend_peer->pb_rpc_client);

  rut_shell_start_redraw (shell);

  rut_shell_update_timelines (shell);

  /* XXX: These are a bit of a misnomer, since they happen before
   * input handling. Typical pre-paint callbacks are allocation
   * callbacks which we want run before painting and since we want
   * input to be consistent with what we paint we want to make sure
   * allocations are also up to date before input handling.
   */
  rut_shell_run_pre_paint_callbacks (shell);

  /* Again we are immediately about to start painting but this is
   * another set of callbacks that can hook into the start of
   * processing a frame with the difference (compared to pre-paint
   * callbacks) that they aren't unregistered each frame and they
   * aren't sorted with respect to a node in a graph.
   */
  rut_shell_run_start_paint_callbacks (shell);

  rut_shell_dispatch_input_events (shell);

#error "If the simulator is running slowly then it's possible that we won't kick a new frame and so we might also no toggle play mode or send important edits!?"
  if (!frontend->ui_update_pending)
    {
      Rig__FrameSetup setup = RIG__FRAME_SETUP__INIT;
      RutInputQueue *input_queue = engine->simulator_input_queue;
      RigPBSerializer *serializer;
      float x, y, z;

      serializer = rig_pb_serializer_new (engine);

      setup.n_events = input_queue->n_events;
      setup.events =
        rig_pb_serialize_input_events (serializer, input_queue);

      if (frontend->has_resized)
        {
          setup.has_view_width = true;
          setup.view_width = frontend->pending_width;
          setup.has_view_height = true;
          setup.view_height = frontend->pending_height;
          frontend->has_resized = false;
        }

      handle_edit_operations (editor, setup);

      /* Inform the simulator of the offset position of the main camera
       * view so that it can transform its input events accordingly...
       */
      x = y = z = 0;
      rut_graphable_fully_transform_point (engine->main_camera_view,
                                           engine->camera_2d,
                                           &x, &y, &z);
      setup.has_view_x = true;
      setup.view_x = RUT_UTIL_NEARBYINT (x);

      setup.has_view_y = true;
      setup.view_y = RUT_UTIL_NEARBYINT (y);

      setup.has_play_mode = true;
      setup.play_mode = engine->play_mode;

      rig_frontend_run_simulator_frame (frontend, &setup);

      rig_pb_serializer_destroy (serializer);

      rut_input_queue_clear (input_queue);

      rut_memory_stack_rewind (engine->sim_frame_stack);
    }

  rig_engine_paint (engine);

  rig_engine_garbage_collect (engine,
                              delete_object_cb,
                              editor);

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
suspend_play_mode_controllers (RigEditor *editor)
{
  RigEngine *engine = editor->engine;
  RigUI *ui = engine->play_mode_ui;
  RutBoxed boxed_false;
  GList *l;

  boxed_false.type = RUT_PROPERTY_TYPE_BOOLEAN;
  boxed_false.d.boolean_val = false;

  /* Unlike most edit operations (which we map to play-mode ops,
   * forward to slaves and to the simulator), the operations we want to
   * queue here should only be sent to the simulator, and they don't
   * need to be mapped...
   */
  rig_engine_set_apply_op_callback (engine,
                                    apply_sim_only_op_cb,
                                    editor);

  for (l = ui->controllers; l; l = l->next)
    {
      RigController *controller = l->data;

      if (controller->active)
        {
          RutProperty *active_property =
            rut_introspectable_get_property (controller,
                                             RIG_CONTROLLER_PROP_SUSPENDED);

#error fixme
          /* XXX: we need to de-activate these controllers in the
           * frontend and the simulator, but unlike other operations
           * we are applying them directly to the play-mode objects
           * and so they shouldn't be mapped - nor should they be
           * forwarded to slaves :-/ urgh! */
          rig_engine_op_set_property (engine,
                                      active_property,
                                      &boxed_false);

          editor->suspended_controllers =
            g_list_prepend (editor->suspended_controllers, controller);

          /* We take a reference on all suspended controllers so we
           * don't need to worry if any of the controllers are deleted
           * while in edit mode. */
          rut_object_ref (controller);
        }
    }

  rig_engine_set_apply_op_callback (engine,
                                    apply_sim_only_op_cb,
                                    editor);
}

static void
resume_play_mode_controllers (RigEditor *editor)
{
  RigEngine *engine = editor->engine;
  RutBoxed boxed_true;
  GList *l;

  boxed_true.type = RUT_PROPERTY_TYPE_BOOLEAN;
  boxed_true.d.boolean_val = true;

  for (l = editor->suspended_controllers; l; l = l->next)
    {
      RigController *controller = l->data;
      RutProperty *active_property =
        rut_introspectable_get_property (controller,
                                         RIG_CONTROLLER_PROP_SUSPENDED);

      rig_engine_op_set_property (engine,
                                  active_property,
                                  &boxed_false);

      rut_object_unref (controller);
    }

  g_list_free (editor->suspended_controllers);
  editor->suspended_controllers = NULL;
}

void
rig_editor_set_play_mode_enabled (RigEditor *editor, bool enabled)
{
  RigEngine *engine = editor->engine;

  engine->play_mode = enabled;

  if (engine->play_mode)
    {
      /* No edit operations should have been queued up while in play
       * mode... */
      g_warn_if_fail (editor->edit_ops->len == 0);

      rig_engine_set_current_ui (engine, engine->play_mode_ui);
      rig_camera_view_set_play_mode_enabled (engine->main_camera_view,
                                             true);
      resume_play_mode_controllers (editor);
    }
  else
    {
      suspend_play_mode_controllers (editor);
      rig_engine_set_current_ui (engine, engine->edit_mode_ui);
      rig_camera_view_set_play_mode_enabled (engine->main_camera_view,
                                             false);
    }
}
