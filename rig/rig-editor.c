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

#include "rut-renderer.h"

#include "rig-engine.h"
#include "rig-avahi.h"
#include "rig-slave-master.h"
#include "rig-inspector.h"

#include "rig.pb-c.h"

#include "components/rig-button-input.h"
#include "components/rig-camera.h"
#include "components/rig-diamond.h"
#include "components/rig-hair.h"
#include "components/rig-light.h"
#include "components/rig-material.h"
#include "components/rig-model.h"
#include "components/rig-nine-slice.h"
#include "components/rig-pointalism-grid.h"
#include "components/rig-shape.h"

static char **_rig_editor_remaining_args = NULL;

static const GOptionEntry _rig_editor_entries[] =
{
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY,
    &_rig_editor_remaining_args, "Project" },
  { 0 }
};

struct _RigEditor
{
  RutObjectBase _base;

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

  RutQueue *edit_ops;

  RigEngineOpApplyContext apply_op_ctx;
  RigEngineOpCopyContext copy_op_ctx;
  RigEngineOpMapContext map_op_ctx;
  RigEngineOpApplyContext play_apply_op_ctx;
};

static void
nop_register_id_cb (void *object,
                    uint64_t id,
                    void *user_data)
{
  /* NOP */
}

#if 0
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
#endif

static void
apply_edit_op_cb (Rig__Operation *pb_op,
                  void *user_data)
{
  RigEditor *editor = user_data;

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

static void *
lookup_play_mode_object_cb (uint64_t edit_mode_id,
                            void *user_data)
{
  RigEditor *editor = user_data;
  void *edit_mode_object = (void *)(intptr_t)edit_mode_id;
  return g_hash_table_lookup (editor->edit_to_play_object_map, edit_mode_object);
}

static void
register_play_mode_object (RigEditor *editor,
                           uint64_t edit_mode_id,
                           void *play_mode_object)
{
  /* NB: in this case we know the ids fit inside a pointer and
   * the hash table keys are pointers
   */

  void *edit_mode_object = (void *)(intptr_t)edit_mode_id;

  g_hash_table_insert (editor->edit_to_play_object_map,
                       edit_mode_object, play_mode_object);
  g_hash_table_insert (editor->play_to_edit_object_map,
                       play_mode_object, edit_mode_object);
}

static void
register_play_mode_object_cb (void *play_mode_object,
                              uint64_t edit_mode_id,
                              void *user_data)
{
  RigEditor *editor = user_data;
  register_play_mode_object (editor, edit_mode_id, play_mode_object);
}

static uint64_t
edit_id_to_play_id (RigEditor *editor, uint64_t edit_id)
{
  void *ptr_edit_id = (void *)(intptr_t)edit_id;
  void *ptr_play_id = g_hash_table_lookup (editor->edit_to_play_object_map,
                                           ptr_edit_id);

  return (uint64_t)(intptr_t)ptr_play_id;
}

static uint64_t
map_id_cb (uint64_t id, void *user_data)
{
  RigEditor *editor = user_data;
  return edit_id_to_play_id (editor, id);
}

static RigAsset *
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

  play_mode_ui = derive_play_mode_ui (editor);
  rig_engine_set_play_mode_ui (engine, play_mode_ui);
  rut_object_unref (play_mode_ui);

  rig_engine_op_apply_context_set_ui (&editor->play_apply_op_ctx,
                                      play_mode_ui);

  rig_frontend_reload_simulator_ui (engine->frontend,
                                    engine->play_mode_ui,
                                    true); /* play mode */
}

typedef struct _ResultInputClosure
{
  RutObject *result;
  RigEngine *engine;
} ResultInputClosure;

void
rig_editor_free_result_input_closures (RigEngine *engine)
{
  GList *l;

  for (l = engine->result_input_closures; l; l = l->next)
    g_slice_free (ResultInputClosure, l->data);
  g_list_free (engine->result_input_closures);
  engine->result_input_closures = NULL;
}

static void
apply_asset_input_with_entity (RigEngine *engine,
                               RigAsset *asset,
                               RigEntity *entity)
{
  RigUndoJournal *sub_journal;
  RigAssetType type = rig_asset_get_type (asset);
  RigMaterial *material;
  RutObject *geom;

  rig_editor_push_undo_subjournal (engine);

  switch (type)
    {
    case RIG_ASSET_TYPE_TEXTURE:
    case RIG_ASSET_TYPE_NORMAL_MAP:
    case RIG_ASSET_TYPE_ALPHA_MASK:
        {
          material =
            rig_entity_get_component (entity, RUT_COMPONENT_TYPE_MATERIAL);

          if (!material)
            {
              material = rig_material_new (engine->ctx, asset);
              rig_undo_journal_add_component (engine->undo_journal,
                                              entity, material);
            }

          if (type == RIG_ASSET_TYPE_TEXTURE)
            rig_material_set_color_source_asset (material, asset);
          else if (type == RIG_ASSET_TYPE_NORMAL_MAP)
            rig_material_set_normal_map_asset (material, asset);
          else if (type == RIG_ASSET_TYPE_ALPHA_MASK)
            rig_material_set_alpha_mask_asset (material, asset);

          rut_renderer_notify_entity_changed (engine->renderer, entity);

          geom = rig_entity_get_component (entity,
                                           RUT_COMPONENT_TYPE_GEOMETRY);
          if (!geom)
            {
              RigShape *shape = rig_shape_new (engine->ctx, TRUE, 0, 0);
              rig_undo_journal_add_component (engine->undo_journal,
                                              entity, shape);
              geom = shape;
            }

          break;
        }
    case RIG_ASSET_TYPE_MESH:
        {
          RigModel *model;
          float x_range, y_range, z_range, max_range;

          material =
            rig_entity_get_component (entity, RUT_COMPONENT_TYPE_MATERIAL);

          if (!material)
            {
              material = rig_material_new (engine->ctx, asset);
              rig_undo_journal_add_component (engine->undo_journal,
                                              entity, material);
            }

          geom = rig_entity_get_component (entity,
                                           RUT_COMPONENT_TYPE_GEOMETRY);

          if (geom && rut_object_get_type (geom) == &rig_model_type)
            {
              model = geom;
              if (rig_model_get_asset (model) == asset)
                break;
              else
                rig_undo_journal_delete_component (engine->undo_journal, model);
            }
          else if (geom)
            rig_undo_journal_delete_component (engine->undo_journal, geom);

          model = rig_model_new_from_asset (engine->ctx, asset);
          rig_undo_journal_add_component (engine->undo_journal, entity, model);

          x_range = model->max_x - model->min_x;
          y_range = model->max_y - model->min_y;
          z_range = model->max_z - model->min_z;

          max_range = x_range;
          if (y_range > max_range)
            max_range = y_range;
          if (z_range > max_range)
            max_range = z_range;

          rig_entity_set_scale (entity, 200.0 / max_range);

          rut_renderer_notify_entity_changed (engine->renderer, entity);

          break;
        }
    case RIG_ASSET_TYPE_BUILTIN:
      if (asset == engine->text_builtin_asset)
        {
          RutText *text;
          CoglColor color;
          RigHair *hair;

          hair = rig_entity_get_component (entity,
                                           RUT_COMPONENT_TYPE_HAIR);

          if (hair)
            rig_undo_journal_delete_component (engine->undo_journal, hair);

          geom = rig_entity_get_component (entity,
                                           RUT_COMPONENT_TYPE_GEOMETRY);

          if (geom && rut_object_get_type (geom) == &rut_text_type)
            break;
          else if (geom)
            rig_undo_journal_delete_component (engine->undo_journal, geom);

          text = rut_text_new_with_text (engine->ctx, "Sans 60px", "text");
          cogl_color_init_from_4f (&color, 1, 1, 1, 1);
          rut_text_set_color (text, &color);
          rig_undo_journal_add_component (engine->undo_journal, entity, text);

          rut_renderer_notify_entity_changed (engine->renderer, entity);
        }
      else if (asset == engine->circle_builtin_asset)
        {
          RigShape *shape;
          int tex_width = 200, tex_height = 200;

          geom = rig_entity_get_component (entity,
                                           RUT_COMPONENT_TYPE_GEOMETRY);

          if (geom && rut_object_get_type (geom) == &rig_shape_type)
            break;
          else if (geom)
            rig_undo_journal_delete_component (engine->undo_journal, geom);

          material =
            rig_entity_get_component (entity, RUT_COMPONENT_TYPE_MATERIAL);

          if (material)
            {
              RigAsset *texture_asset =
                rig_material_get_color_source_asset (material);
              if (texture_asset)
                {
                  if (rig_asset_get_is_video (texture_asset))
                    {
                      /* XXX: until we start decoding the
                       * video we don't know the size of the
                       * video so for now we just assume a
                       * default size. Maybe we should just
                       * decode a single frame to find out the
                       * size? */
                      tex_width = 640;
                      tex_height = 480;
                    }
                  else
                    {
                      CoglTexture *texture =
                        rig_asset_get_texture (texture_asset);
                      tex_width = cogl_texture_get_width (texture);
                      tex_height = cogl_texture_get_height (texture);
                    }
                }
            }

          shape = rig_shape_new (engine->ctx, TRUE, tex_width,
                                 tex_height);
          rig_undo_journal_add_component (engine->undo_journal,
                                          entity, shape);

          rut_renderer_notify_entity_changed (engine->renderer, entity);
        }
      else if (asset == engine->diamond_builtin_asset)
        {
          RigDiamond *diamond;

          geom = rig_entity_get_component (entity,
                                           RUT_COMPONENT_TYPE_GEOMETRY);

          if (geom && rut_object_get_type (geom) == &rig_diamond_type)
            break;
          else if (geom)
            rig_undo_journal_delete_component (engine->undo_journal, geom);

          diamond = rig_diamond_new (engine->ctx, 200);
          rig_undo_journal_add_component (engine->undo_journal,
                                          entity, diamond);
          rut_object_unref (diamond);

          rut_renderer_notify_entity_changed (engine->renderer, entity);
        }
      else if (asset == engine->nine_slice_builtin_asset)
        {
          RigNineSlice *nine_slice;
          int tex_width = 200, tex_height = 200;

          geom = rig_entity_get_component (entity,
                                           RUT_COMPONENT_TYPE_GEOMETRY);

          if (geom && rut_object_get_type (geom) == &rig_nine_slice_type)
            break;
          else if (geom)
            rig_undo_journal_delete_component (engine->undo_journal, geom);

          material =
            rig_entity_get_component (entity,
                                      RUT_COMPONENT_TYPE_MATERIAL);

          if (material)
            {
              RigAsset *color_source_asset =
                rig_material_get_color_source_asset (material);
              if (color_source_asset)
                {
                  if (rig_asset_get_is_video (color_source_asset))
                    {
                      /* XXX: until we start decoding the
                       * video we don't know the size of the
                       * video so for now we just assume a
                       * default size. Maybe we should just
                       * decode a single frame to find out the
                       * size? */
                      tex_width = 640;
                      tex_height = 480;
                    }
                  else
                    {
                      CoglTexture *texture =
                        rig_asset_get_texture (color_source_asset);
                      tex_width = cogl_texture_get_width (texture);
                      tex_height = cogl_texture_get_height (texture);
                    }
                }
            }

          nine_slice = rig_nine_slice_new (engine->ctx, NULL,
                                           0, 0, 0, 0,
                                           tex_width, tex_height);
          rig_undo_journal_add_component (engine->undo_journal,
                                          entity, nine_slice);

          rut_renderer_notify_entity_changed (engine->renderer, entity);
        }
      else if (asset == engine->pointalism_grid_builtin_asset)
        {
          RigPointalismGrid *grid;
          int tex_width = 200, tex_height = 200;

          geom = rig_entity_get_component (entity,
                                           RUT_COMPONENT_TYPE_GEOMETRY);

          if (geom && rut_object_get_type (geom) ==
              &rig_pointalism_grid_type)
            {
              break;
            }
          else if (geom)
            rig_undo_journal_delete_component (engine->undo_journal, geom);

          material =
            rig_entity_get_component (entity,
                                      RUT_COMPONENT_TYPE_MATERIAL);

          if (material)
            {
              RigAsset *texture_asset =
                rig_material_get_color_source_asset (material);
              if (texture_asset)
                {
                  if (rig_asset_get_is_video (texture_asset))
                    {
                      tex_width = 640;
                      tex_height = 480;
                    }
                  else
                    {
                      CoglTexture *texture =
                        rig_asset_get_texture (texture_asset);
                      tex_width = cogl_texture_get_width (texture);
                      tex_height = cogl_texture_get_height (texture);
                    }
                }
            }

          grid = rig_pointalism_grid_new (engine->ctx, 20, tex_width,
                                          tex_height);

          rig_undo_journal_add_component (engine->undo_journal, entity, grid);

          rut_renderer_notify_entity_changed (engine->renderer, entity);
        }
      else if (asset == engine->hair_builtin_asset)
        {
          RigHair *hair = rig_entity_get_component (entity,
                                                    RUT_COMPONENT_TYPE_HAIR);
          if (hair)
            break;

          hair = rig_hair_new (engine->ctx);
          rig_undo_journal_add_component (engine->undo_journal, entity, hair);
          geom = rig_entity_get_component (entity,
                                           RUT_COMPONENT_TYPE_GEOMETRY);

          if (geom && rut_object_get_type (geom) == &rig_model_type)
            {
              RigModel *hair_geom = rig_model_new_for_hair (geom);

              rig_hair_set_length (hair,
                                   rig_model_get_default_hair_length (hair_geom));

              rig_undo_journal_delete_component (engine->undo_journal, geom);
              rig_undo_journal_add_component (engine->undo_journal,
                                              entity, hair_geom);
            }

          rut_renderer_notify_entity_changed (engine->renderer, entity);
        }
      else if (asset == engine->button_input_builtin_asset)
        {
          RigButtonInput *button_input =
            rig_entity_get_component (entity, RUT_COMPONENT_TYPE_INPUT);
          if (button_input)
            break;

          button_input = rig_button_input_new (engine->ctx);
          rig_undo_journal_add_component (engine->undo_journal,
                                          entity, button_input);

          rut_renderer_notify_entity_changed (engine->renderer, entity);
        }
      break;
    }

  sub_journal = rig_editor_pop_undo_subjournal (engine);

  if (rig_undo_journal_is_empty (sub_journal))
    rig_undo_journal_free (sub_journal);
  else
    rig_undo_journal_log_subjournal (engine->undo_journal, sub_journal);
}

static void
apply_result_input_with_entity (RigEntity *entity,
                                ResultInputClosure *closure)
{
  if (rut_object_get_type (closure->result) == &rig_asset_type)
    apply_asset_input_with_entity (closure->engine,
                                   closure->result,
                                   entity);
  else if (rut_object_get_type (closure->result) == &rig_entity_type)
    rig_select_object (closure->engine,
                       closure->result,
                       RUT_SELECT_ACTION_REPLACE);
  else if (rut_object_get_type (closure->result) == &rig_controller_type)
    rig_select_object (closure->engine,
                       closure->result,
                       RUT_SELECT_ACTION_REPLACE);
}

static RutInputEventStatus
result_input_cb (RutInputRegion *region,
                 RutInputEvent *event,
                 void *user_data)
{
  ResultInputClosure *closure = user_data;
  RutInputEventStatus status = RUT_INPUT_EVENT_STATUS_UNHANDLED;

  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION)
    {
      if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_UP)
        {
          RigEngine *engine = closure->engine;

          if (engine->objects_selection->objects)
            {
              g_list_foreach (engine->objects_selection->objects,
                              (GFunc) apply_result_input_with_entity,
                              closure);
            }
          else
            {
              RigEntity *entity = rig_entity_new (engine->ctx);
              rig_undo_journal_add_entity (engine->undo_journal,
                                           engine->edit_mode_ui->scene,
                                           entity);
              rig_select_object (engine, entity, RUT_SELECT_ACTION_REPLACE);
              apply_result_input_with_entity (entity, closure);
            }

          rig_editor_update_inspector (engine);
          rut_shell_queue_redraw (engine->ctx->shell);
          status = RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

  return status;
}
void
rig_editor_clear_search_results (RigEngine *engine)
{
  if (engine->search_results_vbox)
    {
      rut_fold_set_child (engine->search_results_fold, NULL);
      rig_editor_free_result_input_closures (engine);

      /* NB: We don't maintain any additional references on asset
       * result widgets beyond the references for them being in the
       * scene graph and so setting a NULL fold child should release
       * everything underneath...
       */

      engine->search_results_vbox = NULL;

      engine->entity_results = NULL;
      engine->controller_results = NULL;
      engine->assets_geometry_results = NULL;
      engine->assets_image_results = NULL;
      engine->assets_video_results = NULL;
      engine->assets_other_results = NULL;
    }
}

static RutFlowLayout *
add_results_flow (RutContext *ctx,
                  const char *label,
                  RutBoxLayout *vbox)
{
  RutFlowLayout *flow =
    rut_flow_layout_new (ctx, RUT_FLOW_LAYOUT_PACKING_LEFT_TO_RIGHT);
  RutText *text = rut_text_new_with_text (ctx, "Bold Sans 15px", label);
  CoglColor color;
  RutBin *label_bin = rut_bin_new (ctx);
  RutBin *flow_bin = rut_bin_new (ctx);

  rut_bin_set_left_padding (label_bin, 10);
  rut_bin_set_top_padding (label_bin, 10);
  rut_bin_set_bottom_padding (label_bin, 10);
  rut_bin_set_child (label_bin, text);
  rut_object_unref (text);

  rut_color_init_from_uint32 (&color, 0xffffffff);
  rut_text_set_color (text, &color);

  rut_box_layout_add (vbox, FALSE, label_bin);
  rut_object_unref (label_bin);

  rut_flow_layout_set_x_padding (flow, 5);
  rut_flow_layout_set_y_padding (flow, 5);
  rut_flow_layout_set_max_child_height (flow, 100);

  //rut_bin_set_left_padding (flow_bin, 5);
  rut_bin_set_child (flow_bin, flow);
  rut_object_unref (flow);

  rut_box_layout_add (vbox, TRUE, flow_bin);
  rut_object_unref (flow_bin);

  return flow;
}

static void
add_search_result (RigEngine *engine,
                   RutObject *result)
{
  ResultInputClosure *closure;
  RutStack *stack;
  RutBin *bin;
  CoglTexture *texture;
  RutInputRegion *region;
  RutDragBin *drag_bin;

  closure = g_slice_new (ResultInputClosure);
  closure->result = result;
  closure->engine = engine;

  bin = rut_bin_new (engine->ctx);

  drag_bin = rut_drag_bin_new (engine->ctx);
  rut_drag_bin_set_payload (drag_bin, result);
  rut_bin_set_child (bin, drag_bin);
  rut_object_unref (drag_bin);

  stack = rut_stack_new (engine->ctx, 0, 0);
  rut_drag_bin_set_child (drag_bin, stack);
  rut_object_unref (stack);

  region = rut_input_region_new_rectangle (0, 0, 100, 100,
                                           result_input_cb,
                                           closure);
  rut_stack_add (stack, region);
  rut_object_unref (region);

  if (rut_object_get_type (result) == &rig_asset_type)
    {
      RigAsset *asset = result;

      texture = rig_asset_get_texture (asset);

      if (texture)
        {
          RutImage *image = rut_image_new (engine->ctx, texture);
          rut_stack_add (stack, image);
          rut_object_unref (image);
        }
      else
        {
          char *basename = g_path_get_basename (rig_asset_get_path (asset));
          RutText *text = rut_text_new_with_text (engine->ctx, NULL, basename);
          rut_stack_add (stack, text);
          rut_object_unref (text);
          g_free (basename);
        }
    }
  else if (rut_object_get_type (result) == &rig_entity_type)
    {
      RigEntity *entity = result;
      RutBoxLayout *vbox =
        rut_box_layout_new (engine->ctx,
                            RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM);
      RutImage *image;
      RutText *text;

      rut_stack_add (stack, vbox);
      rut_object_unref (vbox);

#warning "Create a sensible icon to represent entities"
      texture = rut_load_texture_from_data_file (engine->ctx,
                                                 "transparency-grid.png", NULL);
      image = rut_image_new (engine->ctx, texture);
      cogl_object_unref (texture);

      rut_box_layout_add (vbox, FALSE, image);
      rut_object_unref (image);

      text = rut_text_new_with_text (engine->ctx, NULL, entity->label);
      rut_box_layout_add (vbox, false, text);
      rut_object_unref (text);
    }
  else if (rut_object_get_type (result) == &rig_controller_type)
    {
      RigController *controller = result;
      RutBoxLayout *vbox =
        rut_box_layout_new (engine->ctx,
                            RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM);
      RutImage *image;
      RutText *text;

      rut_stack_add (stack, vbox);
      rut_object_unref (vbox);

#warning "Create a sensible icon to represent controllers"
      texture = rut_load_texture_from_data_file (engine->ctx,
                                                 "transparency-grid.png", NULL);
      image = rut_image_new (engine->ctx, texture);
      cogl_object_unref (texture);

      rut_box_layout_add (vbox, false, image);
      rut_object_unref (image);

      text = rut_text_new_with_text (engine->ctx, NULL, controller->label);
      rut_box_layout_add (vbox, FALSE, text);
      rut_object_unref (text);
    }

  if (rut_object_get_type (result) == &rig_asset_type)
    {
      RigAsset *asset = result;

      if (rig_asset_has_tag (asset, "geometry"))
        {
          if (!engine->assets_geometry_results)
            {
              engine->assets_geometry_results =
                add_results_flow (engine->ctx,
                                  "Geometry",
                                  engine->search_results_vbox);
            }

          rut_flow_layout_add (engine->assets_geometry_results, bin);
          rut_object_unref (bin);
        }
      else if (rig_asset_has_tag (asset, "image"))
        {
          if (!engine->assets_image_results)
            {
              engine->assets_image_results =
                add_results_flow (engine->ctx,
                                  "Images",
                                  engine->search_results_vbox);
            }

          rut_flow_layout_add (engine->assets_image_results, bin);
          rut_object_unref (bin);
        }
      else if (rig_asset_has_tag (asset, "video"))
        {
          if (!engine->assets_video_results)
            {
              engine->assets_video_results =
                add_results_flow (engine->ctx,
                                  "Video",
                                  engine->search_results_vbox);
            }

          rut_flow_layout_add (engine->assets_video_results, bin);
          rut_object_unref (bin);
        }
      else
        {
          if (!engine->assets_other_results)
            {
              engine->assets_other_results =
                add_results_flow (engine->ctx,
                                  "Other",
                                  engine->search_results_vbox);
            }

          rut_flow_layout_add (engine->assets_other_results, bin);
          rut_object_unref (bin);
        }
    }
  else if (rut_object_get_type (result) == &rig_entity_type)
    {
      if (!engine->entity_results)
        {
          engine->entity_results =
            add_results_flow (engine->ctx,
                              "Entity",
                              engine->search_results_vbox);
        }

      rut_flow_layout_add (engine->entity_results, bin);
      rut_object_unref (bin);
    }
  else if (rut_object_get_type (result) == &rig_controller_type)
    {
      if (!engine->controller_results)
        {
          engine->controller_results =
            add_results_flow (engine->ctx,
                              "Controllers",
                              engine->search_results_vbox);
        }

      rut_flow_layout_add (engine->controller_results, bin);
      rut_object_unref (bin);
    }


  /* XXX: It could be nicer to have some form of weak pointer
   * mechanism to manage the lifetime of these closures... */
  engine->result_input_closures = g_list_prepend (engine->result_input_closures,
                                               closure);
}

typedef struct _SearchState
{
  RigEngine *engine;
  const char *search;
  bool found;
} SearchState;

static RutTraverseVisitFlags
add_matching_entity_cb (RutObject *object,
                        int depth,
                        void *user_data)
{
  if (rut_object_get_type (object) == &rig_entity_type)
    {
      RigEntity *entity = object;
      SearchState *state = user_data;

      if (state->search == NULL)
        {
          state->found = true;
          add_search_result (state->engine, entity);
        }
      else if (entity->label &&
               strncmp (entity->label, "rig:", 4) != 0)
        {
          char *entity_label = g_ascii_strdown (entity->label, -1);
#warning "FIXME: handle utf8 string comparisons!"

          if (strstr (entity_label, state->search))
            {
              state->found = true;
              add_search_result (state->engine, entity);
            }

          g_free (entity_label);
        }
    }
  return RUT_TRAVERSE_VISIT_CONTINUE;
}

static void
add_matching_controller (RigController *controller,
                         SearchState *state)
{
  char *controller_label = g_ascii_strdown (controller->label, -1);
#warning "FIXME: handle utf8 string comparisons!"

  if (state->search == NULL ||
      strstr (controller_label, state->search))
    {
      state->found = true;
      add_search_result (state->engine, controller);
    }

  g_free (controller_label);
}

static CoglBool
asset_matches_search (RigEngine *engine,
                      RigAsset *asset,
                      const char *search)
{
  GList *l;
  bool found = false;
  const GList *inferred_tags;
  char **tags;
  const char *path;
  int i;

  for (l = engine->required_search_tags; l; l = l->next)
    {
      if (rig_asset_has_tag (asset, l->data))
        {
          found = true;
          break;
        }
    }

  if (engine->required_search_tags && found == false)
    return FALSE;

  if (!search)
    return TRUE;

  inferred_tags = rig_asset_get_inferred_tags (asset);
  tags = g_strsplit_set (search, " \t", 0);

  path = rig_asset_get_path (asset);
  if (path)
    {
      if (strstr (path, search))
        return TRUE;
    }

  for (i = 0; tags[i]; i++)
    {
      const GList *l;
      CoglBool found = FALSE;

      for (l = inferred_tags; l; l = l->next)
        {
          if (strcmp (tags[i], l->data) == 0)
            {
              found = TRUE;
              break;
            }
        }

      if (!found)
        {
          g_strfreev (tags);
          return FALSE;
        }
    }

  g_strfreev (tags);
  return TRUE;
}

static bool
rig_search_with_text (RigEngine *engine, const char *user_search)
{
  GList *l;
  int i;
  CoglBool found = FALSE;
  SearchState state;
  char *search;

  if (user_search)
    search = g_ascii_strdown (user_search, -1);
  else
    search = NULL;
#warning "FIXME: handle non-ascii searches!"

  rig_editor_clear_search_results (engine);

  engine->search_results_vbox =
    rut_box_layout_new (engine->ctx, RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM);
  rut_fold_set_child (engine->search_results_fold,
                      engine->search_results_vbox);
  rut_object_unref (engine->search_results_vbox);

  for (l = engine->edit_mode_ui->assets, i= 0; l; l = l->next, i++)
    {
      RigAsset *asset = l->data;

      if (!asset_matches_search (engine, asset, search))
        continue;

      found = TRUE;
      add_search_result (engine, asset);
    }

  state.engine = engine;
  state.search = search;
  state.found = FALSE;

  if (!engine->required_search_tags ||
      rut_util_find_tag (engine->required_search_tags, "entity"))
    {
      rut_graphable_traverse (engine->edit_mode_ui->scene,
                              RUT_TRAVERSE_DEPTH_FIRST,
                              add_matching_entity_cb,
                              NULL, /* post visit */
                              &state);
    }

  if (!engine->required_search_tags ||
      rut_util_find_tag (engine->required_search_tags, "controller"))
    {
      for (l = engine->edit_mode_ui->controllers; l; l = l->next)
        add_matching_controller (l->data, &state);
    }

  g_free (search);

  if (!engine->required_search_tags)
    return found | state.found;
  else
    {
      /* If the user has toggled on certain search
       * tag constraints then we don't want to
       * fallback to matching everything when there
       * are no results from the search so we
       * always claim that something was found...
       */
      return TRUE;
    }
}

static void
rig_run_search (RigEngine *engine)
{
  if (!rig_search_with_text (engine, rut_text_get_text (engine->search_text)))
    rig_search_with_text (engine, NULL);
}

void
rig_editor_refresh_thumbnails (RigAsset *video, void *user_data)
{
  rig_run_search (user_data);
}

static void
asset_search_update_cb (RutText *text,
                        void *user_data)
{
  rig_run_search (user_data);
}

static void
add_asset (RigEngine *engine, GFileInfo *info, GFile *asset_file)
{
  GFile *assets_dir = g_file_new_for_path (engine->ctx->assets_location);
  char *path = g_file_get_relative_path (assets_dir, asset_file);
  GList *l;
  RigAsset *asset = NULL;

  /* Avoid loading duplicate assets... */
  for (l = engine->edit_mode_ui->assets; l; l = l->next)
    {
      RigAsset *existing = l->data;

      if (strcmp (rig_asset_get_path (existing), path) == 0)
        return;
    }

  asset = rig_load_asset (engine, info, asset_file);
  if (asset)
    engine->edit_mode_ui->assets =
      g_list_prepend (engine->edit_mode_ui->assets, asset);
}

#if 0
static GList *
copy_tags (GList *tags)
{
  GList *l, *copy = NULL;
  for (l = tags; l; l = l->next)
    {
      char *tag = g_intern_string (l->data);
      copy = g_list_prepend (copy, tag);
    }
  return copy;
}
#endif

static void
enumerate_dir_for_assets (RigEngine *engine,
                          GFile *directory);

void
enumerate_file_info (RigEngine *engine, GFile *parent, GFileInfo *info)
{
  GFileType type = g_file_info_get_file_type (info);
  const char *name = g_file_info_get_name (info);

  if (name[0] == '.')
    return;

  if (type == G_FILE_TYPE_DIRECTORY)
    {
      GFile *directory = g_file_get_child (parent, name);

      enumerate_dir_for_assets (engine, directory);

      g_object_unref (directory);
    }
  else if (type == G_FILE_TYPE_REGULAR ||
           type == G_FILE_TYPE_SYMBOLIC_LINK)
    {
      if (rut_file_info_is_asset (info, name))
        {
          GFile *image = g_file_get_child (parent, name);
          add_asset (engine, info, image);
          g_object_unref (image);
        }
    }
}

#ifdef USE_ASYNC_IO
typedef struct _AssetEnumeratorState
{
  RigEngine *engine;
  GFile *directory;
  GFileEnumerator *enumerator;
  GCancellable *cancellable;
  GList *tags;
} AssetEnumeratorState;

static void
cleanup_assets_enumerator (AssetEnumeratorState *state)
{
  if (state->enumerator)
    g_object_unref (state->enumerator);

  g_object_unref (state->cancellable);
  g_object_unref (state->directory);
  g_list_free (state->tags);

  state->engine->asset_enumerators =
    g_list_remove (state->engine->asset_enumerators, state);

  g_slice_free (AssetEnumeratorState, state);
}

static void
assets_found_cb (GObject *source_object,
                 GAsyncResult *res,
                 gpointer user_data)
{
  AssetEnumeratorState *state = user_data;
  GList *infos;
  GList *l;

  infos = g_file_enumerator_next_files_finish (state->enumerator,
                                               res,
                                               NULL);
  if (!infos)
    {
      cleanup_assets_enumerator (state);
      return;
    }

  for (l = infos; l; l = l->next)
    enumerate_file_info (state->engine, state->directory, l->data);

  g_list_free (infos);

  g_file_enumerator_next_files_async (state->enumerator,
                                      5, /* what's a good number here? */
                                      G_PRIORITY_DEFAULT,
                                      state->cancellable,
                                      asset_found_cb,
                                      state);
}

static void
assets_enumerator_cb (GObject *source_object,
                      GAsyncResult *res,
                      gpointer user_data)
{
  AssetEnumeratorState *state = user_data;
  GError *error = NULL;

  state->enumerator =
    g_file_enumerate_children_finish (state->directory, res, &error);
  if (!state->enumerator)
    {
      g_warning ("Error while looking for assets: %s", error->message);
      g_error_free (error);
      cleanup_assets_enumerator (state);
      return;
    }

  g_file_enumerator_next_files_async (state->enumerator,
                                      5, /* what's a good number here? */
                                      G_PRIORITY_DEFAULT,
                                      state->cancellable,
                                      assets_found_cb,
                                      state);
}

static void
enumerate_dir_for_assets_async (RigEngine *engine,
                                GFile *directory)
{
  AssetEnumeratorState *state = g_slice_new0 (AssetEnumeratorState);

  state->engine = engine;
  state->directory = g_object_ref (directory);

  state->cancellable = g_cancellable_new ();

  /* NB: we can only use asynchronous IO if we are running with a Glib
   * mainloop */
  g_file_enumerate_children_async (file,
                                   "standard::*",
                                   G_FILE_QUERY_INFO_NONE,
                                   G_PRIORITY_DEFAULT,
                                   state->cancellable,
                                   assets_enumerator_cb,
                                   engine);

  engine->asset_enumerators = g_list_prepend (engine->asset_enumerators, state);
}

#else /* USE_ASYNC_IO */

static void
enumerate_dir_for_assets (RigEngine *engine,
                          GFile *file)
{
  GFileEnumerator *enumerator;
  GError *error = NULL;
  GFileInfo *file_info;

  enumerator = g_file_enumerate_children (file,
                                          "standard::*",
                                          G_FILE_QUERY_INFO_NONE,
                                          NULL,
                                          &error);
  if (!enumerator)
    {
      char *path = g_file_get_path (file);
      g_warning ("Failed to enumerator assets dir %s: %s",
                 path, error->message);
      g_free (path);
      g_error_free (error);
      return;
    }

  while ((file_info = g_file_enumerator_next_file (enumerator,
                                                   NULL,
                                                   &error)))
    {
      enumerate_file_info (engine, file, file_info);
    }

  g_object_unref (enumerator);
}
#endif /* USE_ASYNC_IO */

static void
rig_load_asset_list (RigEngine *engine)
{
  GFile *assets_dir = g_file_new_for_path (engine->ctx->assets_location);

  enumerate_dir_for_assets (engine, assets_dir);

  rut_object_ref (engine->nine_slice_builtin_asset);
  engine->edit_mode_ui->assets =
    g_list_prepend (engine->edit_mode_ui->assets,
                    engine->nine_slice_builtin_asset);

  rut_object_ref (engine->diamond_builtin_asset);
  engine->edit_mode_ui->assets =
    g_list_prepend (engine->edit_mode_ui->assets,
                    engine->diamond_builtin_asset);

  rut_object_ref (engine->circle_builtin_asset);
  engine->edit_mode_ui->assets =
    g_list_prepend (engine->edit_mode_ui->assets,
                    engine->circle_builtin_asset);

  rut_object_ref (engine->pointalism_grid_builtin_asset);
  engine->edit_mode_ui->assets =
    g_list_prepend (engine->edit_mode_ui->assets,
                    engine->pointalism_grid_builtin_asset);

  rut_object_ref (engine->text_builtin_asset);
  engine->edit_mode_ui->assets =
    g_list_prepend (engine->edit_mode_ui->assets,
                    engine->text_builtin_asset);

  rut_object_ref (engine->hair_builtin_asset);
  engine->edit_mode_ui->assets =
    g_list_prepend (engine->edit_mode_ui->assets,
                    engine->hair_builtin_asset);

  rut_object_ref (engine->button_input_builtin_asset);
  engine->edit_mode_ui->assets =
    g_list_prepend (engine->edit_mode_ui->assets,
                    engine->button_input_builtin_asset);

  g_object_unref (assets_dir);

  rig_run_search (engine);
}

/* These should be sorted in descending order of size to
 * avoid gaps due to attributes being naturally aligned. */
static RutPLYAttribute ply_attributes[] =
{
  {
    .name = "cogl_position_in",
    .properties = {
      { "x" },
      { "y" },
      { "z" },
    },
    .n_properties = 3,
    .min_components = 1,
  },
  {
    .name = "cogl_normal_in",
    .properties = {
      { "nx" },
      { "ny" },
      { "nz" },
    },
    .n_properties = 3,
    .min_components = 3,
    .pad_n_components = 3,
    .pad_type = RUT_ATTRIBUTE_TYPE_FLOAT,
  },
  {
    .name = "cogl_tex_coord0_in",
    .properties = {
      { "s" },
      { "t" },
      { "r" },
    },
    .n_properties = 3,
    .min_components = 2,
    .pad_n_components = 3,
    .pad_type = RUT_ATTRIBUTE_TYPE_FLOAT,
  },
  {
    .name = "tangent_in",
    .properties = {
      { "tanx" },
      { "tany" },
      { "tanz" }
    },
    .n_properties = 3,
    .min_components = 3,
    .pad_n_components = 3,
    .pad_type = RUT_ATTRIBUTE_TYPE_FLOAT,
  },
  {
    .name = "cogl_color_in",
    .properties = {
      { "red" },
      { "green" },
      { "blue" },
      { "alpha" }
    },
    .n_properties = 4,
    .normalized = TRUE,
    .min_components = 3,
  }
};

static void
add_light_handle (RigEngine *engine, RigUI *ui)
{
  //RigCamera *camera =
  //  rig_entity_get_component (ui->light, RUT_COMPONENT_TYPE_CAMERA);
  RutPLYAttributeStatus padding_status[G_N_ELEMENTS (ply_attributes)];
  char *full_path = rut_find_data_file ("light.ply");
  GError *error = NULL;
  RutMesh *mesh;

  if (full_path == NULL)
    g_critical ("could not find model \"light.ply\"");

  mesh = rut_mesh_new_from_ply (engine->ctx,
                                full_path,
                                ply_attributes,
                                G_N_ELEMENTS (ply_attributes),
                                padding_status,
                                &error);
  if (mesh)
    {
      RigModel *model = rig_model_new_from_asset_mesh (engine->ctx, mesh,
                                                       FALSE, FALSE);
      RigMaterial *material = rig_material_new (engine->ctx, NULL);

      engine->light_handle = rig_entity_new (engine->ctx);
      rig_entity_set_label (engine->light_handle, "rig:light_handle");
      rig_entity_set_scale (engine->light_handle, 100);
      rut_graphable_add_child (ui->light, engine->light_handle);

      rig_entity_add_component (engine->light_handle, model);

      rig_entity_add_component (engine->light_handle, material);
      rig_material_set_receive_shadow (material, false);
      rig_material_set_cast_shadow (material, false);

      rut_object_unref (model);
      rut_object_unref (material);
    }
  else
    g_critical ("could not load model %s: %s", full_path, error->message);

  g_free (full_path);
}

static void
add_play_camera_handle (RigEngine *engine, RigUI *ui)
{
  RutPLYAttributeStatus padding_status[G_N_ELEMENTS (ply_attributes)];
  char *model_path;
  RutMesh *mesh;
  GError *error = NULL;

  model_path = rut_find_data_file ("camera-model.ply");
  if (model_path == NULL)
    {
      g_error ("could not find model \"camera-model.ply\"");
      return;
    }

  mesh = rut_mesh_new_from_ply (engine->ctx,
                                model_path,
                                ply_attributes,
                                G_N_ELEMENTS (ply_attributes),
                                padding_status,
                                &error);
  if (mesh == NULL)
    {
      g_critical ("could not load model %s: %s",
                  model_path,
                  error->message);
      g_clear_error (&error);
    }
  else
    {
      /* XXX: we'd like to show a model for the camera that
       * can be used as a handle to select the camera in the
       * editor but for the camera model tends to get in the
       * way of editing so it's been disable for now */
#if 0
      RigModel *model = rig_model_new_from_mesh (engine->ctx, mesh);
      RigMaterial *material = rig_material_new (engine->ctx, NULL);

      engine->play_camera_handle = rig_entity_new (engine->ctx);
      rig_entity_set_label (engine->play_camera_handle,
                            "rig:play_camera_handle");

      rig_entity_add_component (engine->play_camera_handle,
                                model);

      rig_entity_add_component (engine->play_camera_handle,
                                material);
      rig_material_set_receive_shadow (material, false);
      rig_material_set_cast_shadow (material, FALSE);
      rut_graphable_add_child (engine->play_camera,
                               engine->play_camera_handle);

      rut_object_unref (model);
      rut_object_unref (material);
      rut_object_unref (mesh);
#endif
    }
}

static void
on_ui_load_cb (void *user_data)
{
  RigEditor *editor = user_data;
  RigEngine *engine = editor->engine;
  RigUI *ui = engine->edit_mode_ui;

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

  rig_engine_op_apply_context_set_ui (&editor->apply_op_ctx, ui);

  /* Whenever we replace the edit mode graph that implies we need
   * to scrap and update the play mode graph, with a snapshot of
   * the new edit mode graph.
   */
  reset_play_mode_ui (editor);
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

  rig_frontend_reload_simulator_ui (frontend,
                                    engine->edit_mode_ui,
                                    false);

  /* Whenever we connect to the simulator that implies we need to
   * update the play mode graph, with a snapshot of the edit mode
   * graph.
   */
  reset_play_mode_ui (editor);
}

static RigNineSlice *
load_gradient_image (RutContext *ctx,
                     const char *filename)
{
  GError *error = NULL;
  CoglTexture *gradient =
    rut_load_texture_from_data_file (ctx,
                                     filename,
                                     &error);
  if (gradient)
    {
      return rig_nine_slice_new (ctx,
                                 gradient,
                                 0, 0, 0, 0, 0, 0);
    }
  else
    {
      g_error ("Failed to load gradient %s: %s", filename, error->message);
      g_error_free (error);
      return NULL;
    }
}

static void
connect_pressed_cb (RutIconButton *button,
                    void *user_data)
{
  RigEngine *engine = user_data;
  GList *l;

  for (l = engine->slave_addresses; l; l = l->next)
    rig_connect_to_slave (engine, l->data);
}

static void
create_top_bar (RigEngine *engine)
{
  RutStack *top_bar_stack = rut_stack_new (engine->ctx, 123, 0);
  RutIconButton *connect_button =
    rut_icon_button_new (engine->ctx,
                         NULL,
                         RUT_ICON_BUTTON_POSITION_BELOW,
                         "connect.png",
                         "connect.png",
                         "connect-white.png",
                         "connect.png");
  RutIcon *icon = rut_icon_new (engine->ctx, "settings-icon.png");
  RigNineSlice *gradient =
    load_gradient_image (engine->ctx, "top-bar-gradient.png");

  rut_box_layout_add (engine->top_vbox, FALSE, top_bar_stack);

  rut_stack_add (top_bar_stack, gradient);
  rut_object_unref (gradient);

  engine->top_bar_hbox =
    rut_box_layout_new (engine->ctx, RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);
  engine->top_bar_hbox_ltr =
    rut_box_layout_new (engine->ctx, RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);
  rut_box_layout_add (engine->top_bar_hbox, TRUE, engine->top_bar_hbox_ltr);

  engine->top_bar_hbox_rtl =
    rut_box_layout_new (engine->ctx, RUT_BOX_LAYOUT_PACKING_RIGHT_TO_LEFT);
  rut_box_layout_add (engine->top_bar_hbox, TRUE, engine->top_bar_hbox_rtl);

  rut_box_layout_add (engine->top_bar_hbox_rtl, FALSE, icon);

  rut_stack_add (top_bar_stack, engine->top_bar_hbox);

  rut_icon_button_add_on_click_callback (connect_button,
                                         connect_pressed_cb,
                                         engine,
                                         NULL); /* destroy callback */
  rut_box_layout_add (engine->top_bar_hbox_ltr, FALSE, connect_button);
  rut_object_unref (connect_button);
}
static void
create_camera_view (RigEngine *engine)
{
  RutStack *stack = rut_stack_new (engine->ctx, 0, 0);
  RutBin *bin = rut_bin_new (engine->ctx);
  RigNineSlice *gradient =
    load_gradient_image (engine->ctx, "document-bg-gradient.png");
  CoglTexture *left_drop_shadow;
  CoglTexture *bottom_drop_shadow;
  RutBoxLayout *hbox = rut_box_layout_new (engine->ctx,
                                           RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);
  RutBoxLayout *vbox = rut_box_layout_new (engine->ctx,
                                           RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM);
  RigNineSlice *left_drop;
  RutStack *left_stack;
  RutBin *left_shim;
  RigNineSlice *bottom_drop;
  RutStack *bottom_stack;
  RutBin *bottom_shim;


  rut_stack_add (stack, gradient);
  rut_stack_add (stack, bin);

  engine->main_camera_view = rig_camera_view_new (engine);

  left_drop_shadow =
    rut_load_texture_from_data_file (engine->ctx,
                                     "left-drop-shadow.png",
                                     NULL);
  bottom_drop_shadow =
    rut_load_texture_from_data_file (engine->ctx,
                                     "bottom-drop-shadow.png",
                                     NULL);

      /* Instead of creating one big drop-shadow that extends
       * underneath the document we simply create a thin drop
       * shadow for the left and bottom where the shadow is
       * actually visible...
       */

  left_drop = rig_nine_slice_new (engine->ctx,
                                  left_drop_shadow,
                                  10 /* top */,
                                  0, /* right */
                                  10, /* bottom */
                                  0, /* left */
                                  0, 0);
  left_stack = rut_stack_new (engine->ctx, 0, 0);
  left_shim = rut_bin_new (engine->ctx);
  bottom_drop = rig_nine_slice_new (engine->ctx,
                                    bottom_drop_shadow,
                                    0, 10, 0, 0, 0, 0);
  bottom_stack = rut_stack_new (engine->ctx, 0, 0);
  bottom_shim = rut_bin_new (engine->ctx);

  rut_bin_set_left_padding (left_shim, 10);
  rut_bin_set_bottom_padding (bottom_shim, 10);

  rut_bin_set_child (bin, hbox);
  rut_box_layout_add (hbox, FALSE, left_stack);

  rut_stack_add (left_stack, left_shim);
  rut_stack_add (left_stack, left_drop);

  rut_box_layout_add (hbox, TRUE, vbox);
  rut_box_layout_add (vbox, TRUE, engine->main_camera_view);
  rut_box_layout_add (vbox, FALSE, bottom_stack);

  rut_stack_add (bottom_stack, bottom_shim);
  rut_stack_add (bottom_stack, bottom_drop);

  rut_bin_set_top_padding (bin, 5);

  rut_box_layout_add (engine->asset_panel_hbox, TRUE, stack);

  rut_object_unref (bottom_shim);
  rut_object_unref (bottom_stack);
  rut_object_unref (bottom_drop);

  rut_object_unref (left_shim);
  rut_object_unref (left_stack);
  rut_object_unref (left_drop);

  cogl_object_unref (bottom_drop_shadow);
  cogl_object_unref (left_drop_shadow);

  rut_object_unref (vbox);
  rut_object_unref (hbox);
  rut_object_unref (gradient);
  rut_object_unref (bin);
  rut_object_unref (stack);
}

static void
tool_changed_cb (RutIconToggleSet *toggle_set,
                 int selection,
                 void *user_data)
{
  RigEngine *engine = user_data;
  rut_closure_list_invoke (&engine->tool_changed_cb_list,
                           RigToolChangedCallback,
                           user_data,
                           selection);
}

void
rig_add_tool_changed_callback (RigEngine *engine,
                               RigToolChangedCallback callback,
                               void *user_data,
                               RutClosureDestroyCallback destroy_notify)
{
  rut_closure_list_add (&engine->tool_changed_cb_list,
                        callback,
                        user_data,
                        destroy_notify);
}

static void
create_toolbar (RigEngine *engine)
{
  RutStack *stack = rut_stack_new (engine->ctx, 0, 0);
  RigNineSlice *gradient = load_gradient_image (engine->ctx, "toolbar-bg-gradient.png");
  RutIcon *icon = rut_icon_new (engine->ctx, "chevron-icon.png");
  RutBin *bin = rut_bin_new (engine->ctx);
  RutIconToggle *pointer_toggle;
  RutIconToggle *rotate_toggle;
  RutIconToggleSet *toggle_set;

  rut_stack_add (stack, gradient);
  rut_object_unref (gradient);

  engine->toolbar_vbox = rut_box_layout_new (engine->ctx,
                                           RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM);
  rut_bin_set_child (bin, engine->toolbar_vbox);

  rut_bin_set_left_padding (bin, 5);
  rut_bin_set_right_padding (bin, 5);
  rut_bin_set_top_padding (bin, 5);

  rut_box_layout_add (engine->toolbar_vbox, FALSE, icon);

  pointer_toggle = rut_icon_toggle_new (engine->ctx,
                                        "pointer-white.png",
                                        "pointer.png");
  rotate_toggle = rut_icon_toggle_new (engine->ctx,
                                       "rotate-white.png",
                                       "rotate.png");
  toggle_set = rut_icon_toggle_set_new (engine->ctx,
                                        RUT_ICON_TOGGLE_SET_PACKING_TOP_TO_BOTTOM);
  rut_icon_toggle_set_add (toggle_set, pointer_toggle,
                           RIG_TOOL_ID_SELECTION);
  rut_object_unref (pointer_toggle);
  rut_icon_toggle_set_add (toggle_set, rotate_toggle,
                           RIG_TOOL_ID_ROTATION);
  rut_object_unref (rotate_toggle);

  rut_icon_toggle_set_set_selection (toggle_set, RIG_TOOL_ID_SELECTION);

  rut_icon_toggle_set_add_on_change_callback (toggle_set,
                                              tool_changed_cb,
                                              engine,
                                              NULL);  /* destroy notify */

  rut_box_layout_add (engine->toolbar_vbox, false, toggle_set);
  rut_object_unref (toggle_set);

  rut_stack_add (stack, bin);

  rut_box_layout_add (engine->top_hbox, FALSE, stack);
}

static void
create_properties_bar (RigEngine *engine)
{
  RutStack *stack0 = rut_stack_new (engine->ctx, 0, 0);
  RutStack *stack1 = rut_stack_new (engine->ctx, 0, 0);
  RutBin *bin = rut_bin_new (engine->ctx);
  RigNineSlice *gradient = load_gradient_image (engine->ctx, "document-bg-gradient.png");
  RutUIViewport *properties_vp;
  RutRectangle *bg;

  rut_stack_add (stack0, gradient);
  rut_object_unref (gradient);

  rut_bin_set_left_padding (bin, 10);
  rut_bin_set_right_padding (bin, 5);
  rut_bin_set_bottom_padding (bin, 10);
  rut_bin_set_top_padding (bin, 5);
  rut_stack_add (stack0, bin);
  rut_object_unref (bin);

  rut_bin_set_child (bin, stack1);

  bg = rut_rectangle_new4f (engine->ctx,
                            0, 0, /* size */
                            1, 1, 1, 1);
  rut_stack_add (stack1, bg);
  rut_object_unref (bg);

  properties_vp = rut_ui_viewport_new (engine->ctx, 0, 0);
  engine->properties_vp = properties_vp;

  rut_stack_add (stack1, properties_vp);
  rut_object_unref (properties_vp);

  rut_ui_viewport_set_x_pannable (properties_vp, FALSE);
  rut_ui_viewport_set_y_pannable (properties_vp, TRUE);

  engine->inspector_bin = rut_bin_new (engine->ctx);
  rut_ui_viewport_add (engine->properties_vp, engine->inspector_bin);

  rut_ui_viewport_set_sync_widget (properties_vp, engine->inspector_bin);

  rut_box_layout_add (engine->properties_hbox, FALSE, stack0);
  rut_object_unref (stack0);
}

typedef struct _SearchToggleState
{
  RigEngine *engine;
  char *required_tag;
} SearchToggleState;

static void
asset_search_toggle_cb (RutIconToggle *toggle,
                        bool state,
                        void *user_data)
{
  SearchToggleState *toggle_state = user_data;
  RigEngine *engine = toggle_state->engine;

  if (state)
    {
      engine->required_search_tags =
        g_list_prepend (engine->required_search_tags,
                        toggle_state->required_tag);
    }
  else
    {
      engine->required_search_tags =
        g_list_remove (engine->required_search_tags,
                       toggle_state->required_tag);
    }

  rig_run_search (engine);
}

static void
free_search_toggle_state (void *user_data)
{
  SearchToggleState *state = user_data;

  state->engine->required_search_tags =
    g_list_remove (state->engine->required_search_tags, state->required_tag);

  g_free (state->required_tag);

  g_slice_free (SearchToggleState, state);
}

static RutIconToggle *
create_search_toggle (RigEngine *engine,
                      const char *set_icon,
                      const char *unset_icon,
                      const char *required_tag)
{
  RutIconToggle *toggle =
    rut_icon_toggle_new (engine->ctx, set_icon, unset_icon);
  SearchToggleState *state = g_slice_new0 (SearchToggleState);

  state->engine = engine;
  state->required_tag = g_strdup (required_tag);

  rut_icon_toggle_add_on_toggle_callback (toggle,
                                          asset_search_toggle_cb,
                                          state,
                                          free_search_toggle_state);

  return toggle;
}

static void
create_asset_selectors (RigEngine *engine,
                        RutStack *icons_stack)
{
  RutBoxLayout *hbox =
    rut_box_layout_new (engine->ctx, RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);
  RutIconToggle *toggle;

  toggle = create_search_toggle (engine,
                                 "geometry-white.png",
                                 "geometry.png",
                                 "geometry");
  rut_box_layout_add (hbox, FALSE, toggle);
  rut_object_unref (toggle);

  toggle = create_search_toggle (engine,
                                 "image-white.png",
                                 "image.png",
                                 "image");
  rut_box_layout_add (hbox, FALSE, toggle);
  rut_object_unref (toggle);

  toggle = create_search_toggle (engine,
                                 "video-white.png",
                                 "video.png",
                                 "video");
  rut_box_layout_add (hbox, FALSE, toggle);
  rut_object_unref (toggle);

  toggle = create_search_toggle (engine,
                                 "entity-white.png",
                                 "entity.png",
                                 "entity");
  rut_box_layout_add (hbox, FALSE, toggle);
  rut_object_unref (toggle);

  toggle = create_search_toggle (engine,
                                 "logic-white.png",
                                 "logic.png",
                                 "logic");
  rut_box_layout_add (hbox, FALSE, toggle);
  rut_object_unref (toggle);

  rut_stack_add (icons_stack, hbox);
  rut_object_unref (hbox);
}

static void
create_assets_view (RigEngine *engine)
{
  RutBoxLayout *vbox =
    rut_box_layout_new (engine->ctx, RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM);
  RutStack *search_stack = rut_stack_new (engine->ctx, 0, 0);
  RutBin *search_bin = rut_bin_new (engine->ctx);
  RutStack *icons_stack = rut_stack_new (engine->ctx, 0, 0);
  RutStack *stack = rut_stack_new (engine->ctx, 0, 0);
  RigNineSlice *gradient = load_gradient_image (engine->ctx, "toolbar-bg-gradient.png");
  RutRectangle *bg;
  RutEntry *entry;
  RutText *text;
  RutIcon *search_icon;
  CoglColor color;

  bg = rut_rectangle_new4f (engine->ctx, 0, 0, 0.2, 0.2, 0.2, 1);
  rut_stack_add (search_stack, bg);
  rut_object_unref (bg);

  entry = rut_entry_new (engine->ctx);

  text = rut_entry_get_text (entry);
  engine->search_text = text;
  rut_text_set_single_line_mode (text, TRUE);
  rut_text_set_hint_text (text, "Search...");

  search_icon = rut_icon_new (engine->ctx, "magnifying-glass.png");
  rut_entry_set_icon (entry, search_icon);

  rut_text_add_text_changed_callback (text,
                                      asset_search_update_cb,
                                      engine,
                                      NULL);

  rut_bin_set_child (search_bin, entry);
  rut_object_unref (entry);

  rut_stack_add (search_stack, search_bin);
  rut_bin_set_left_padding (search_bin, 10);
  rut_bin_set_right_padding (search_bin, 10);
  rut_bin_set_top_padding (search_bin, 2);
  rut_bin_set_bottom_padding (search_bin, 2);
  rut_object_unref (search_bin);

  rut_box_layout_add (vbox, FALSE, search_stack);
  rut_object_unref (search_stack);

  bg = rut_rectangle_new4f (engine->ctx, 0, 0, 0.57, 0.57, 0.57, 1);
  rut_stack_add (icons_stack, bg);
  rut_object_unref (bg);

  create_asset_selectors (engine, icons_stack);

  rut_box_layout_add (vbox, FALSE, icons_stack);
  rut_object_unref (icons_stack);

  rut_box_layout_add (vbox, TRUE, stack);
  rut_object_unref (stack);

  rut_stack_add (stack, gradient);
  rut_object_unref (gradient);


  engine->search_vp = rut_ui_viewport_new (engine->ctx, 0, 0);
  rut_stack_add (stack, engine->search_vp);

  engine->search_results_fold = rut_fold_new (engine->ctx, "Results");

  rut_color_init_from_uint32 (&color, 0x79b8b0ff);
  rut_fold_set_label_color (engine->search_results_fold, &color);

  rut_fold_set_font_name (engine->search_results_fold, "Bold Sans 20px");

  rut_ui_viewport_add (engine->search_vp, engine->search_results_fold);
  rut_ui_viewport_set_sync_widget (engine->search_vp, engine->search_results_fold);

  rut_ui_viewport_set_x_pannable (engine->search_vp, FALSE);

  rut_box_layout_add (engine->asset_panel_hbox, FALSE, vbox);
  rut_object_unref (vbox);
}

static void
reload_animated_inspector_properties_cb (RigControllerPropData *prop_data,
                                         void *user_data)
{
  RigEngine *engine = user_data;

  rig_reload_inspector_property (engine, prop_data->property);
}

static void
reload_animated_inspector_properties (RigEngine *engine)
{
  if (engine->inspector && engine->selected_controller)
    rig_controller_foreach_property (engine->selected_controller,
                                     reload_animated_inspector_properties_cb,
                                     engine);
}

static void
controller_progress_changed_cb (RutProperty *progress_prop,
                                void *user_data)
{
  reload_animated_inspector_properties (user_data);
}



static void
set_selected_controller (RigEngine *engine,
                         RigController *controller)
{
  if (engine->selected_controller == controller)
    return;

  if (engine->selected_controller)
    {
      rut_property_closure_destroy (engine->controller_progress_closure);
      rut_object_unref (engine->selected_controller);
    }

  engine->selected_controller = controller;

  if (controller)
    {
      rut_object_ref (controller);

      engine->controller_progress_closure =
        rut_property_connect_callback (&controller->props[RIG_CONTROLLER_PROP_PROGRESS],
                                       controller_progress_changed_cb,
                                       engine);
    }
}
static void
controller_changed_cb (RigControllerView *view,
                       RigController *controller,
                       void *user_data)
{
  RigEngine *engine = user_data;

  set_selected_controller (engine, controller);
}

static void
create_controller_view (RigEngine *engine)
{
  engine->controller_view =
    rig_controller_view_new (engine, engine->undo_journal);

  rig_controller_view_add_controller_changed_callback (engine->controller_view,
                                                       controller_changed_cb,
                                                       engine,
                                                       NULL);

  rig_split_view_set_child1 (engine->splits[0], engine->controller_view);
  rut_object_unref (engine->controller_view);
}

static void
init_resize_handle (RigEngine *engine)
{
#ifdef __APPLE__
  CoglTexture *resize_handle_texture;
  GError *error = NULL;

  resize_handle_texture =
    rut_load_texture_from_data_file (engine->ctx,
                                     "resize-handle.png",
                                     &error);

  if (resize_handle_texture == NULL)
    {
      g_warning ("Failed to load resize-handle.png: %s", error->message);
      g_error_free (error);
    }
  else
    {
      RutImage *resize_handle;

      resize_handle = rut_image_new (engine->ctx, resize_handle_texture);

      engine->resize_handle_transform =
        rut_transform_new (engine->ctx, resize_handle);

      rut_graphable_add_child (engine->root, engine->resize_handle_transform);

      rut_object_unref (engine->resize_handle_transform);
      rut_object_unref (resize_handle);
      cogl_object_unref (resize_handle_texture);
    }

#endif /* __APPLE__ */
}

static RutImage *
load_transparency_grid (RutContext *ctx)
{
  GError *error = NULL;
  CoglTexture *texture =
    rut_load_texture_from_data_file (ctx, "transparency-grid.png", &error);
  RutImage *ret;

  if (texture == NULL)
    {
      g_warning ("Failed to load transparency-grid.png: %s",
                 error->message);
      g_error_free (error);
    }
  else
    {
      ret = rut_image_new (ctx, texture);

      rut_image_set_draw_mode (ret, RUT_IMAGE_DRAW_MODE_REPEAT);
      rut_sizable_set_size (ret, 1000000.0f, 1000000.0f);

      cogl_object_unref (texture);
    }

  return ret;
}

void
rig_editor_create_ui (RigEngine *engine)
{
  engine->properties_hbox = rut_box_layout_new (engine->ctx,
                                                RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);

  /* controllers on the bottom, everything else above */
  engine->splits[0] = rig_split_view_new (engine,
                                          RIG_SPLIT_VIEW_SPLIT_HORIZONTAL,
                                          100,
                                          100);

  /* assets on the left, main area on the right */
  engine->asset_panel_hbox =
    rut_box_layout_new (engine->ctx, RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);

  create_assets_view (engine);

  create_camera_view (engine);

  create_controller_view (engine);

  rig_split_view_set_child0 (engine->splits[0], engine->asset_panel_hbox);

  rut_box_layout_add (engine->properties_hbox, TRUE, engine->splits[0]);
  create_properties_bar (engine);

  rig_split_view_set_split_fraction (engine->splits[0], 0.75);

  engine->top_vbox = rut_box_layout_new (engine->ctx,
                                         RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM);
  create_top_bar (engine);

  /* FIXME: originally I'd wanted to make this a RIGHT_TO_LEFT box
   * layout but it didn't work so I guess I guess there is a bug
   * in the box-layout allocate code. */
  engine->top_hbox = rut_box_layout_new (engine->ctx,
                                         RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);
  rut_box_layout_add (engine->top_vbox, TRUE, engine->top_hbox);

  rut_box_layout_add (engine->top_hbox, TRUE, engine->properties_hbox);
  create_toolbar (engine);

  rut_stack_add (engine->top_stack, engine->top_vbox);

  engine->transparency_grid = load_transparency_grid (engine->ctx);

  init_resize_handle (engine);
}

static Rig__Operation **
serialize_ops (RigEditor *editor, RigPBSerializer *serializer)
{
  Rig__Operation **pb_ops;
  RutQueueItem *item;
  int n_ops = editor->edit_ops->len;
  int i;

  if (!n_ops)
    return NULL;

  pb_ops = rut_memory_stack_memalign (serializer->stack,
                                      sizeof (void *) * n_ops,
                                      RUT_UTIL_ALIGNOF (void *));

  i = 0;
  rut_list_for_each (item, &editor->edit_ops->items, list_node)
    {
      pb_ops[i++] = item->data;
    }

  return pb_ops;
}

static void
handle_edit_operations (RigEditor *editor,
                        RigPBSerializer *serializer,
                        Rig__FrameSetup *pb_frame_setup)
{
  RigEngine *engine = editor->engine;
  Rig__UIEdit *play_edits;
  GList *l;

  pb_frame_setup->edit = rig_pb_new (serializer, Rig__UIEdit, rig__uiedit__init);
  pb_frame_setup->edit->n_ops = editor->edit_ops->len;
  if (pb_frame_setup->edit->n_ops)
    pb_frame_setup->edit->ops = serialize_ops (editor, serializer);

  pb_frame_setup->play_edit = NULL;

  /* XXX:
   * Edit operations are applied as they are made so we don't need to
   * apply them here.
   */

  /* Here we try and map edits into corresponding edits of the
   * play-mode ui state...
   *
   * Note: that operations that modify existing objects will refer to
   * play-mode object ids after this mapping, but operations that
   * create new objects will use the original edit-mode ids.
   *
   * This allows us to maintain a mapping from edit-mode objects
   * to new play-mode objects via the register/unregister callbacks
   * given when applying these operations to the play-mode ui
   */
  play_edits = rig_engine_copy_pb_ui_edit (&editor->copy_op_ctx,
                                           pb_frame_setup->edit);

  /* Forward both sets of edits to the simulator... */

  if (rig_engine_map_pb_ui_edit (&editor->map_op_ctx,
                                 &editor->play_apply_op_ctx,
                                 play_edits))
    {
      pb_frame_setup->play_edit = play_edits;
    }
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
    rig_slave_master_forward_pb_ui_edit (l->data, pb_frame_setup->edit);

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

static void
rig_editor_redraw (RutShell *shell, void *user_data)
{
  RigEditor *editor = user_data;
  RigEngine *engine = editor->engine;
  RigFrontend *frontend = engine->frontend;

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

  if (!frontend->ui_update_pending)
    {
      Rig__FrameSetup pb_frame_setup = RIG__FRAME_SETUP__INIT;
      RutInputQueue *input_queue = engine->simulator_input_queue;
      RigPBSerializer *serializer;
      float x, y, z;

      serializer = rig_pb_serializer_new (engine);

      pb_frame_setup.n_events = input_queue->n_events;
      pb_frame_setup.events =
        rig_pb_serialize_input_events (serializer, input_queue);

      if (frontend->has_resized)
        {
          pb_frame_setup.has_view_width = true;
          pb_frame_setup.view_width = frontend->pending_width;
          pb_frame_setup.has_view_height = true;
          pb_frame_setup.view_height = frontend->pending_height;
          frontend->has_resized = false;
        }

      handle_edit_operations (editor, serializer, &pb_frame_setup);

      /* Inform the simulator of the offset position of the main camera
       * view so that it can transform its input events accordingly...
       */
      x = y = z = 0;
      rut_graphable_fully_transform_point (engine->main_camera_view,
                                           engine->camera_2d,
                                           &x, &y, &z);
      pb_frame_setup.has_view_x = true;
      pb_frame_setup.view_x = RUT_UTIL_NEARBYINT (x);

      pb_frame_setup.has_view_y = true;
      pb_frame_setup.view_y = RUT_UTIL_NEARBYINT (y);

      if (engine->play_mode != frontend->pending_play_mode_enabled)
        {
          pb_frame_setup.has_play_mode = true;
          pb_frame_setup.play_mode = frontend->pending_play_mode_enabled;
        }

      rig_frontend_run_simulator_frame (frontend, serializer, &pb_frame_setup);

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
_rig_editor_free (RutObject *object)
{
  RigEditor *editor = object;

  rig_engine_op_apply_context_destroy (&editor->apply_op_ctx);
  rig_engine_op_copy_context_destroy (&editor->copy_op_ctx);
  rig_engine_op_map_context_destroy (&editor->map_op_ctx);
  rig_engine_op_apply_context_destroy (&editor->play_apply_op_ctx);

  rut_queue_clear (editor->edit_ops);
  rut_queue_free (editor->edit_ops);

  rut_object_unref (editor->engine);

  rut_object_unref (editor->frontend);
  rut_object_unref (editor->ctx);
  rut_object_unref (editor->shell);

  rut_object_free (RigEditor, editor);
}

RutType rig_editor_type;

static void
_rig_editor_init_type (void)
{
  rut_type_init (&rig_editor_type, "RigEditor", _rig_editor_free);
}

RigEditor *
rig_editor_new (const char *filename)
{
  RigEditor *editor = rut_object_alloc0 (RigEditor,
                                         &rig_editor_type,
                                         _rig_editor_init_type);
  char *assets_location;
  RigEngine *engine;

  editor->shell = rut_shell_new (false, /* not headless */
                                 NULL, /* shell init */
                                 NULL, /* shell fini */
                                 rig_editor_redraw,
                                 editor);

  editor->ctx = rut_context_new (editor->shell);
  rut_context_init (editor->ctx);

  editor->ui_filename = g_strdup (filename);

  assets_location = g_path_get_dirname (editor->ui_filename);
  rut_set_assets_location (editor->ctx, assets_location);
  g_free (assets_location);

  editor->edit_ops = rut_queue_new ();

  /* TODO: RigFrontend should be a trait of the engine */
  editor->frontend = rig_frontend_new (editor->shell,
                                       RIG_FRONTEND_ID_EDITOR,
                                       editor->ui_filename,
                                       false); /* start in edit mode */

  engine = editor->frontend->engine;
  editor->engine = engine;

  /* TODO: RigEditor should be a trait of the engine */
  engine->editor = editor;

  rig_frontend_set_simulator_connected_callback (editor->frontend,
                                                 simulator_connected_cb,
                                                 editor);

  rig_engine_set_apply_op_callback (engine, apply_edit_op_cb, editor);

  /* XXX: we should have a better way of handling this ui load
   * callback. Currently it's not possible to set the callback until
   * after we have created a RigFrontend which creates our RigEngine,
   * but since we pass a filename in when creating the engine we can
   * actually load a UI before we register our callback.
   */
  on_ui_load_cb (editor);
  rig_engine_set_ui_load_callback (engine, on_ui_load_cb, editor);

  rig_engine_op_apply_context_init (&editor->apply_op_ctx,
                                    engine,
                                    nop_register_id_cb,
                                    NULL, /* unregister id */
                                    editor); /* user data */

  rig_engine_op_copy_context_init (&editor->copy_op_ctx,
                                   engine);

  rig_engine_op_map_context_init (&editor->map_op_ctx,
                                  engine,
                                  map_id_cb,
                                  editor); /* user data */

  rig_engine_op_apply_context_init (&editor->play_apply_op_ctx,
                                    engine,
                                    register_play_mode_object_cb,
                                    NULL, /* unregister id */
                                    editor); /* user data */

  /* TODO move into editor */
  rig_avahi_run_browser (engine);

  rut_shell_add_input_callback (editor->shell,
                                rig_engine_input_handler,
                                engine, NULL);

  return editor;
}

void
rig_editor_load_file (RigEditor *editor,
                      const char *filename)
{
  /* FIXME: report an error to the user! */
  g_return_if_fail (editor->engine->play_mode == false);

  if (editor->ui_filename)
    g_free (editor->ui_filename);

  editor->ui_filename = g_strdup (filename);
  rig_engine_load_file (editor->engine, filename);
}

void
rig_editor_run (RigEditor *editor)
{
  rut_shell_main (editor->shell);
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
inspector_property_changed_cb (RutProperty *inspected_property,
                               RutProperty *inspector_property,
                               bool mergeable,
                               void *user_data)
{
  RigEngine *engine = user_data;
  RutBoxed new_value;

  rut_property_box (inspector_property, &new_value);

  rig_controller_view_edit_property (engine->controller_view,
                                     mergeable,
                                     inspected_property, &new_value);

  rut_boxed_destroy (&new_value);
}

static void
inspector_controlled_changed_cb (RutProperty *property,
                                 bool value,
                                 void *user_data)
{
  RigEngine *engine = user_data;

  rig_undo_journal_set_controlled (engine->undo_journal,
                                   engine->selected_controller,
                                   property,
                                   value);
}

typedef struct
{
  RigEngine *engine;
  RigInspector *inspector;
} InitControlledStateData;

static void
init_property_controlled_state_cb (RutProperty *property,
                                   void *user_data)
{
  InitControlledStateData *data = user_data;

  /* XXX: how should we handle showing whether a property is
   * controlled or not when we have multiple objects selected and the
   * property is controlled for some of them, but not all? */
  if (property->spec->animatable)
    {
      RigControllerPropData *prop_data;
      RigController *controller = data->engine->selected_controller;

      prop_data =
        rig_controller_find_prop_data_for_property (controller, property);

      if (prop_data)
        rig_inspector_set_property_controlled (data->inspector, property, TRUE);
    }
}

static RigInspector *
create_inspector (RigEngine *engine,
                  GList *objects)
{
  RutObject *reference_object = objects->data;
  RigInspector *inspector =
    rig_inspector_new (engine->ctx,
                       objects,
                       inspector_property_changed_cb,
                       inspector_controlled_changed_cb,
                       engine);

  if (rut_object_is (reference_object, RUT_TRAIT_ID_INTROSPECTABLE))
    {
      InitControlledStateData controlled_data;

      controlled_data.engine = engine;
      controlled_data.inspector = inspector;

      rut_introspectable_foreach_property (reference_object,
                                           init_property_controlled_state_cb,
                                           &controlled_data);
    }

  return inspector;
}

typedef struct _DeleteButtonState
{
  RigEngine *engine;
  GList *components;
} DeleteButtonState;

static void
free_delete_button_state (void *user_data)
{
  DeleteButtonState *state = user_data;

  g_list_free (state->components);
  g_slice_free (DeleteButtonState, user_data);
}

static void
delete_button_click_cb (RutIconButton *button, void *user_data)
{
  DeleteButtonState *state = user_data;
  GList *l;

  for (l = state->components; l; l = l->next)
    {
      rig_undo_journal_delete_component (state->engine->undo_journal,
                                         l->data);
    }

  rut_shell_queue_redraw (state->engine->ctx->shell);
}

static void
create_components_inspector (RigEngine *engine,
                             GList *components)
{
  RutComponent *reference_component = components->data;
  RigInspector *inspector = create_inspector (engine, components);
  const char *name = rut_object_get_type_name (reference_component);
  char *label;
  RutFold *fold;
  RutBin *button_bin;
  RutIconButton *delete_button;
  DeleteButtonState *button_state;

  if (strncmp (name, "Rig", 3) == 0)
    name += 3;

  label = g_strconcat (name, " Component", NULL);

  fold = rut_fold_new (engine->ctx, label);

  g_free (label);

  rut_fold_set_child (fold, inspector);
  rut_object_unref (inspector);

  button_bin = rut_bin_new (engine->ctx);
  rut_bin_set_left_padding (button_bin, 10);
  rut_fold_set_header_child (fold, button_bin);

  /* FIXME: we need better assets here so we can see a visual change
   * when the button is pressed down */
  delete_button = rut_icon_button_new (engine->ctx,
                                       NULL, /* no label */
                                       RUT_ICON_BUTTON_POSITION_BELOW,
                                       "component-delete.png", /* normal */
                                       "component-delete.png", /* hover */
                                       "component-delete.png", /* active */
                                       "component-delete.png"); /* disabled */
  button_state = g_slice_new (DeleteButtonState);
  button_state->engine = engine;
  button_state->components = g_list_copy (components);
  rut_icon_button_add_on_click_callback (delete_button,
                                         delete_button_click_cb,
                                         button_state,
                                         free_delete_button_state); /* destroy notify */
  rut_bin_set_child (button_bin, delete_button);
  rut_object_unref (delete_button);

  rut_box_layout_add (engine->inspector_box_layout, FALSE, fold);
  rut_object_unref (fold);

  engine->all_inspectors =
    g_list_prepend (engine->all_inspectors, inspector);
}

RutObject *
find_component (RigEntity *entity,
                RutComponentType type)
{
  int i;

  for (i = 0; i < entity->components->len; i++)
    {
      RutObject *component = g_ptr_array_index (entity->components, i);
      RutComponentableProps *component_props =
        rut_object_get_properties (component, RUT_TRAIT_ID_COMPONENTABLE);

      if (component_props->type == type)
        return component;
    }

  return NULL;
}

typedef struct _MatchAndListState
{
  RigEngine *engine;
  GList *entities;
} MatchAndListState;

static void
match_and_create_components_inspector_cb (RutComponent *reference_component,
                                          void *user_data)
{
  MatchAndListState *state = user_data;
  RutComponentableProps *component_props =
    rut_object_get_properties (reference_component,
                               RUT_TRAIT_ID_COMPONENTABLE);
  RutComponentType type = component_props->type;
  GList *l;
  GList *components = NULL;

  for (l = state->entities; l; l = l->next)
    {
      /* XXX: we will need to update this if we ever allow attaching
       * multiple components of the same type to an entity. */

      /* If there is no component of the same type attached to all the
       * other entities then don't list the component */
      RutComponent *component = rig_entity_get_component (l->data, type);
      if (!component)
        goto EXIT;

      /* Or of the component doesn't also have the same RutObject type
       * don't list the component */
      if (rut_object_get_type (component) !=
          rut_object_get_type (reference_component))
        goto EXIT;

      components = g_list_prepend (components, component);
    }

  if (components)
    create_components_inspector (state->engine, components);

EXIT:

  g_list_free (components);
}

/* TODO: Update to take a RigEditor */
void
rig_editor_update_inspector (RigEngine *engine)
{
  GList *objects = engine->objects_selection->objects;

  /* This will drop the last reference to any current
   * engine->inspector_box_layout and also any indirect references
   * to existing RigInspectors */
  rut_bin_set_child (engine->inspector_bin, NULL);

  engine->inspector_box_layout =
    rut_box_layout_new (engine->ctx,
                        RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM);
  rut_bin_set_child (engine->inspector_bin, engine->inspector_box_layout);

  engine->inspector = NULL;
  g_list_free (engine->all_inspectors);
  engine->all_inspectors = NULL;

  if (objects)
    {
      RutObject *reference_object = objects->data;
      MatchAndListState state;

      engine->inspector = create_inspector (engine, objects);

      rut_box_layout_add (engine->inspector_box_layout, FALSE, engine->inspector);
      engine->all_inspectors =
        g_list_prepend (engine->all_inspectors, engine->inspector);

      if (rut_object_get_type (reference_object) == &rig_entity_type)
        {
          state.engine = engine;
          state.entities = objects;

          rig_entity_foreach_component (reference_object,
                                        match_and_create_components_inspector_cb,
                                        &state);
        }
    }
}

void
rig_reload_inspector_property (RigEngine *engine,
                               RutProperty *property)
{
  if (engine->inspector)
    {
      GList *l;

      for (l = engine->all_inspectors; l; l = l->next)
        rig_inspector_reload_property (l->data, property);
    }
}

void
rig_reload_position_inspector (RigEngine *engine,
                               RigEntity *entity)
{
  if (engine->inspector)
    {
      RutProperty *property =
        rut_introspectable_lookup_property (entity, "position");

      rig_inspector_reload_property (engine->inspector, property);
    }
}

static void
_rig_objects_selection_cancel (RutObject *object)
{
  RigObjectsSelection *selection = object;
  g_list_free_full (selection->objects, (GDestroyNotify)rut_object_unref);
  selection->objects = NULL;
}

static RutObject *
_rig_objects_selection_copy (RutObject *object)
{
  RigObjectsSelection *selection = object;
  RigObjectsSelection *copy = _rig_objects_selection_new (selection->engine);
  GList *l;

  for (l = selection->objects; l; l = l->next)
    {
      if (rut_object_get_type (l->data) == &rig_entity_type)
        {
          copy->objects =
            g_list_prepend (copy->objects, rig_entity_copy (l->data));
        }
      else
        {
#warning "todo: Create a copyable interface for anything that can be selected for copy and paste"
          g_warn_if_reached ();
        }
    }

  return copy;
}

static void
_rig_objects_selection_delete (RutObject *object)
{
  RigObjectsSelection *selection = object;

  if (selection->objects)
    {
      RigEngine *engine = selection->engine;

      /* XXX: It's assumed that a selection either corresponds to
       * engine->objects_selection or to a derived selection due to
       * the selectable::copy vfunc.
       *
       * A copy should contain deep-copied entities that don't need to
       * be directly deleted with
       * rig_undo_journal_delete_entity() because they won't
       * be part of the scenegraph.
       */

      if (selection == engine->objects_selection)
        {
          GList *l, *next;
          int len = g_list_length (selection->objects);

          for (l = selection->objects; l; l = next)
            {
              next = l->next;
              rig_undo_journal_delete_entity (engine->undo_journal,
                                              l->data);
            }

          /* NB: that rig_undo_journal_delete_component() will
           * remove the entity from the scenegraph*/

          /* XXX: make sure that
           * rig_undo_journal_delete_entity () doesn't change
           * the selection, since it used to. */
          g_warn_if_fail (len == g_list_length (selection->objects));
        }

      g_list_free_full (selection->objects,
                        (GDestroyNotify)rut_object_unref);
      selection->objects = NULL;

      g_warn_if_fail (selection->objects == NULL);
    }
}

static void
_rig_objects_selection_free (void *object)
{
  RigObjectsSelection *selection = object;

  _rig_objects_selection_cancel (selection);

  rut_closure_list_disconnect_all (&selection->selection_events_cb_list);

  rut_object_free (RigObjectsSelection, selection);
}

RutType rig_objects_selection_type;

static void
_rig_objects_selection_init_type (void)
{
  static RutSelectableVTable selectable_vtable = {
      .cancel = _rig_objects_selection_cancel,
      .copy = _rig_objects_selection_copy,
      .del = _rig_objects_selection_delete,
  };
  static RutMimableVTable mimable_vtable = {
      .copy = _rig_objects_selection_copy
  };

  RutType *type = &rig_objects_selection_type;
#define TYPE RigObjectsSelection

  rut_type_init (type, G_STRINGIFY (TYPE), _rig_objects_selection_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_SELECTABLE,
                      0, /* no associated properties */
                      &selectable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_MIMABLE,
                      0, /* no associated properties */
                      &mimable_vtable);

#undef TYPE
}

RigObjectsSelection *
_rig_objects_selection_new (RigEngine *engine)
{
  RigObjectsSelection *selection =
    rut_object_alloc0 (RigObjectsSelection,
                       &rig_objects_selection_type,
                       _rig_objects_selection_init_type);

  selection->engine = engine;
  selection->objects = NULL;

  rut_list_init (&selection->selection_events_cb_list);

  return selection;
}

RutClosure *
rig_objects_selection_add_event_callback (RigObjectsSelection *selection,
                                          RigObjectsSelectionEventCallback callback,
                                          void *user_data,
                                          RutClosureDestroyCallback destroy_cb)
{
  return rut_closure_list_add (&selection->selection_events_cb_list,
                               callback,
                               user_data,
                               destroy_cb);
}

static void
remove_selection_cb (RutObject *object,
                     RigObjectsSelection *selection)
{
  rut_closure_list_invoke (&selection->selection_events_cb_list,
                           RigObjectsSelectionEventCallback,
                           selection,
                           RIG_OBJECTS_SELECTION_REMOVE_EVENT,
                           object);
  rut_object_unref (object);
}

void
rig_select_object (RigEngine *engine,
                   RutObject *object,
                   RutSelectAction action)
{
  RigObjectsSelection *selection = engine->objects_selection;

  /* For now we only support selecting multiple entities... */
  if (object && rut_object_get_type (object) != &rig_entity_type)
    action = RUT_SELECT_ACTION_REPLACE;

#if RIG_EDITOR_ENABLED
  if (object == engine->light_handle)
    object = engine->edit_mode_ui->light;
#endif

#if 0
  else if (entity == engine->play_camera_handle)
    entity = engine->play_camera;
#endif

  switch (action)
    {
    case RUT_SELECT_ACTION_REPLACE:
      {
        GList *old = selection->objects;

        selection->objects = NULL;

        g_list_foreach (old,
                        (GFunc)remove_selection_cb,
                        selection);
        g_list_free (old);

        if (object)
          {
            selection->objects = g_list_prepend (selection->objects,
                                                 rut_object_ref (object));
            rut_closure_list_invoke (&selection->selection_events_cb_list,
                                     RigObjectsSelectionEventCallback,
                                     selection,
                                     RIG_OBJECTS_SELECTION_ADD_EVENT,
                                     object);
          }
        break;
      }
    case RUT_SELECT_ACTION_TOGGLE:
      {
        GList *link = g_list_find (selection->objects, object);

        if (link)
          {
            selection->objects =
              g_list_remove_link (selection->objects, link);

            rut_closure_list_invoke (&selection->selection_events_cb_list,
                                     RigObjectsSelectionEventCallback,
                                     selection,
                                     RIG_OBJECTS_SELECTION_REMOVE_EVENT,
                                     link->data);
            rut_object_unref (link->data);
          }
        else if (object)
          {
            rut_closure_list_invoke (&selection->selection_events_cb_list,
                                     RigObjectsSelectionEventCallback,
                                     selection,
                                     RIG_OBJECTS_SELECTION_ADD_EVENT,
                                     object);

            rut_object_ref (object);
            selection->objects =
              g_list_prepend (selection->objects, object);
          }
      }
      break;
    }

  if (selection->objects)
    rut_shell_set_selection (engine->shell, engine->objects_selection);

  rut_shell_queue_redraw (engine->ctx->shell);

  if (engine->frontend)
    rig_editor_update_inspector (engine);
}

void
rig_editor_push_undo_subjournal (RigEngine *engine)
{
  RigUndoJournal *subjournal = rig_undo_journal_new (engine);

  rig_undo_journal_set_apply_on_insert (subjournal, true);

  engine->undo_journal_stack = g_list_prepend (engine->undo_journal_stack,
                                               subjournal);
  engine->undo_journal = subjournal;
}

RigUndoJournal *
rig_editor_pop_undo_subjournal (RigEngine *engine)
{
  RigUndoJournal *head_journal = engine->undo_journal;

  engine->undo_journal_stack = g_list_delete_link (engine->undo_journal_stack,
                                                   engine->undo_journal_stack);
  g_return_if_fail (engine->undo_journal_stack);

  engine->undo_journal = engine->undo_journal_stack->data;

  return head_journal;
}
