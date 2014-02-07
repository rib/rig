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

  GList *suspended_controllers;
};


void
rig_editor_init (RutShell *shell, void *user_data)
{
  RigEditor *editor = user_data;
  RigEngine *engine;

  editor->edit_mode_deletes = rut_queue_new ();
  editor->play_mode_deletes = rut_queue_new ();

  /* TODO: RigFrontend should be a trait of the engine */
  editor->frontend = rig_frontend_new (shell,
                                       RIG_FRONTEND_ID_EDITOR,
                                       editor->ui_filename);

  engine = editor->frontend->engine;
  editor->engine = engine;

  /* TODO: RigEditor should be a trait of the engine */
  engine->editor = editor;

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

  for (l = editor->suspend_play_mode_controllers; l; l = l->next)
    rut_object_unref (l->data);
  g_list_free (editor->suspend_play_mode_controllers);
  editor->suspend_play_mode_controllers = NULL;

  rut_queue_free (editor->edit_mode_deletes);
  rut_queue_free (editor->play_mode_deletes);

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

static void *
nop_id_cast_cb (uint64_t id, void *user_data)
{
  return (void *)(uintptr_t)id;
}

static void
nop_register_id_cb (void *object,
                    uint64_t id,
                    void *user_data)
{
  /* NOP */
}

static void
nop_unregister_id_cb (uint64_t id, void *user_data)
{
  /* NOP */
}

static void
register_play_mode_object_cb (void *play_mode_object,
                              uint64_t edit_mode_id,
                              void *user_data)
{
  RigEngine *engine = user_data;
  rig_engine_register_play_mode_object (engine,
                                        edit_mode_id,
                                        play_mode_object);
}

static void
queue_delete_edit_mode_object_cb (uint64_t edit_mode_id, void *user_data)
{
  RigEditor *editor = user_data;
  void *edit_mode_object = (void *)(intptr_t)play_mode_id;
  rut_queue_push_tail (editor->edit_mode_deletes, edit_mode_object);
}

static void
queue_delete_play_mode_object_cb (uint64_t play_mode_id, void *user_data)
{
  RigEditor *editor = user_data;
  void *play_mode_object = (void *)(intptr_t)play_mode_id;
  rut_queue_push_tail (editor->play_mode_deletes, play_mode_object);
}

static uint64_t
map_id_cb (uint64_t id, void *user_data)
{
  RigEngine *engine = user_data;
  uint64_t id;

  id = rig_engine_edit_id_to_play_id (engine, id);

  g_warn_if_fail (id);

  return id;
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

  setup->edit = rig_pb_new (serializer, Rig__UIEdit, rig__ui_edit__init);
  setup->edit->n_ops = engine->n_ops;
  if (setup->edit->n_ops != 0)
    pb_frame_setup->edit->ops = rig_engine_serialize_ops (engine, serializer);

  setup->play_edit = NULL;

  /* Edit operations are applied as they are made so we don't need to
   * apply them here. */
#if 0
  rig_engine_apply_pb_ui_edit (engine,
                               setup->edit,
                               nop_register_id_cb,
                               nop_id_cast_cb, /* ids == obj ptrs */
                               queue_delete_edit_mode_id_cb,
                               engine); /* user data */
#endif

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
    rig_engine_reset_play_mode_ui (engine);

  /* Forward edits to all slaves... */
  for (l = engine->slave_masters; l; l = l->next)
    rig_slave_master_forward_pb_ui_edit (l->data, setup->edit);


  /* Note: Object deletions are deferred so that all edit-to-play mode
   * and play-to-edit mode mappings remain valid until we have
   * finished applying all the operations.
   */

  rut_list_for_each (item, &editor->edit_mode_deletes, list_node)
    {
      rig_engine_unregister_edit_object (engine, item->data);
      rut_object_unref (item->data);
    }
  rut_queue_clear (&editor->edit_mode_deletes);

  rut_list_for_each (item, &editor->play_mode_deletes, list_node)
    {
      rig_engine_unregister_play_mode_object (engine, item->data);
      rut_object_unref (item->data);
    }
  rut_queue_clear (&editor->play_mode_deletes);

  rig_engine_clear_ops (engine);
}

void
rig_editor_apply_last_op (RigEngine *engine)
{
  Rig__Operation *pb_op = rut_queue_peek_tail (engine->ops);
  bool status = rig_engine_pb_op_apply (engine,
                                        pb_op,
                                        nop_register_id_cb,
                                        nop_id_cast_cb, /* ids == obj ptrs */
                                        queue_delete_edit_mode_object_cb,
                                        engine); /* user data */
  g_warn_if_fail (status);
}

void
rig_editor_paint (RutShell *shell, void *user_data)
{
  RigEditor *editor = user_data;
  RigEngine *engine = editor->engine;
  RigFrontend *frontend = engine->frontend;
  ProtobufCService *simulator_service =
    rig_pb_rpc_client_get_service (frontend->frontend_peer->pb_rpc_client);
  RutInputQueue *input_queue = engine->simulator_input_queue;
  Rig__FrameSetup setup = RIG__FRAME_SETUP__INIT;
  RigPBSerializer *serializer;
  float x, y, z;

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

  rig__simulator__run_frame (simulator_service,
                             &setup,
                             handle_run_frame_ack,
                             shell);

  rig_pb_serializer_destroy (serializer);

  rut_input_queue_clear (input_queue);

  rig_engine_paint (engine);

  rut_shell_run_post_paint_callbacks (shell);

  rut_shell_end_redraw (shell);

  rut_memory_stack_rewind (engine->frame_stack);

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

  for (l = ui->controllers; l; l = l->next)
    {
      RigController *controller = l->data;

      if (controller->active)
        {
          RutProperty *active_property =
            rut_introspectable_get_property (controller,
                                             RIG_CONTROLLER_PROP_SUSPENDED);

          rig_engine_op_set_property (engine,
                                      active_property,
                                      &boxed_false);
          rig_editor_apply_last_op (editor);

          editor->suspended_controllers =
            g_list_prepend (editor->suspended_controllers, controller);

          /* We take a reference on all suspended controllers so we
           * don't need to worry if any of the controllers are deleted
           * while in edit mode. */
          rut_object_ref (controller);
        }
    }
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
      rig_editor_apply_last_op (editor);

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
      g_warn_if_fail (engine->ops->len == 0);

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
