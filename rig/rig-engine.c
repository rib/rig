/*
 * Rig
 *
 * Copyright (C) 2012,2013  Intel Corporation.
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

#include <glib.h>
#include <gio/gio.h>
#include <sys/stat.h>
#include <math.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <glib-object.h>


#include <cogl/cogl.h>

#include <rut.h>
#include <rut-bin.h>

#include "rig-engine.h"
#include "rig-controller.h"
#include "rig-load-save.h"
#include "rig-undo-journal.h"
#include "rig-renderer.h"
#include "rig-defines.h"
#include "rig-osx.h"
#ifdef USE_GTK
#include "rig-application.h"
#include <gtk/gtk.h>
#endif /* USE_GTK */
#include "rig-split-view.h"
#include "rig-rpc-network.h"
#include "rig.pb-c.h"
#include "rig-slave-master.h"
#include "rig-frontend.h"
#include "rig-simulator.h"

//#define DEVICE_WIDTH 480.0
//#define DEVICE_HEIGHT 800.0
#define DEVICE_WIDTH 720.0
#define DEVICE_HEIGHT 1280.0

static RutPropertySpec _rig_engine_prop_specs[] = {
  {
    .name = "width",
    .flags = RUT_PROPERTY_FLAG_READABLE,
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RigEngine, window_width)
  },
  {
    .name = "height",
    .flags = RUT_PROPERTY_FLAG_READABLE,
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RigEngine, window_height)
  },
  {
    .name = "device_width",
    .flags = RUT_PROPERTY_FLAG_READABLE,
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RigEngine, device_width)
  },
  {
    .name = "device_height",
    .flags = RUT_PROPERTY_FLAG_READABLE,
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RigEngine, device_height)
  },
  { 0 }
};

static void
rig_load_asset_list (RigEngine *engine);

static RigObjectsSelection *
_rig_objects_selection_new (RigEngine *engine);

#ifdef RIG_EDITOR_ENABLED
bool _rig_in_editor_mode = FALSE;
#endif

bool _rig_in_simulator_mode = FALSE;

static RutTraverseVisitFlags
scenegraph_pre_paint_cb (RutObject *object,
                         int depth,
                         void *user_data)
{
  RutPaintContext *rut_paint_ctx = user_data;
  RutCamera *camera = rut_paint_ctx->camera;
  CoglFramebuffer *fb = rut_camera_get_framebuffer (camera);

#if 0
  if (rut_object_get_type (object) == &rut_camera_type)
    {
      g_print ("%*sCamera = %p\n", depth, "", object);
      rut_camera_flush (RUT_CAMERA (object));
      return RUT_TRAVERSE_VISIT_CONTINUE;
    }
  else
#endif

  if (rut_object_get_type (object) == &rut_ui_viewport_type)
    {
      RutUIViewport *ui_viewport = RUT_UI_VIEWPORT (object);
#if 0
      g_print ("%*sPushing clip = %f %f\n",
               depth, "",
               rut_ui_viewport_get_width (ui_viewport),
               rut_ui_viewport_get_height (ui_viewport));
#endif
      cogl_framebuffer_push_rectangle_clip (fb,
                                            0, 0,
                                            rut_ui_viewport_get_width (ui_viewport),
                                            rut_ui_viewport_get_height (ui_viewport));
    }

  if (rut_object_is (object, RUT_TRAIT_ID_TRANSFORMABLE))
    {
      //g_print ("%*sTransformable = %p\n", depth, "", object);
      const CoglMatrix *matrix = rut_transformable_get_matrix (object);
      //cogl_debug_matrix_print (matrix);
      cogl_framebuffer_push_matrix (fb);
      cogl_framebuffer_transform (fb, matrix);
    }

  if (rut_object_is (object, RUT_TRAIT_ID_PAINTABLE))
    {
      RutPaintableVTable *vtable =
        rut_object_get_vtable (object, RUT_TRAIT_ID_PAINTABLE);
      vtable->paint (object, rut_paint_ctx);
    }

  /* XXX:
   * How can we maintain state between the pre and post stages?  Is it
   * ok to just "sub-class" the paint context and maintain a stack of
   * state that needs to be shared with the post paint code.
   */

  return RUT_TRAVERSE_VISIT_CONTINUE;
}

static RutTraverseVisitFlags
scenegraph_post_paint_cb (RutObject *object,
                          int depth,
                          void *user_data)
{
  RutPaintContext *rut_paint_ctx = user_data;
  CoglFramebuffer *fb = rut_camera_get_framebuffer (rut_paint_ctx->camera);

#if 0
  if (rut_object_get_type (object) == &rut_camera_type)
    {
      rut_camera_end_frame (RUT_CAMERA (object));
      return RUT_TRAVERSE_VISIT_CONTINUE;
    }
  else
#endif

  if (rut_object_get_type (object) == &rut_ui_viewport_type)
    {
      cogl_framebuffer_pop_clip (fb);
    }

  if (rut_object_is (object, RUT_TRAIT_ID_TRANSFORMABLE))
    {
      cogl_framebuffer_pop_matrix (fb);
    }

  return RUT_TRAVERSE_VISIT_CONTINUE;
}

void
rig_engine_paint (RigEngine *engine)
{
  CoglFramebuffer *fb = engine->onscreen;
  RigPaintContext paint_ctx;
  RutPaintContext *rut_paint_ctx = &paint_ctx._parent;

  rut_camera_set_framebuffer (engine->camera_2d, fb);

#warning "FIXME: avoid clear overdraw between engine_paint and camera_view_paint"
  cogl_framebuffer_clear4f (fb,
                            COGL_BUFFER_BIT_COLOR|COGL_BUFFER_BIT_DEPTH,
                            0.9, 0.9, 0.9, 1);

  paint_ctx.engine = engine;
  paint_ctx.renderer = engine->renderer;

  paint_ctx.pass = RIG_PASS_COLOR_BLENDED;
  rut_paint_ctx->camera = engine->camera_2d;

  rut_camera_flush (engine->camera_2d);
  rut_paint_graph_with_layers (engine->root,
                               scenegraph_pre_paint_cb,
                               scenegraph_post_paint_cb,
                               rut_paint_ctx);
  rut_camera_end_frame (engine->camera_2d);

  cogl_onscreen_swap_buffers (COGL_ONSCREEN (fb));
}

void
rig_reload_inspector_property (RigEngine *engine,
                               RutProperty *property)
{
  if (engine->inspector)
    {
      GList *l;

      for (l = engine->all_inspectors; l; l = l->next)
        rut_inspector_reload_property (l->data, property);
    }
}

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
                                 CoglBool value,
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
  RutInspector *inspector;
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
        rut_inspector_set_property_controlled (data->inspector, property, TRUE);
    }
}

static RutInspector *
create_inspector (RigEngine *engine,
                  GList *objects)
{
  RutObject *reference_object = objects->data;
  RutInspector *inspector =
    rut_inspector_new (engine->ctx,
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
  RutInspector *inspector = create_inspector (engine, components);
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
find_component (RutEntity *entity,
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
      RutComponent *component = rut_entity_get_component (l->data, type);
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

void
_rig_engine_update_inspector (RigEngine *engine)
{
  GList *objects = engine->objects_selection->objects;

  /* This will drop the last reference to any current
   * engine->inspector_box_layout and also any indirect references
   * to existing RutInspectors */
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

      if (rut_object_get_type (reference_object) == &rut_entity_type)
        {
          state.engine = engine;
          state.entities = objects;

          rut_entity_foreach_component (reference_object,
                                        match_and_create_components_inspector_cb,
                                        &state);
        }
    }
}

void
rig_engine_dirty_properties_menu (RutImageSource *source,
                                  void *user_data)
{
#ifdef RIG_EDITOR_ENABLED
  RigEngine *engine = user_data;
  if (_rig_in_editor_mode)
    _rig_engine_update_inspector (engine);
#endif
}

void
rig_reload_position_inspector (RigEngine *engine,
                               RutEntity *entity)
{
  if (engine->inspector)
    {
      RutProperty *property =
        rut_introspectable_lookup_property (entity, "position");

      rut_inspector_reload_property (engine->inspector, property);
    }
}

static void
rig_engine_set_current_ui (RigEngine *engine,
                           RigUI *ui)
{
  rig_camera_view_set_ui (engine->main_camera_view, ui);
  engine->current_ui = ui;
  rut_shell_queue_redraw (engine->ctx->shell);
}

void
rig_engine_set_play_mode_enabled (RigEngine *engine, bool enabled)
{
  engine->play_mode = enabled;

  if (engine->play_mode)
    {
      rig_engine_set_current_ui (engine, engine->play_mode_ui);
      rig_camera_view_set_play_mode_enabled (engine->main_camera_view,
                                             true);
    }
  else
    {
      rig_engine_set_current_ui (engine, engine->edit_mode_ui);
      rig_camera_view_set_play_mode_enabled (engine->main_camera_view,
                                             false);
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
      if (rut_object_get_type (l->data) == &rut_entity_type)
        {
          copy->objects =
            g_list_prepend (copy->objects, rut_entity_copy (l->data));
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

static RigObjectsSelection *
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

  /* From the simulator we forward select actions to the frontend
   * editor, but also do our own state tracking of what entities are
   * selected.
   */
  if (engine->simulator)
    rig_simulator_action_select_object (engine->simulator, object, action);

  /* For now we only support selecting multiple entities... */
  if (object && rut_object_get_type (object) != &rut_entity_type)
    action = RUT_SELECT_ACTION_REPLACE;

  if (object == engine->light_handle)
    object = engine->edit_mode_ui->light;

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
    _rig_engine_update_inspector (engine);
}

static void
allocate (RigEngine *engine)
{
  //engine->main_width = engine->window_width - engine->left_bar_width - engine->right_bar_width;
  //engine->main_height = engine->window_height - engine->top_bar_height - engine->bottom_bar_height;

  rut_sizable_set_size (engine->top_stack, engine->window_width, engine->window_height);

#ifdef RIG_EDITOR_ENABLED
  if (_rig_in_editor_mode)
    {
      if (engine->resize_handle_transform)
        {
          RutTransform *transform = engine->resize_handle_transform;

          rut_transform_init_identity (transform);
          rut_transform_translate (transform,
                                   engine->window_width - 18.0f,
                                   engine->window_height - 18.0f,
                                   0.0f);
        }
    }
#endif

  /* Update the window camera */
  rut_camera_set_projection_mode (engine->camera_2d, RUT_PROJECTION_ORTHOGRAPHIC);
  rut_camera_set_orthographic_coordinates (engine->camera_2d,
                                           0, 0, engine->window_width, engine->window_height);
  rut_camera_set_near_plane (engine->camera_2d, -1);
  rut_camera_set_far_plane (engine->camera_2d, 100);

  rut_camera_set_viewport (engine->camera_2d, 0, 0, engine->window_width, engine->window_height);
}

void
rig_engine_resize (RigEngine *engine,
                   int width,
                   int height)
{
  engine->window_width = width;
  engine->window_height = height;

  rut_property_dirty (&engine->ctx->property_ctx, &engine->properties[RIG_ENGINE_PROP_WIDTH]);
  rut_property_dirty (&engine->ctx->property_ctx, &engine->properties[RIG_ENGINE_PROP_HEIGHT]);

  allocate (engine);
}

static void
engine_onscreen_resize (CoglOnscreen *onscreen,
                        int width,
                        int height,
                        void *user_data)
{
  RigEngine *engine = user_data;

  g_return_if_fail (!_rig_in_simulator_mode);

  rig_engine_resize (engine, width, height);
}

typedef struct _ResultInputClosure
{
  RutObject *result;
  RigEngine *engine;
} ResultInputClosure;

static void
free_result_input_closures (RigEngine *engine)
{
  GList *l;

  for (l = engine->result_input_closures; l; l = l->next)
    g_slice_free (ResultInputClosure, l->data);
  g_list_free (engine->result_input_closures);
  engine->result_input_closures = NULL;
}

static void
apply_asset_input_with_entity (RigEngine *engine,
                               RutAsset *asset,
                               RutEntity *entity)
{
  RigUndoJournal *sub_journal;
  RutAssetType type = rut_asset_get_type (asset);
  RutMaterial *material;
  RutObject *geom;

  rig_engine_push_undo_subjournal (engine);

  switch (type)
    {
    case RUT_ASSET_TYPE_TEXTURE:
    case RUT_ASSET_TYPE_NORMAL_MAP:
    case RUT_ASSET_TYPE_ALPHA_MASK:
        {
          material =
            rut_entity_get_component (entity, RUT_COMPONENT_TYPE_MATERIAL);

          if (!material)
            {
              material = rut_material_new (engine->ctx, asset);
              rig_undo_journal_add_component (engine->undo_journal,
                                              entity, material);
            }

          if (type == RUT_ASSET_TYPE_TEXTURE)
            rut_material_set_color_source_asset (material, asset);
          else if (type == RUT_ASSET_TYPE_NORMAL_MAP)
            rut_material_set_normal_map_asset (material, asset);
          else if (type == RUT_ASSET_TYPE_ALPHA_MASK)
            rut_material_set_alpha_mask_asset (material, asset);

          rut_renderer_notify_entity_changed (engine->renderer, entity);

          geom = rut_entity_get_component (entity,
                                           RUT_COMPONENT_TYPE_GEOMETRY);
          if (!geom)
            {
              RutShape *shape = rut_shape_new (engine->ctx, TRUE, 0, 0);
              rig_undo_journal_add_component (engine->undo_journal,
                                              entity, shape);
              geom = shape;
            }

          break;
        }
    case RUT_ASSET_TYPE_PLY_MODEL:
        {
          RutModel *model;
          float x_range, y_range, z_range, max_range;

          material =
            rut_entity_get_component (entity, RUT_COMPONENT_TYPE_MATERIAL);

          if (!material)
            {
              material = rut_material_new (engine->ctx, asset);
              rig_undo_journal_add_component (engine->undo_journal,
                                              entity, material);
            }

          geom = rut_entity_get_component (entity,
                                           RUT_COMPONENT_TYPE_GEOMETRY);

          if (geom && rut_object_get_type (geom) == &rut_model_type)
            {
              model = geom;
              if (model == rut_asset_get_model (asset))
                break;
              else
                rig_undo_journal_delete_component (engine->undo_journal, model);
            }
          else if (geom)
            rig_undo_journal_delete_component (engine->undo_journal, geom);

          model = rut_asset_get_model (asset);
          rig_undo_journal_add_component (engine->undo_journal, entity, model);

          x_range = model->max_x - model->min_x;
          y_range = model->max_y - model->min_y;
          z_range = model->max_z - model->min_z;

          max_range = x_range;
          if (y_range > max_range)
            max_range = y_range;
          if (z_range > max_range)
            max_range = z_range;

          rut_entity_set_scale (entity, 200.0 / max_range);

          rut_renderer_notify_entity_changed (engine->renderer, entity);

          break;
        }
    case RUT_ASSET_TYPE_BUILTIN:
      if (asset == engine->text_builtin_asset)
        {
          RutText *text;
          CoglColor color;
          RutHair *hair;

          hair = rut_entity_get_component (entity,
                                           RUT_COMPONENT_TYPE_HAIR);

          if (hair)
            rig_undo_journal_delete_component (engine->undo_journal, hair);

          geom = rut_entity_get_component (entity,
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
          RutShape *shape;
          int tex_width = 200, tex_height = 200;

          geom = rut_entity_get_component (entity,
                                           RUT_COMPONENT_TYPE_GEOMETRY);

          if (geom && rut_object_get_type (geom) == &rut_shape_type)
            break;
          else if (geom)
            rig_undo_journal_delete_component (engine->undo_journal, geom);

          material =
            rut_entity_get_component (entity, RUT_COMPONENT_TYPE_MATERIAL);

          if (material)
            {
              RutAsset *texture_asset =
                rut_material_get_color_source_asset (material);
              if (texture_asset)
                {
                  if (rut_asset_get_is_video (texture_asset))
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
                        rut_asset_get_texture (texture_asset);
                      tex_width = cogl_texture_get_width (texture);
                      tex_height = cogl_texture_get_height (texture);
                    }
                }
            }

          shape = rut_shape_new (engine->ctx, TRUE, tex_width,
                                 tex_height);
          rig_undo_journal_add_component (engine->undo_journal,
                                          entity, shape);

          rut_renderer_notify_entity_changed (engine->renderer, entity);
        }
      else if (asset == engine->diamond_builtin_asset)
        {
          RutDiamond *diamond;
          int tex_width = 200, tex_height = 200;

          geom = rut_entity_get_component (entity,
                                           RUT_COMPONENT_TYPE_GEOMETRY);

          if (geom && rut_object_get_type (geom) == &rut_diamond_type)
            break;
          else if (geom)
            rig_undo_journal_delete_component (engine->undo_journal, geom);

          material =
            rut_entity_get_component (entity,
                                      RUT_COMPONENT_TYPE_MATERIAL);

          if (material)
            {
              RutAsset *texture_asset =
                rut_material_get_color_source_asset (material);
              if (texture_asset)
                {
                  if (rut_asset_get_is_video (texture_asset))
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
                        rut_asset_get_texture (texture_asset);
                      tex_width = cogl_texture_get_width (texture);
                      tex_height = cogl_texture_get_height (texture);
                    }
                }
            }

          diamond = rut_diamond_new (engine->ctx, 200, tex_width,
                                     tex_height);
          rig_undo_journal_add_component (engine->undo_journal,
                                          entity, diamond);

          rut_renderer_notify_entity_changed (engine->renderer, entity);
        }
      else if (asset == engine->nine_slice_builtin_asset)
        {
          RutNineSlice *nine_slice;
          int tex_width = 200, tex_height = 200;

          geom = rut_entity_get_component (entity,
                                           RUT_COMPONENT_TYPE_GEOMETRY);

          if (geom && rut_object_get_type (geom) == &rut_nine_slice_type)
            break;
          else if (geom)
            rig_undo_journal_delete_component (engine->undo_journal, geom);

          material =
            rut_entity_get_component (entity,
                                      RUT_COMPONENT_TYPE_MATERIAL);

          if (material)
            {
              RutAsset *color_source_asset =
                rut_material_get_color_source_asset (material);
              if (color_source_asset)
                {
                  if (rut_asset_get_is_video (color_source_asset))
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
                        rut_asset_get_texture (color_source_asset);
                      tex_width = cogl_texture_get_width (texture);
                      tex_height = cogl_texture_get_height (texture);
                    }
                }
            }

          nine_slice = rut_nine_slice_new (engine->ctx, NULL,
                                           0, 0, 0, 0,
                                           tex_width, tex_height);
          rig_undo_journal_add_component (engine->undo_journal,
                                          entity, nine_slice);

          rut_renderer_notify_entity_changed (engine->renderer, entity);
        }
      else if (asset == engine->pointalism_grid_builtin_asset)
        {
          RutPointalismGrid *grid;
          int tex_width = 200, tex_height = 200;

          geom = rut_entity_get_component (entity,
                                           RUT_COMPONENT_TYPE_GEOMETRY);

          if (geom && rut_object_get_type (geom) ==
              &rut_pointalism_grid_type)
            {
              break;
            }
          else if (geom)
            rig_undo_journal_delete_component (engine->undo_journal, geom);

          material =
            rut_entity_get_component (entity,
                                      RUT_COMPONENT_TYPE_MATERIAL);

          if (material)
            {
              RutAsset *texture_asset =
                rut_material_get_color_source_asset (material);
              if (texture_asset)
                {
                  if (rut_asset_get_is_video (texture_asset))
                    {
                      tex_width = 640;
                      tex_height = 480;
                    }
                  else
                    {
                      CoglTexture *texture =
                        rut_asset_get_texture (texture_asset);
                      tex_width = cogl_texture_get_width (texture);
                      tex_height = cogl_texture_get_height (texture);
                    }
                }
            }

          grid = rut_pointalism_grid_new (engine->ctx, 20, tex_width,
                                          tex_height);

          rig_undo_journal_add_component (engine->undo_journal, entity, grid);

          rut_renderer_notify_entity_changed (engine->renderer, entity);
        }
      else if (asset == engine->hair_builtin_asset)
        {
          RutHair *hair = rut_entity_get_component (entity,
                                                    RUT_COMPONENT_TYPE_HAIR);
          if (hair)
            break;

          hair = rut_hair_new (engine->ctx);
          rig_undo_journal_add_component (engine->undo_journal, entity, hair);
          geom = rut_entity_get_component (entity,
                                           RUT_COMPONENT_TYPE_GEOMETRY);

          if (geom && rut_object_get_type (geom) == &rut_model_type)
            {
              RutModel *hair_geom = rut_model_new_for_hair (geom);

              rut_hair_set_length (hair,
                                   rut_model_get_default_hair_length (hair_geom));

              rig_undo_journal_delete_component (engine->undo_journal, geom);
              rig_undo_journal_add_component (engine->undo_journal,
                                              entity, hair_geom);
            }

          rut_renderer_notify_entity_changed (engine->renderer, entity);
        }
      else if (asset == engine->button_input_builtin_asset)
        {
          RutButtonInput *button_input =
            rut_entity_get_component (entity, RUT_COMPONENT_TYPE_INPUT);
          if (button_input)
            break;

          button_input = rut_button_input_new (engine->ctx);
          rig_undo_journal_add_component (engine->undo_journal,
                                          entity, button_input);

          rut_renderer_notify_entity_changed (engine->renderer, entity);
        }
      break;
    }

  sub_journal = rig_engine_pop_undo_subjournal (engine);

  if (rig_undo_journal_is_empty (sub_journal))
    rig_undo_journal_free (sub_journal);
  else
    rig_undo_journal_log_subjournal (engine->undo_journal, sub_journal);
}

static void
apply_result_input_with_entity (RutEntity *entity,
                                ResultInputClosure *closure)
{
  if (rut_object_get_type (closure->result) == &rut_asset_type)
    apply_asset_input_with_entity (closure->engine,
                                   closure->result,
                                   entity);
  else if (rut_object_get_type (closure->result) == &rut_entity_type)
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
              RutEntity *entity = rut_entity_new (engine->ctx);
              rig_undo_journal_add_entity (engine->undo_journal,
                                           engine->edit_mode_ui->scene,
                                           entity);
              rig_select_object (engine, entity, RUT_SELECT_ACTION_REPLACE);
              apply_result_input_with_entity (entity, closure);
            }

          _rig_engine_update_inspector (engine);
          rut_shell_queue_redraw (engine->ctx->shell);
          status = RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

  return status;
}

static CoglBool
asset_matches_search (RigEngine *engine,
                      RutAsset *asset,
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
      if (rut_asset_has_tag (asset, l->data))
        {
          found = true;
          break;
        }
    }

  if (engine->required_search_tags && found == false)
    return FALSE;

  if (!search)
    return TRUE;

  inferred_tags = rut_asset_get_inferred_tags (asset);
  tags = g_strsplit_set (search, " \t", 0);

  path = rut_asset_get_path (asset);
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

  if (rut_object_get_type (result) == &rut_asset_type)
    {
      RutAsset *asset = result;

      texture = rut_asset_get_texture (asset);

      if (texture)
        {
          RutImage *image = rut_image_new (engine->ctx, texture);
          rut_stack_add (stack, image);
          rut_object_unref (image);
        }
      else
        {
          char *basename = g_path_get_basename (rut_asset_get_path (asset));
          RutText *text = rut_text_new_with_text (engine->ctx, NULL, basename);
          rut_stack_add (stack, text);
          rut_object_unref (text);
          g_free (basename);
        }
    }
  else if (rut_object_get_type (result) == &rut_entity_type)
    {
      RutEntity *entity = result;
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

  if (rut_object_get_type (result) == &rut_asset_type)
    {
      RutAsset *asset = result;

      if (rut_asset_has_tag (asset, "geometry"))
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
      else if (rut_asset_has_tag (asset, "image"))
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
      else if (rut_asset_has_tag (asset, "video"))
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
  else if (rut_object_get_type (result) == &rut_entity_type)
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

static void
clear_search_results (RigEngine *engine)
{
  if (engine->search_results_vbox)
    {
      rut_fold_set_child (engine->search_results_fold, NULL);
      free_result_input_closures (engine);

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
  if (rut_object_get_type (object) == &rut_entity_type)
    {
      RutEntity *entity = object;
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

  clear_search_results (engine);

  engine->search_results_vbox =
    rut_box_layout_new (engine->ctx, RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM);
  rut_fold_set_child (engine->search_results_fold,
                      engine->search_results_vbox);
  rut_object_unref (engine->search_results_vbox);

  for (l = engine->edit_mode_ui->assets, i= 0; l; l = l->next, i++)
    {
      RutAsset *asset = l->data;

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

static void
rig_refresh_thumbnails (RutAsset *video,
                        void *user_data)
{
  rig_run_search (user_data);
}

static void
asset_search_update_cb (RutText *text,
                        void *user_data)
{
  rig_run_search (user_data);
}


#ifdef RIG_EDITOR_ENABLED

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

#endif /* RIG_EDITOR_ENABLED */

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

#ifdef RIG_EDITOR_ENABLED

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

RutNineSlice *
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
      return rut_nine_slice_new (ctx,
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

void
connect_pressed_cb (RutIconButton *button,
                    void *user_data)
{
  RigEngine *engine = user_data;
  GList *l;

  for (l = engine->slave_addresses; l; l = l->next)
    rig_connect_to_slave (engine, l->data);
}

static void
load_builtin_assets (RigEngine *engine)
{

  engine->nine_slice_builtin_asset = rut_asset_new_builtin (engine->ctx, "nine-slice.png");
  rut_asset_add_inferred_tag (engine->nine_slice_builtin_asset, "nine-slice");
  rut_asset_add_inferred_tag (engine->nine_slice_builtin_asset, "builtin");
  rut_asset_add_inferred_tag (engine->nine_slice_builtin_asset, "geom");
  rut_asset_add_inferred_tag (engine->nine_slice_builtin_asset, "geometry");

  engine->diamond_builtin_asset = rut_asset_new_builtin (engine->ctx, "diamond.png");
  rut_asset_add_inferred_tag (engine->diamond_builtin_asset, "diamond");
  rut_asset_add_inferred_tag (engine->diamond_builtin_asset, "builtin");
  rut_asset_add_inferred_tag (engine->diamond_builtin_asset, "geom");
  rut_asset_add_inferred_tag (engine->diamond_builtin_asset, "geometry");

  engine->circle_builtin_asset = rut_asset_new_builtin (engine->ctx, "circle.png");
  rut_asset_add_inferred_tag (engine->circle_builtin_asset, "shape");
  rut_asset_add_inferred_tag (engine->circle_builtin_asset, "circle");
  rut_asset_add_inferred_tag (engine->circle_builtin_asset, "builtin");
  rut_asset_add_inferred_tag (engine->circle_builtin_asset, "geom");
  rut_asset_add_inferred_tag (engine->circle_builtin_asset, "geometry");

  engine->pointalism_grid_builtin_asset = rut_asset_new_builtin (engine->ctx,
                                            "pointalism.png");
  rut_asset_add_inferred_tag (engine->pointalism_grid_builtin_asset, "grid");
  rut_asset_add_inferred_tag (engine->pointalism_grid_builtin_asset,
                              "pointalism");
  rut_asset_add_inferred_tag (engine->pointalism_grid_builtin_asset, "builtin");
  rut_asset_add_inferred_tag (engine->pointalism_grid_builtin_asset, "geom");
  rut_asset_add_inferred_tag (engine->pointalism_grid_builtin_asset,
                              "geometry");

  engine->text_builtin_asset = rut_asset_new_builtin (engine->ctx, "fonts.png");
  rut_asset_add_inferred_tag (engine->text_builtin_asset, "text");
  rut_asset_add_inferred_tag (engine->text_builtin_asset, "label");
  rut_asset_add_inferred_tag (engine->text_builtin_asset, "builtin");
  rut_asset_add_inferred_tag (engine->text_builtin_asset, "geom");
  rut_asset_add_inferred_tag (engine->text_builtin_asset, "geometry");

  engine->hair_builtin_asset = rut_asset_new_builtin (engine->ctx, "hair.png");
  rut_asset_add_inferred_tag (engine->hair_builtin_asset, "hair");
  rut_asset_add_inferred_tag (engine->hair_builtin_asset, "builtin");

  engine->button_input_builtin_asset = rut_asset_new_builtin (engine->ctx,
                                                               "button.png");
  rut_asset_add_inferred_tag (engine->button_input_builtin_asset, "button");
  rut_asset_add_inferred_tag (engine->button_input_builtin_asset, "builtin");
  rut_asset_add_inferred_tag (engine->button_input_builtin_asset, "input");
}

static void
free_builtin_assets (RigEngine *engine)
{
  rut_object_unref (engine->nine_slice_builtin_asset);
  rut_object_unref (engine->diamond_builtin_asset);
  rut_object_unref (engine->circle_builtin_asset);
  rut_object_unref (engine->pointalism_grid_builtin_asset);
  rut_object_unref (engine->text_builtin_asset);
  rut_object_unref (engine->hair_builtin_asset);
  rut_object_unref (engine->button_input_builtin_asset);
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
  RutNineSlice *gradient =
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
  RutNineSlice *gradient =
    load_gradient_image (engine->ctx, "document-bg-gradient.png");
  CoglTexture *left_drop_shadow;
  CoglTexture *bottom_drop_shadow;
  RutBoxLayout *hbox = rut_box_layout_new (engine->ctx,
                                           RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);
  RutBoxLayout *vbox = rut_box_layout_new (engine->ctx,
                                           RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM);
  RutNineSlice *left_drop;
  RutStack *left_stack;
  RutBin *left_shim;
  RutNineSlice *bottom_drop;
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

  left_drop = rut_nine_slice_new (engine->ctx,
                                  left_drop_shadow,
                                  10 /* top */,
                                  0, /* right */
                                  10, /* bottom */
                                  0, /* left */
                                  0, 0);
  left_stack = rut_stack_new (engine->ctx, 0, 0);
  left_shim = rut_bin_new (engine->ctx);
  bottom_drop = rut_nine_slice_new (engine->ctx,
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
  RutNineSlice *gradient = load_gradient_image (engine->ctx, "toolbar-bg-gradient.png");
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
  RutNineSlice *gradient = load_gradient_image (engine->ctx, "document-bg-gradient.png");
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
  RutNineSlice *gradient = load_gradient_image (engine->ctx, "toolbar-bg-gradient.png");
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
#endif /* RIG_EDITOR_ENABLED */

static void
create_editor_ui (RigEngine *engine)
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

static void
create_debug_gradient (RigEngine *engine)
{
  CoglVertexP2C4 quad[] = {
        { 0, 0, 0xff, 0x00, 0x00, 0xff },
        { 0, 200, 0x00, 0xff, 0x00, 0xff },
        { 200, 200, 0x00, 0x00, 0xff, 0xff },
        { 200, 0, 0xff, 0xff, 0xff, 0xff }
  };
  CoglOffscreen *offscreen;
  CoglPrimitive *prim =
    cogl_primitive_new_p2c4 (engine->ctx->cogl_context,
                             COGL_VERTICES_MODE_TRIANGLE_FAN, 4, quad);
  CoglPipeline *pipeline = cogl_pipeline_new (engine->ctx->cogl_context);

  engine->gradient =
    cogl_texture_2d_new_with_size (rut_cogl_context, 200, 200);

  offscreen = cogl_offscreen_new_with_texture (engine->gradient);

  cogl_framebuffer_orthographic (offscreen,
                                 0, 0,
                                 200,
                                 200,
                                 -1,
                                 100);
  cogl_framebuffer_clear4f (offscreen,
                            COGL_BUFFER_BIT_COLOR | COGL_BUFFER_BIT_DEPTH,
                            0, 0, 0, 1);
  cogl_primitive_draw (prim,
                       offscreen,
                       pipeline);

  cogl_object_unref (prim);
  cogl_object_unref (offscreen);
}

static void
add_light_handle (RigEngine *engine, RigUI *ui)
{
  //RutCamera *camera =
  //  rut_entity_get_component (ui->light, RUT_COMPONENT_TYPE_CAMERA);
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
      RutModel *model = rut_model_new_from_asset_mesh (engine->ctx, mesh,
                                                       FALSE, FALSE);
      RutMaterial *material = rut_material_new (engine->ctx, NULL);

      engine->light_handle = rut_entity_new (engine->ctx);
      rut_entity_set_label (engine->light_handle, "rig:light_handle");
      rut_entity_set_scale (engine->light_handle, 100);
      rut_graphable_add_child (ui->light, engine->light_handle);

      rut_entity_add_component (engine->light_handle, model);

      rut_entity_add_component (engine->light_handle, material);
      rut_material_set_receive_shadow (material, false);
      rut_material_set_cast_shadow (material, false);

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
      RutModel *model = rut_model_new_from_mesh (engine->ctx, mesh);
      RutMaterial *material = rut_material_new (engine->ctx, NULL);

      engine->play_camera_handle = rut_entity_new (engine->ctx);
      rut_entity_set_label (engine->play_camera_handle,
                            "rig:play_camera_handle");

      rut_entity_add_component (engine->play_camera_handle,
                                model);

      rut_entity_add_component (engine->play_camera_handle,
                                material);
      rut_material_set_receive_shadow (material, false);
      rut_material_set_cast_shadow (material, FALSE);
      rut_graphable_add_child (engine->play_camera,
                               engine->play_camera_handle);

      rut_object_unref (model);
      rut_object_unref (material);
      rut_object_unref (mesh);
#endif
    }
}

void
rig_engine_set_play_mode_ui (RigEngine *engine,
                             RigUI *ui)
{
  //bool first_ui = (engine->edit_mode_ui == NULL &&
  //                 engine->play_mode_ui == NULL &&
  //                 ui != NULL);

  if (engine->play_mode_ui == ui)
    return;

  if (engine->play_mode_ui)
    {
      rut_object_unref (engine->play_mode_ui);
      engine->play_mode_ui = NULL;

      if (engine->edit_to_play_object_map)
        {
          g_hash_table_destroy (engine->edit_to_play_object_map);
          engine->edit_to_play_object_map = NULL;
        }
    }

  if (ui)
    engine->play_mode_ui = rut_object_ref (ui);

  //if (engine->edit_mode_ui == NULL && engine->play_mode_ui == NULL)
  //  free_shared_ui_state (engine);

  if (engine->play_mode)
    rig_engine_set_current_ui (engine, ui);

  if (!ui)
    return;

  //if (first_ui)
  //  setup_shared_ui_state (engine);
}

static RutAsset *
share_asset_cb (RigPBUnSerializer *unserializer,
                Rig__Asset *pb_asset,
                void *user_data)
{
  RutObject *obj = (RutObject *)(intptr_t)pb_asset->id;
  return rut_object_ref (obj);
}

static void *
lookup_copied_object_cb (uint64_t id,
                         void *user_data)
{
  GHashTable *ptr_id_to_object_map = user_data;
  return g_hash_table_lookup (ptr_id_to_object_map, &id);
}

static void
register_copied_object_cb (void *object,
                           uint64_t id,
                           void *user_data)
{
  GHashTable *ptr_id_to_object_map = user_data;
  void *ptr_id = (void *)(intptr_t)id;

  /* NB: in this case we know the ids fit inside a pointer and
   * the hash table keys are pointers
   */

  g_hash_table_insert (ptr_id_to_object_map, ptr_id, object);
}

static RigUI *
rig_engine_copy_ui (RigEngine *engine,
                    RigUI *src_ui,
                    GHashTable *src_id_to_copied_object_map)
{
  RigPBSerializer *serializer;
  Rig__UI *pb_ui;
  RigPBUnSerializer unserializer;
  RigUI *copy;

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
                               false, /* edit mode */
                               src_ui);

  rig_pb_unserializer_init (&unserializer, engine);

  rig_pb_unserializer_set_object_register_callback (&unserializer,
                                                    register_copied_object_cb,
                                                    src_id_to_copied_object_map);

  rig_pb_unserializer_set_id_to_object_callback (&unserializer,
                                                 lookup_copied_object_cb,
                                                 src_id_to_copied_object_map);

  rig_pb_unserializer_set_asset_unserialize_callback (&unserializer,
                                                      share_asset_cb,
                                                      NULL);

  copy = rig_pb_unserialize_ui (&unserializer, pb_ui);

  rig_pb_unserializer_destroy (&unserializer);

  rig_pb_serialized_ui_destroy (pb_ui);

  rig_pb_serializer_destroy (serializer);

  return copy;
}

void
rig_engine_reset_play_mode_ui (RigEngine *engine)
{
  RigUI *play_mode_ui;

  g_return_if_fail (engine->frontend &&
                    engine->frontend_id == RIG_FRONTEND_ID_EDITOR);

  rig_engine_set_play_mode_ui (engine, NULL);

  g_warn_if_fail (engine->edit_to_play_object_map == NULL);

  engine->edit_to_play_object_map =
    g_hash_table_new (NULL, /* direct hash */
                      NULL); /* direct key equal */

  /* NB: the edit_to_play_object_map hash table lets us map an
   * edit-mode object into a corresponding play-mode object so we
   * make best effort attempts to apply edit operations to the
   * play-mode ui.
   */
  play_mode_ui = rig_engine_copy_ui (engine,
                                     engine->edit_mode_ui,
                                     engine->edit_to_play_object_map);
  rig_engine_set_play_mode_ui (engine, play_mode_ui);
  rut_object_unref (play_mode_ui);

  rig_frontend_reload_simulator_uis (engine->frontend);
}

void
rig_engine_set_edit_mode_ui (RigEngine *engine,
                             RigUI *ui)
{
  //bool first_ui = (engine->edit_mode_ui == NULL &&
  //                 engine->play_mode_ui == NULL &&
  //                 ui != NULL);

  if (engine->edit_mode_ui == ui)
    return;

  /* Updating the edit mode ui implies we need to also replace
   * any play mode ui too... */
  rig_engine_set_play_mode_ui (engine, NULL);

  if (engine->frontend)
    {
      rig_controller_view_set_controller (engine->controller_view,
                                          NULL);

      clear_search_results (engine);
      free_result_input_closures (engine);

      if (engine->grid_prim)
        {
          cogl_object_unref (engine->grid_prim);
          engine->grid_prim = NULL;
        }
    }

  if (engine->play_camera_handle)
    {
      rut_object_unref (engine->play_camera_handle);
      engine->play_camera_handle = NULL;
    }

  if (engine->light_handle)
    {
      rut_object_unref (engine->light_handle);
      engine->light_handle = NULL;
    }

  if (engine->edit_mode_ui)
    rut_object_unref (engine->edit_mode_ui);
  engine->edit_mode_ui = rut_object_ref (ui);

  //if (engine->edit_mode_ui == NULL && engine->play_mode_ui == NULL)
  //  free_shared_ui_state (engine);

  if (engine->play_mode == false)
    rig_engine_set_current_ui (engine, ui);

  rig_engine_set_play_mode_enabled (engine, false);

  if (!ui)
    return;

  //if (first_ui)
  //  setup_shared_ui_state (engine);

  if (engine->frontend)
    {
      rig_controller_view_update_controller_list (engine->controller_view);

      rig_controller_view_set_controller (engine->controller_view,
                                          ui->controllers->data);

      engine->grid_prim = rut_create_create_grid (engine->ctx,
                                                  engine->device_width,
                                                  engine->device_height,
                                                  100,
                                                  100);

      rig_load_asset_list (engine);

      /* Note: we only have to explicitly add light and camera handles
       * in the editor frontend, since the whole UI will be serialized
       * unconditionally when sending to the simulator, including any
       * special entities we add...
       */
      add_light_handle (engine, ui);
      add_play_camera_handle (engine, ui);

      /* Whenever we replace the edit mode graph that implies we need
       * to scrap and update the play mode graph, with a snapshot of
       * the new edit mode graph.
       */
      rig_engine_reset_play_mode_ui (engine);
    }
}

void
rig_engine_set_onscreen_size (RigEngine *engine,
                              int width,
                              int height)
{
  if (engine->window_width == width && engine->window_height == height)
    return;

  /* FIXME: This should probably be rut_shell api instead */
#if defined (COGL_HAS_SDL_SUPPORT) && (SDL_MAJOR_VERSION >= 2)
  {
    SDL_Window *sdl_window = cogl_sdl_onscreen_get_window (engine->onscreen);
    SDL_SetWindowSize (sdl_window, width, height);
  }
#else
#warning "rig_engine_set_onscreen_size unsupported without SDL2"
#endif
}

static void
ensure_shadow_map (RigEngine *engine)
{
  CoglTexture2D *color_buffer;
  //RigUI *ui = engine->edit_mode_ui ?
  //  engine->edit_mode_ui : engine->play_mode_ui;

  /*
   * Shadow mapping
   */

  /* Setup the shadow map */

  g_warn_if_fail (engine->shadow_color == NULL);

  color_buffer = cogl_texture_2d_new_with_size (rut_cogl_context,
                                                engine->device_width * 2,
                                                engine->device_height * 2);

  engine->shadow_color = color_buffer;

  g_warn_if_fail (engine->shadow_fb == NULL);

  /* XXX: Right now there's no way to avoid allocating a color buffer. */
  engine->shadow_fb =
    cogl_offscreen_new_with_texture (color_buffer);
  if (engine->shadow_fb == NULL)
    g_critical ("could not create offscreen buffer");

  /* retrieve the depth texture */
  cogl_framebuffer_set_depth_texture_enabled (engine->shadow_fb,
                                              TRUE);

  g_warn_if_fail (engine->shadow_map == NULL);

  engine->shadow_map =
    cogl_framebuffer_get_depth_texture (engine->shadow_fb);
}

static void
free_shadow_map (RigEngine *engine)
{
  if (engine->shadow_map)
    {
      cogl_object_unref (engine->shadow_map);
      engine->shadow_map = NULL;
    }
  if (engine->shadow_fb)
    {
      cogl_object_unref (engine->shadow_fb);
      engine->shadow_fb = NULL;
    }
  if (engine->shadow_color)
    {
      cogl_object_unref (engine->shadow_color);
      engine->shadow_color = NULL;
    }
}

static void
_rig_engine_free (void *object)
{
  RigEngine *engine = object;
  RutShell *shell = engine->shell;

  if (engine->frontend)
    {
#ifdef RIG_EDITOR_ENABLED
      if (engine->frontend_id == RIG_FRONTEND_ID_EDITOR)
        {
          int i;

          free_builtin_assets (engine);

          for (i = 0; i < G_N_ELEMENTS (engine->splits); i++)
            rut_object_unref (engine->splits[i]);

          rut_object_unref (engine->top_vbox);
          rut_object_unref (engine->top_hbox);
          rut_object_unref (engine->asset_panel_hbox);
          rut_object_unref (engine->properties_hbox);

          if (engine->transparency_grid)
            rut_object_unref (engine->transparency_grid);

          rut_closure_list_disconnect_all (&engine->tool_changed_cb_list);
        }
#endif
      rig_renderer_fini (engine);

      cogl_object_unref (engine->circle_node_attribute);

      free_shadow_map (engine);

      cogl_object_unref (engine->onscreen);

      cogl_object_unref (engine->default_pipeline);

#ifdef __APPLE__
      rig_osx_deinit (engine);
#endif

#ifdef USE_GTK
      {
        GApplication *application = g_application_get_default ();
        g_object_unref (application);
      }
#endif /* USE_GTK */
    }

  rut_object_unref (engine->objects_selection);

  rig_engine_set_edit_mode_ui (engine, NULL);

  rut_shell_remove_input_camera (shell, engine->camera_2d, engine->root);

  rut_object_unref (engine->main_camera_view);
  rut_object_unref (engine->camera_2d);
  rut_object_unref (engine->root);

  rig_pb_serializer_destroy (engine->ops_serializer);

  rut_memory_stack_free (engine->frame_stack);
  rut_magazine_free (engine->object_id_magazine);

  rut_introspectable_destroy (engine);

  rut_object_free (RigEngine, engine);
}

RutType rig_engine_type;

static void
_rig_engine_init_type (void)
{
  RutType *type = &rig_engine_type;
#define TYPE RigEngine

  rut_type_init (type, G_STRINGIFY (TYPE), _rig_engine_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_INTROSPECTABLE,
                      offsetof (TYPE, introspectable),
                      NULL); /* no implied vtable */

#undef TYPE
}

#if 0
static RutMagazine *object_id_magazine = NULL;

static void
free_object_id (void *id)
{
  rut_magazine_chunk_free (object_id_magazine, id);
}
#endif

static RigEngine *
_rig_engine_new_full (RutShell *shell,
                      const char *ui_filename,
                      RigFrontend *frontend,
                      RigSimulator *simulator)
{
  RigEngine *engine = rut_object_alloc0 (RigEngine, &rig_engine_type,
                                         _rig_engine_init_type);
  CoglFramebuffer *fb;


  engine->shell = shell;
  engine->ctx = rut_shell_get_context (shell);

  engine->headless = engine->ctx->headless;

  if (frontend)
    {
      engine->frontend_id = frontend->id;
      engine->frontend = frontend;
    }
  else if (simulator)
    {
      engine->frontend_id = simulator->frontend_id;
      engine->simulator = simulator;
    }


  cogl_matrix_init_identity (&engine->identity);

  rut_introspectable_init (engine,
                           _rig_engine_prop_specs,
                           engine->properties);

  engine->frame_stack = rut_memory_stack_new (8192);

  engine->object_id_magazine = rut_magazine_new (sizeof (uint64_t), 1000);

  engine->ops_serializer = rig_pb_serializer_new (engine);

  rig_pb_serializer_set_use_pointer_ids_enabled (engine->ops_serializer, true);

  engine->assets_registry = g_hash_table_new_full (g_str_hash,
                                                   g_str_equal,
                                                   g_free,
                                                   rut_object_unref);

  engine->device_width = DEVICE_WIDTH;
  engine->device_height = DEVICE_HEIGHT;

  if (engine->frontend)
    ensure_shadow_map (engine);

  /*
   * Setup the 2D widget scenegraph
   */
  engine->root = rut_graph_new (engine->ctx);

  engine->top_stack = rut_stack_new (engine->ctx, 1, 1);
  rut_graphable_add_child (engine->root, engine->top_stack);
  rut_object_unref (engine->top_stack);

  engine->camera_2d = rut_camera_new (engine->ctx,
                                      -1, /* ortho/vp width */
                                      -1, /* ortho/vp height */
                                      NULL);
  rut_camera_set_clear (engine->camera_2d, FALSE);

  /* XXX: Basically just a hack for now. We should have a
   * RutShellWindow type that internally creates a RutCamera that can
   * be used when handling input events in device coordinates.
   */
  rut_shell_set_window_camera (shell, engine->camera_2d);

  rut_shell_add_input_camera (shell, engine->camera_2d, engine->root);

#ifdef RIG_EDITOR_ENABLED

  /* NB: The simulator also needs to track selections when in support
   * of an editor. */
  engine->objects_selection = _rig_objects_selection_new (engine);

  if (_rig_in_editor_mode)
    {
      rut_list_init (&engine->tool_changed_cb_list);

      rig_engine_push_undo_subjournal (engine);

      /* NB: in device mode we assume all inputs need to got to the
       * simulator and we don't need a separate queue. */
      engine->simulator_input_queue = rut_input_queue_new (engine->shell);

      /* Create a color gradient texture that can be used for debugging
       * shadow mapping.
       *
       * XXX: This should probably simply be #ifdef DEBUG code.
       */
      create_debug_gradient (engine);

      load_builtin_assets (engine);

      create_editor_ui (engine);
    }
  else
#endif
    {
      engine->main_camera_view = rig_camera_view_new (engine);
      rut_stack_add (engine->top_stack, engine->main_camera_view);
    }


  if (!_rig_in_simulator_mode)
    {
      engine->default_pipeline = cogl_pipeline_new (engine->ctx->cogl_context);

      engine->circle_node_attribute =
        rut_create_circle_fan_p2 (engine->ctx, 20, &engine->circle_node_n_verts);

      engine->renderer = rig_renderer_new (engine);
      rig_renderer_init (engine);

#ifndef __ANDROID__
      if (ui_filename)
        {
          struct stat st;

          stat (ui_filename, &st);
          if (S_ISREG (st.st_mode))
            rig_engine_load_file (engine, ui_filename);
          else
            {
              RigUI *ui = rig_ui_new (engine);
              rig_ui_prepare (ui);

              if (engine->frontend_id == RIG_FRONTEND_ID_EDITOR)
                rig_engine_set_edit_mode_ui (engine, ui);
              else
                rig_engine_set_play_mode_ui (engine, ui);
            }
        }
#endif

#ifdef RIG_EDITOR_ENABLED
      if (_rig_in_editor_mode)
        {
          engine->onscreen = cogl_onscreen_new (engine->ctx->cogl_context,
                                                1000, 700);
          cogl_onscreen_set_resizable (engine->onscreen, TRUE);
        }
      else
#endif
        engine->onscreen = cogl_onscreen_new (engine->ctx->cogl_context,
                                              engine->device_width / 2,
                                              engine->device_height / 2);

      cogl_onscreen_add_resize_callback (engine->onscreen,
                                         engine_onscreen_resize,
                                         engine,
                                         NULL);

      cogl_framebuffer_allocate (engine->onscreen, NULL);

      fb = engine->onscreen;
      engine->window_width = cogl_framebuffer_get_width (fb);
      engine->window_height  = cogl_framebuffer_get_height (fb);

      engine->frontend->has_resized = true;
      engine->frontend->pending_width = engine->window_width;
      engine->frontend->pending_height = engine->window_height;

      rut_shell_add_onscreen (engine->shell, engine->onscreen);

#ifdef USE_GTK
        {
          RigApplication *application = rig_application_new (engine);

          gtk_init (NULL, NULL);

          /* We need to register the application before showing the onscreen
           * because we need to set the dbus paths before the window is
           * mapped. FIXME: Eventually it might be nice to delay creating
           * the windows until the ‘activate’ or ‘open’ signal is emitted so
           * that we can support the single process properly. In that case
           * we could let g_application_run handle the registration
           * itself */
          if (!g_application_register (G_APPLICATION (application),
                                       NULL, /* cancellable */
                                       NULL /* error */))
            /* Another instance of the application is already running */
            rut_shell_quit (shell);

          rig_application_add_onscreen (application, engine->onscreen);
        }
#endif

#ifdef __APPLE__
      rig_osx_init (engine);
#endif

      rut_shell_set_title (engine->shell,
                           engine->onscreen,
                           "Rig " G_STRINGIFY (RIG_VERSION));

      cogl_onscreen_show (engine->onscreen);

#warning "FIXME: rely on simulator to handle allocate()"
      allocate (engine);
    }

  return engine;
}

void
rig_engine_load_file (RigEngine *engine,
                      const char *filename)
{
  RigUI *ui;

  engine->ui_filename = g_strdup (filename);

  ui = rig_load (engine, filename);

  if (!ui)
    {
      ui = rig_ui_new (engine);
      rig_ui_prepare (ui);
    }

  if (engine->frontend_id == RIG_FRONTEND_ID_EDITOR)
    rig_engine_set_edit_mode_ui (engine, ui);
  else
    rig_engine_set_play_mode_ui (engine, ui);

}

RigEngine *
rig_engine_new_for_simulator (RutShell *shell,
                              RigSimulator *simulator)
{
  return _rig_engine_new_full (shell, NULL, NULL, simulator);
}

RigEngine *
rig_engine_new_for_frontend (RutShell *shell,
                             RigFrontend *frontend,
                             const char *ui_filename)
{
  return _rig_engine_new_full (shell, ui_filename, frontend, NULL);
}

RutInputEventStatus
rig_engine_input_handler (RutInputEvent *event, void *user_data)
{
  RigEngine *engine = user_data;

#if 0
  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION)
    {
      /* Anything that can claim the keyboard focus will do so during
       * motion events so we clear it before running other input
       * callbacks */
      engine->key_focus_callback = NULL;
    }
#endif

  switch (rut_input_event_get_type (event))
    {
    case RUT_INPUT_EVENT_TYPE_KEY:
#ifdef RIG_EDITOR_ENABLED
      if (_rig_in_editor_mode &&
          rut_key_event_get_action (event) == RUT_KEY_EVENT_ACTION_DOWN)
        {
          switch (rut_key_event_get_keysym (event))
            {
            case RUT_KEY_s:
              if ((rut_key_event_get_modifier_state (event) &
                   RUT_MODIFIER_CTRL_ON))
                {
                  rig_save (engine, engine->ui_filename);
                  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
                }
              break;
            case RUT_KEY_z:
              if ((rut_key_event_get_modifier_state (event) &
                   RUT_MODIFIER_CTRL_ON))
                {
                  rig_undo_journal_undo (engine->undo_journal);
                  return RUT_INPUT_EVENT_STATUS_HANDLED;
                }
              break;
            case RUT_KEY_y:
              if ((rut_key_event_get_modifier_state (event) &
                   RUT_MODIFIER_CTRL_ON))
                {
                  rig_undo_journal_redo (engine->undo_journal);
                  return RUT_INPUT_EVENT_STATUS_HANDLED;
                }
              break;

#if 1
              /* HACK: Currently it's quite hard to select the play
               * camera because it will usually be positioned far away
               * from the scene. This provides a way to select it by
               * pressing Ctrl+C. Eventually it should be possible to
               * select it using a list of entities somewhere */
            case RUT_KEY_r:
              if ((rut_key_event_get_modifier_state (event) &
                   RUT_MODIFIER_CTRL_ON))
                {
                  RutEntity *play_camera = engine->play_mode ?
                    engine->play_mode_ui->play_camera : engine->edit_mode_ui->play_camera;

                  rig_select_object (engine, play_camera, RUT_SELECT_ACTION_REPLACE);
                  _rig_engine_update_inspector (engine);
                  return RUT_INPUT_EVENT_STATUS_HANDLED;
                }
              break;
#endif
            }
        }
#endif
      break;

    case RUT_INPUT_EVENT_TYPE_MOTION:
    case RUT_INPUT_EVENT_TYPE_TEXT:
    case RUT_INPUT_EVENT_TYPE_DROP:
      break;
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

#if 0
static RutInputEventStatus
add_light_cb (RutInputRegion *region,
              RutInputEvent *event,
              void *user_data)
{
  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION)
    {
      if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_DOWN)
        {
          g_print ("Add light!\n");
          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}
#endif

void
rig_register_asset (RigEngine *engine,
                    RutAsset *asset)
{
  char *key = g_strdup (rut_asset_get_path (asset));

  g_hash_table_insert (engine->assets_registry,
                       key,
                       rut_object_ref (asset));
}

RutAsset *
rig_lookup_asset (RigEngine *engine,
                  const char *path)
{
  return g_hash_table_lookup (engine->assets_registry, path);
}

RutAsset *
rig_load_asset (RigEngine *engine, GFileInfo *info, GFile *asset_file)
{
  GFile *assets_dir = g_file_new_for_path (engine->ctx->assets_location);
  GFile *dir = g_file_get_parent (asset_file);
  char *path = g_file_get_relative_path (assets_dir, asset_file);
  GList *inferred_tags = NULL;
  RutAsset *asset = NULL;

  inferred_tags = rut_infer_asset_tags (engine->ctx, info, asset_file);

  if (rut_util_find_tag (inferred_tags, "image") ||
      rut_util_find_tag (inferred_tags, "video"))
    {
      if (rut_util_find_tag (inferred_tags, "normal-maps"))
        asset = rut_asset_new_normal_map (engine->ctx, path, inferred_tags);
      else if (rut_util_find_tag (inferred_tags, "alpha-masks"))
        asset = rut_asset_new_alpha_mask (engine->ctx, path, inferred_tags);
      else
        asset = rut_asset_new_texture (engine->ctx, path, inferred_tags);
    }
  else if (rut_util_find_tag (inferred_tags, "ply"))
    asset = rut_asset_new_ply_model (engine->ctx, path, inferred_tags);

  if (asset && _rig_in_editor_mode && rut_asset_needs_thumbnail (asset))
    rut_asset_thumbnail (asset, rig_refresh_thumbnails, engine, NULL);

  g_list_free (inferred_tags);

  g_object_unref (assets_dir);
  g_object_unref (dir);
  g_free (path);

  return asset;
}

#ifdef RIG_EDITOR_ENABLED

static void
add_asset (RigEngine *engine, GFileInfo *info, GFile *asset_file)
{
  GFile *assets_dir = g_file_new_for_path (engine->ctx->assets_location);
  char *path = g_file_get_relative_path (assets_dir, asset_file);
  GList *l;
  RutAsset *asset = NULL;

  /* Avoid loading duplicate assets... */
  for (l = engine->edit_mode_ui->assets; l; l = l->next)
    {
      RutAsset *existing = l->data;

      if (strcmp (rut_asset_get_path (existing), path) == 0)
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
#endif

void
rig_engine_sync_slaves (RigEngine *engine)
{
  GList *l;

  for (l = engine->slave_masters; l; l = l->next)
    rig_slave_master_reload_ui (l->data);
}

void
rig_engine_push_undo_subjournal (RigEngine *engine)
{
  RigUndoJournal *subjournal = rig_undo_journal_new (engine);

  rig_undo_journal_set_apply_on_insert (subjournal, true);

  engine->undo_journal_stack = g_list_prepend (engine->undo_journal_stack,
                                               subjournal);
  engine->undo_journal = subjournal;
}

RigUndoJournal *
rig_engine_pop_undo_subjournal (RigEngine *engine)
{
  RigUndoJournal *head_journal = engine->undo_journal;

  engine->undo_journal_stack = g_list_delete_link (engine->undo_journal_stack,
                                                   engine->undo_journal_stack);
  g_return_if_fail (engine->undo_journal_stack);

  engine->undo_journal = engine->undo_journal_stack->data;

  return head_journal;
}

Rig__Operation **
rig_engine_serialize_ops (RigEngine *engine,
                          RigPBSerializer *serializer)
{
  Rig__Operation **pb_ops;
  GList *l;
  int i;

  if (!engine->n_ops)
    return NULL;

  pb_ops =
    rut_memory_stack_memalign (engine->frame_stack,
                               sizeof (void *) * engine->n_ops,
                               RUT_UTIL_ALIGNOF (void *));

  for (i = 0, l = engine->ops; l; l = l->next, i++)
    pb_ops[i] = l->data;

  return pb_ops;

#warning "FIXME"
  /* XXX: How will operations such as add_entity be synchonized
   * with the frontend?
   *
   * Simple property changes work because the simulator logs
   * the property change when applying and sends an update
   * back to the editor.
   *
   * As part of a bigger question:
   * It is going to be desirable for logic to be able to
   * instantiate new entities, add/delete components and
   * manipulate Rigs, how is that going to be possible?
   * - Firstly it implies we need to create corresponding
   *   protocol for communicating changes from the simulator
   *   to the frontend.
   * - It raises the question of how, can we determine what
   *   UI state should be saved, vs what state is a side
   *   effect of running UI logic.
   * - If the editor always maintained a pristine UI that
   *   was never modified by UI logic it would even mean that
   *   simple expressions to make object A follow object B
   *   wouldn't be reflected unless in play mode and so the edit
   *   area could become quite disconnected from the thing being
   *   created.
   *
   *   We could have a flag set by the editor on all objects to
   *   distinquish and ignore objects created by scripts when
   *   saving. What about if the scripts modify editor objects?
   *
   *   Considering all ops:
   *   add_entity: ok with a flag to ignore when saving
   *   delete_entity: could dissallow deleting flagged entities
   *                  can we assume this special rule will be
   *                  acceptable since could should only ever
   *                  try to delete things that it owns so it
   *                  would be strange to want to try and delete
   *                  an objected created with the editor.
   *   add_component: ok with a flag to ignore when saving
   *   delete_component: as with delete_entity, we can assume
   *                     code shouldn't ever need to try and
   *                     delete a component created with the
   *                     editor.
   *   add_controller: ok (with flag)
   *   delete_controller: ok if not flagged
   *   controller_set_const: ok if controller not flagged
   *
   *
   *   XXX: I think we really we need to have a clean separation
   *   between the state of "play mode" and "edit mode" and
   *   initially simply say that no logic runs in edit mode.
   *
   *   Both the simulator and frontend should track separate
   *   play vs edit UIs that can be switched between.
   *
   *   Should we run two simulators - for each mode?
   *   This is similar in concept to being connected to slave
   *   while editing. Edits would be forwarded to any slaves
   *   and the simulator in play mode on a best effort basis
   *   (you may be able to get into an inconsistent state but
   *   we want the convenience of getting immediate feedback
   *   for edits without having to always reset the play mode
   *   state.)
   */
}

void
rig_engine_clear_ops (RigEngine *engine)
{
  g_list_free (engine->ops);
  engine->ops = NULL;
  engine->n_ops = 0;
}

void
rig_engine_op_register_object (RigEngine *engine,
                               RutObject *object)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);
  RutProperty *label_property;
  const char *label;

  pb_op->type = RIG_ENGINE_OP_TYPE_REGISTER_OBJECT;

  pb_op->register_object =
    rig_pb_new (serializer, Rig__Operation__RegisterObject,
                rig__operation__register_object__init);
  label_property = rut_introspectable_lookup_property (object, "label");
  label = rut_property_get_text (label_property);

  pb_op->register_object->label = (char *)rig_pb_strdup (engine, label);
  pb_op->register_object->object_id = (intptr_t)object;

  engine->ops = g_list_prepend (engine->ops, pb_op);
  engine->n_ops++;
}

#error "XXX: consider allowing synchronous edits"
/* XXX:
 * Deferring all ops like we do currently will cause problems for
 * using them from UI logic since it precludes operations for one
 * frame from depending on each other.
 *
 * Since all operations are applied locally before forwarding
 * them, then it might be ok to apply them immediately too?
 */
void
rig_engine_op_set_property (RigEngine *engine,
                            RutProperty *property,
                            RutBoxed *value)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_SET_PROPERTY;

  pb_op->set_property =
    rig_pb_new (serializer, Rig__Operation__SetProperty,
                rig__operation__set_property__init);

  pb_op->set_property->object_id = (intptr_t)property->object;
  pb_op->set_property->property_id = property->id;
  pb_op->set_property->value = pb_property_value_new (serializer, value);

  engine->ops = g_list_prepend (engine->ops, pb_op);
  engine->n_ops++;
}

void
rig_engine_op_add_entity (RigEngine *engine,
                          RutEntity *parent,
                          RutEntity *entity)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op;

  g_return_if_fail (rut_graphable_get_parent (entity) == NULL);

  pb_op = rig_pb_new (serializer, Rig__Operation, rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_ADD_ENTITY;

  pb_op->add_entity = rig_pb_new (serializer, Rig__Operation__AddEntity,
                                  rig__operation__add_entity__init);

  pb_op->add_entity->parent_entity_id = (intptr_t)parent;
  pb_op->add_entity->entity = rig_pb_serialize_entity (serializer,
                                                       NULL,
                                                       entity);

  engine->ops = g_list_prepend (engine->ops, pb_op);
  engine->n_ops++;
}

void
rig_engine_op_delete_entity (RigEngine *engine,
                             RutEntity *entity)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_DELETE_ENTITY;

  pb_op->delete_entity = rig_pb_new (serializer, Rig__Operation__DeleteEntity,
                                     rig__operation__delete_entity__init);

  pb_op->delete_entity->entity_id = (intptr_t)entity;

  engine->ops = g_list_prepend (engine->ops, pb_op);
  engine->n_ops++;
}

void
rig_engine_op_add_component (RigEngine *engine,
                             RutEntity *entity,
                             RutComponent *component)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_ADD_COMPONENT;

  pb_op->add_component = rig_pb_new (serializer, Rig__Operation__AddComponent,
                                     rig__operation__add_component__init);

  pb_op->add_component->parent_entity_id = (intptr_t)entity;
  pb_op->add_component->component =
    rig_pb_serialize_component (serializer, component);

  engine->ops = g_list_prepend (engine->ops, pb_op);
  engine->n_ops++;
}

void
rig_engine_op_delete_component (RigEngine *engine,
                                RutComponent *component)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_DELETE_COMPONENT;

  pb_op->delete_component = rig_pb_new (serializer, Rig__Operation__DeleteComponent,
                                        rig__operation__delete_component__init);
  pb_op->delete_component->component_id = (intptr_t)component;

  engine->ops = g_list_prepend (engine->ops, pb_op);
  engine->n_ops++;
}

void
rig_engine_op_add_controller (RigEngine *engine,
                              RigController *controller)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_ADD_CONTROLLER;

  pb_op->add_controller = rig_pb_new (serializer, Rig__Operation__AddController,
                                      rig__operation__add_controller__init);
  pb_op->add_controller->controller =
    rig_pb_serialize_controller (serializer, controller);

  engine->ops = g_list_prepend (engine->ops, pb_op);
  engine->n_ops++;
}

void
rig_engine_op_delete_controller (RigEngine *engine,
                                 RigController *controller)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_DELETE_CONTROLLER;

  pb_op->delete_controller =
    rig_pb_new (serializer, Rig__Operation__DeleteController,
                rig__operation__delete_controller__init);
  pb_op->delete_controller->controller_id = (intptr_t)controller;

  engine->ops = g_list_prepend (engine->ops, pb_op);
  engine->n_ops++;
}

void
rig_engine_op_controller_set_const (RigEngine *engine,
                                    RigController *controller,
                                    RutProperty *property,
                                    RutBoxed *value)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_CONTROLLER_SET_CONST;

  pb_op->controller_set_const =
    rig_pb_new (serializer, Rig__Operation__ControllerSetConst,
                rig__operation__controller_set_const__init);

  pb_op->controller_set_const->controller_id = (intptr_t)controller;
  pb_op->controller_set_const->object_id = (intptr_t)property->object;
  pb_op->controller_set_const->property_id = property->id;
  pb_op->controller_set_const->value =
    pb_property_value_new (serializer, value);

  engine->ops = g_list_prepend (engine->ops, pb_op);
  engine->n_ops++;
}

void
rig_engine_op_controller_path_add_node (RigEngine *engine,
                                        RigController *controller,
                                        RutProperty *property,
                                        float t,
                                        RutBoxed *value)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_CONTROLLER_PATH_ADD_NODE;

  pb_op->controller_path_add_node =
    rig_pb_new (serializer, Rig__Operation__ControllerPathAddNode,
                rig__operation__controller_path_add_node__init);
  pb_op->controller_path_add_node->controller_id = (intptr_t)controller;
  pb_op->controller_path_add_node->object_id = (intptr_t)property->object;
  pb_op->controller_path_add_node->property_id = property->id;
  pb_op->controller_path_add_node->t = t;
  pb_op->controller_path_add_node->value =
    pb_property_value_new (serializer, value);

  engine->ops = g_list_prepend (engine->ops, pb_op);
  engine->n_ops++;
}

void
rig_engine_op_controller_path_delete_node (RigEngine *engine,
                                           RigController *controller,
                                           RutProperty *property,
                                           float t)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_CONTROLLER_PATH_DELETE_NODE;

  pb_op->controller_path_delete_node =
    rig_pb_new (serializer, Rig__Operation__ControllerPathDeleteNode,
                rig__operation__controller_path_delete_node__init);
  pb_op->controller_path_delete_node->controller_id = (intptr_t)controller;
  pb_op->controller_path_delete_node->object_id = (intptr_t)property->object;
  pb_op->controller_path_delete_node->property_id = property->id;
  pb_op->controller_path_delete_node->t = t;

  engine->ops = g_list_prepend (engine->ops, pb_op);
  engine->n_ops++;
}

void
rig_engine_op_controller_path_set_node (RigEngine *engine,
                                        RigController *controller,
                                        RutProperty *property,
                                        float t,
                                        RutBoxed *value)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_CONTROLLER_PATH_SET_NODE;

  pb_op->controller_path_set_node =
    rig_pb_new (serializer, Rig__Operation__ControllerPathSetNode,
                rig__operation__controller_path_set_node__init);
  pb_op->controller_path_set_node->controller_id = (intptr_t)controller;
  pb_op->controller_path_set_node->object_id = (intptr_t)property->object;
  pb_op->controller_path_set_node->property_id = property->id;
  pb_op->controller_path_set_node->t = t;
  pb_op->controller_path_set_node->value =
    pb_property_value_new (serializer, value);

  engine->ops = g_list_prepend (engine->ops, pb_op);
  engine->n_ops++;
}

void
rig_engine_op_controller_add_property (RigEngine *engine,
                                       RigController *controller,
                                       RutProperty *property)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_CONTROLLER_ADD_PROPERTY;

  pb_op->controller_add_property =
    rig_pb_new (serializer, Rig__Operation__ControllerAddProperty,
                rig__operation__controller_add_property__init);
  pb_op->controller_add_property->controller_id = (intptr_t)controller;
  pb_op->controller_add_property->object_id = (intptr_t)property->object;
  pb_op->controller_add_property->property_id = property->id;

  engine->ops = g_list_prepend (engine->ops, pb_op);
  engine->n_ops++;
}

void
rig_engine_op_controller_remove_property (RigEngine *engine,
                                          RigController *controller,
                                          RutProperty *property)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_CONTROLLER_REMOVE_PROPERTY;

  pb_op->controller_remove_property =
    rig_pb_new (serializer, Rig__Operation__ControllerRemoveProperty,
                rig__operation__controller_remove_property__init);
  pb_op->controller_remove_property->controller_id = (intptr_t)controller;
  pb_op->controller_remove_property->object_id = (intptr_t)property->object;
  pb_op->controller_remove_property->property_id = property->id;

  engine->ops = g_list_prepend (engine->ops, pb_op);
  engine->n_ops++;
}

void
rig_engine_op_controller_property_set_method (RigEngine *engine,
                                              RigController *controller,
                                              RutProperty *property,
                                              RigControllerMethod method)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_CONTROLLER_PROPERTY_SET_METHOD;

  pb_op->controller_property_set_method =
    rig_pb_new (serializer, Rig__Operation__ControllerPropertySetMethod,
                rig__operation__controller_property_set_method__init);
  pb_op->controller_property_set_method->controller_id = (intptr_t)controller;
  pb_op->controller_property_set_method->object_id = (intptr_t)property->object;
  pb_op->controller_property_set_method->property_id = property->id;
  pb_op->controller_property_set_method->method = method;

  engine->ops = g_list_prepend (engine->ops, pb_op);
  engine->n_ops++;
}

void
rig_engine_op_set_play_mode (RigEngine *engine,
                             bool play_mode_enabled)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_SET_PLAY_MODE;

  pb_op->set_play_mode = rig_pb_new (serializer, Rig__Operation__SetPlayMode,
                                     rig__operation__set_play_mode__init);
  pb_op->set_play_mode->play_mode_enabled = play_mode_enabled;

  engine->ops = g_list_prepend (engine->ops, pb_op);
  engine->n_ops++;
}

typedef struct _ApplyOpsState
{
  RigEngineRegisterObjectCallback register_object_cb;
  void *user_data;
} ApplyOpsState;

static bool
apply_edit_register_object (void *play_mode_object,
                            uint64_t id,
                            void *user_data)
{
  ApplyOpsState *state = user_data;
  void *edit_mode_object = (void *)(intptr_t)id;

  /* NB: in this case we know the ids fit inside a pointer and the
   * hash table keys are pointers
   */

  g_hash_table_insert (engine->edit_to_play_object_map,
                       edit_mode_object, play_mode_object);

  return true; /* no need to additionally register the object in rig-pb.c */
}

typedef void (*RigEngineRegisterObjectCallback) (void *object,
                                                 uint64_t id,
                                                 void *user_data);

typedef void (*RigEngineUnRegisterObjectCallback) (uint64_t id,
                                                   void *user_data);

/*
 * XXX: How should we synchronize edits, between a frontend and
 * simulator process as well as forward edits to all simulators?
 *
 * Consider also that UI logic running in simulators will need the
 * ability to edit the UI too.
 */

/*
 * To support editing the play-mode ui, then we should support
 * mapping all of the object-ids from edit-mode to play-mode via
 * engine->edit_to_play_object_map
 *
 * This approach avoids needing to maintain an edit-to-play mapping in
 * the simulator.
 *
 * How will this work for slaves?
 * Initially the play-mode ui for a slave is setup from serialized
 * edit-mode ui, such slaves can maintain an id_to_play_mode_object
 * map that essentially maps edit-mode-ids to play-mode objects
 *
 * When we send edits to a slave to add an object, the ids will
 * also correspond to edit-mode objects
 *
 * How will a slave know when to remove an id mapping while running
 * its UI logic?
 *    Slaves must also maintain an object-to-id mapping so whenever
 *    an object is deleted via UI logic then it should remove any
 *    corresponding entry in the object-to-id and edit-id-to-object
 *    maps. Any further edits sent that reference these objects
 *    will fail.
 *
 *
 * Can we wrap up all these detail in this api?
 *
 * This function could handle:
 * - multiplexing edits to all slaves
 * - If editor: copy all edits and map them into play-mode-ui edits,
 *   append to list of outstanding edits.
 * - apply all edits
 * - forward all edits to to the simulator
 *
 *
 * Issues:
 *  - The simulator will not need the multiplexing nor the play-mode
 *    mapping, so maybe the api needs to be split up.
 *  - The simulator needs separate edit ops for the play-mode-ui. It
 *    will be awkward to try and share the same pb_ui_edit->ops as
 *    we prepare for a rig__simulator__run_frame().
 *  - Slaves won't need the multiplexing, or mapping either.
 *  - Making rig-editor.c responsible for multiplexing and mapping
 *    isn't ideal currently since it needs to update
 *    engine->edit_to_play_object_map currently owned by rig-engine.c
 *
 * XXX: we could make rig-undo-journal.c responsible for applying
 * operations to the edit and play mode uis to avoid needing to be
 * able to copy and map edits.
 *  I think though this would just make the journal code more awkward
 *  and ideally the journal shouldn't know anything about play-mode.
 *  Edit ops should be more low-level, and therefore simpler, than
 *  journal ops so handling this here could be simpler overall.
 *
 * Plan:
 *  Have a separate api for copying and mapping a set of edits into
 *  play-mode-ui edits.
 */

static void
forward_pb_ui_edit_to_slaves (RigEngine *engine,
                              Rig__UIEdit *edit)
{
  GList *l;

  for (l = engine->slave_masters; l; l = l->next)
    rig_slave_master_forward_pb_ui_edit (l->data, edit);
}

#error "make sure we are tracking object-id mappings correctly in all editing use cases"
/* XXX: E.g. consider
 * engine->edit_to_play_object_map
 * simulator->object_to_id_map
 * simulator->id_to_object_map
 * engine->ops_serializer->object_to_id_map
 */


uint64_t
edit_id_to_play_id (RigEngine *engine, uint64_t edit_id)
{
  void *ptr_edit_id = (void *)(intptr_t)edit_id;
  void *ptr_play_id =
    g_hash_table_lookup (engine->edit_to_play_object_map,
                         ptr_edit_id);
  return (uint64_t)(intptr_t)ptr_play_id;
}

/* XXX: Note that *not* everything is deep-copied. For example
 * Rig__PropertyValues that don't reference any object ids
 * will be shared. */
Rig__UIEdit *
rig_engine_map_pb_ui_edit (RigEngine *engine,
                           Rig__UIEdit *pb_ui_edit)
{
  RigPBSerializer *serializer;
  Rig__UIEdit *mapped_pb_ui_edits;
  Rig__Operation *pb_ops;
  int i;

  serializer = rig_pb_serializer_new (engine);
  rig_pb_serializer_set_use_pointer_ids_enabled (serializer, true);

  mapped_pb_ui_edits = rig_pb_new (serializer, Rig__UIEdit, rig__ui_edit__init);
  mapped_pb_ui_edits->n_ops = pb_ui_edit->n_ops;

  if (!pb_ui_edit->n_ops)
    return mapped_pb_ui_edits;

  mapped_pb_ui_edits->ops =
    rut_memory_stack_memalign (serializer->stack,
                               sizeof (void *) * mapped_pb_ui_edits->n_ops,
                               RUT_UTIL_ALIGNOF (void *));

  pb_ops = rut_memory_stack_memalign (serializer->stack,
                                      (sizeof (Rig__Operation) *
                                       mapped_pb_ui_edits->n_ops),
                                      RUT_UTIL_ALIGNOF (Rig__Operation));

  for (i = 0; i < pb_ui_edit->n_ops; i++)
    {
      Rig__Operation *src_pb_op = pb_ui_edit->ops[i];
      Rig__Operation *pb_op = &pb_ops[i];

      rig__operation__init (pb_op);

      mapped_pb_ui_edits->ops[i] = pb_op;

      pb_op->type = src_pb_op->type;

      switch (pb_op->type)
        {
        case RIG_ENGINE_OP_TYPE_SET_PROPERTY:
          {
            pb_op->set_property =
              rig_pb_dup (serializer,
                          Rig__Operation__SetProperty,
                          rig__operation__set_property__init,
                          src_pb_op->set_property);

            pb_op->set_property->object_id =
              edit_id_to_play_id (engine,
                                  src_pb_op->set_property->object_id);
            pb_op->set_property->value = src_pb_op->set_property->value;
            break;
          }
        case RIG_ENGINE_OP_TYPE_ADD_ENTITY:
          {
            pb_op->add_entity = rig_pb_dup (serializer,
                                            Rig__Operation__AddEntity,
                                            rig__operation__add_entity__init,
                                            src_pb_op->add_entity);

            pb_op->add_entity->parent_entity_id =
              edit_id_to_play_id (engine,
                                  src_pb_op->add_entity->parent_entity_id);

            /* XXX: we assume that the new entity isn't currently
             * associated with any components and so the serialized
             * entity doesn't any object ids that need mapping.
             *
             * The id of the entity itself will correspond to an
             * edit-mode object pointer, which can be used later to
             * create a mapping from the new edit-mode entity and the
             * new play-mode entity */
            pb_op->add_entity->entity = src_pb_op->add_entity->entity;
            break;
          }
        case RIG_ENGINE_OP_TYPE_DELETE_ENTITY:
          {
            pb_op->delete_entity = rig_pb_dup (serializer,
                                               Rig__Operation__DeleteEntity,
                                               rig__operation__delete_entity__init,
                                               src_pb_op->delete_entity);

            pb_op->delete_entity->entity_id =
              edit_id_to_play_id (engine,
                                  src_pb_op->delete_entity->entity_id);
            break;
          }
        case RIG_ENGINE_OP_TYPE_ADD_COMPONENT:
          {
            pb_op->add_component = rig_pb_dup (serializer,
                                               Rig__Operation__AddComponent,
                                               rig__operation__add_component__init,
                                               src_pb_op->add_component);

            pb_op->add_component->parent_entity_id =
              edit_id_to_play_id (engine,
                                  src_pb_op->add_component->parent_entity_id);

            /* XXX: we assume that the new component isn't currently
             * associated with any other object ids that need mapping.
             *
             * The id of the component itself will correspond to an
             * edit-mode object pointer, which can be used later to
             * create a mapping from the new edit-mode component and
             * the new play-mode component */
            pb_op->add_component->component =
              src_pb_op->add_component->component;
            break;
          }
        case RIG_ENGINE_OP_TYPE_DELETE_COMPONENT:
          {
            pb_op->delete_component =
              rig_pb_dup (serializer,
                          Rig__Operation__DeleteComponent,
                          rig__operation__delete_component__init,
                          src_pb_op->delete_component);

            pb_op->delete_component->component_id =
              edit_id_to_play_id (engine,
                                  src_pb_op->delete_component->component_id);
            break;
          }
#if 0
      message AddController
        {
          required Controller controller=1;
        }
      optional AddController add_controller=8;

      message DeleteController
        {
          required sint64 controller_id=1;
        }
      optional DeleteController delete_controller=9;

      //Change the constant associated with a controller property
      message ControllerSetConst
        {
          required sint64 controller_id=1;
          required sint64 object_id=2;
          required int32 property_id=3;
          required PropertyValue value=4;
        }
      optional ControllerSetConst controller_set_const=10;

      //Add a new controller property path node / key frame
      message ControllerPathAddNode
        {
          required sint64 controller_id=1;
          required sint64 object_id=2;
          required int32 property_id=3;
          required float t=4;
          required PropertyValue value=5;
        }
      optional ControllerPathAddNode controller_path_add_node=11;

      //Remove a node / key frame from a controller property path
      message ControllerPathDeleteNode
        {
          required sint64 controller_id=1;
          required sint64 object_id=2;
          required int32 property_id=3;
          required float t=4;
        }
      optional ControllerPathDeleteNode controller_path_delete_node=12;

      //Change the value of a controller property path node / key frame
      message ControllerPathSetNode
        {
          required sint64 controller_id=1;
          required sint64 object_id=2;
          required int32 property_id=3;
          required float t=4;
          required PropertyValue value=5;
        }
      optional ControllerPathSetNode controller_path_set_node=13;

      //Associate a property with a controller
      message ControllerAddProperty
        {
          required sint64 controller_id=1;
          required sint64 object_id=2;
          required int32 property_id=3;
        }
      optional ControllerAddProperty controller_add_property=14;

      //Disassociate a property from a controller
      message ControllerRemoveProperty
        {
          required sint64 controller_id=1;
          required sint64 object_id=2;
          required int32 property_id=3;
        }
      optional ControllerRemoveProperty controller_remove_property=15;

      //Change the method of controlling a property
      message ControllerPropertySetMethod
        {
          required sint64 controller_id=1;
          required sint64 object_id=2;
          required int32 property_id=3;

          enum Method { CONSTANT=1; PATH=2; BINDING=3; }

          required Method method=4;
        }
      optional ControllerPropertySetMethod controller_property_set_method=16;

#endif

        case RIG_ENGINE_OP_TYPE_ADD_CONTROLLER:
          {

            break;
          }
        case RIG_ENGINE_OP_TYPE_DELETE_CONTROLLER:
          {

            break;
          }
        case RIG_ENGINE_OP_TYPE_CONTROLLER_SET_CONST:
          {

            break;
          }
        case RIG_ENGINE_OP_TYPE_CONTROLLER_PATH_ADD_NODE:
          {

            break;
          }
        case RIG_ENGINE_OP_TYPE_CONTROLLER_PATH_DELETE_NODE:
          {

            break;
          }
        case RIG_ENGINE_OP_TYPE_CONTROLLER_PATH_SET_NODE:
          {

            break;
          }
        case RIG_ENGINE_OP_TYPE_CONTROLLER_ADD_PROPERTY:
          {

            break;
          }
        case RIG_ENGINE_OP_TYPE_CONTROLLER_REMOVE_PROPERTY:
          {

            break;
          }
        case RIG_ENGINE_OP_TYPE_CONTROLLER_PROPERTY_SET_METHOD:
          {

            break;
          }

        case RIG_ENGINE_OP_TYPE_SET_PLAY_MODE:
          break;
        }
    }

  rig_pb_serializer_destroy (serializer);

  return mapped_pb_ui_edits;
}

bool
rig_engine_pb_op_apply (RigEngine *engine,
                        Rig__Operation *pb_op,
                        RigUI *ui,
                        RigEngineIdToObjectCallback id_to_object_cb,
                        RigEngineRegisterIdCallback register_id_cb,
                        RigEngineDeleteIdCallback delete_id_cb,
                        void *user_data)
{
  switch (pb_op->type)
    {
    case RIG_ENGINE_OP_TYPE_SET_PROPERTY:
        {
          RutObject *object =
            id_to_object_cb (pb_op->set_property->object_id);

          RutProperty *property =
            rut_introspectable_get_property (object,
                                             pb_op->set_property->property_id);
          RutBoxed boxed;

          /* XXX: ideally we shouldn't need to init a RutBoxed and set
           * that on a property, and instead we could just directly
           * apply the value to the property we have. */
          rig_pb_init_boxed_value (&unserializer,
                                   &boxed,
                                   property->spec->type,
                                   pb_op->set_property->value);

          /* Note: at this point the logging of property changes
           * should be disabled in the simulator, so this shouldn't
           * redundantly feed-back to the frontend process. */
          rut_property_set_boxed  (prop_ctx, property, &boxed);
          break;
        }
    case RIG_ENGINE_OP_TYPE_ADD_ENTITY:
        {
          RutEntity *parent = NULL;
          RutEntity *entity;

          g_warn_if_fail (pb_op->add_entity->entity->has_parent_id == false);

          if (pb_op->add_entity->parent_entity_id)
            {
              parent = id_to_object_cb (pb_op->add_entity->parent_entity_id);
              if (!parent)
                goto ERROR;
            }

          entity = rig_pb_unserialize_entity (unserializer,
                                              pb_op->add_entity->entity);

          if (!entity)
            goto ERROR;

          register_id_cb (pb_op->add_entity->entity->id, entity, user_data);

          if (parent)
            rut_graphable_add_child (parent, entity);
          break;
        }
    case RIG_ENGINE_OP_TYPE_DELETE_ENTITY:
        {
          RutEntity *entity =
            id_to_object_cb (pb_op->delete_entity->entity_id, user_data);

          if (!entity)
            goto ERROR;

          rut_graphable_remove_child (entity);

          delete_id_cb (pb_op->add_entity->parent_entity_id, user_data);
          break;
        }
    case RIG_ENGINE_OP_TYPE_ADD_COMPONENT:
        {

          break;
        }
    case RIG_ENGINE_OP_TYPE_DELETE_COMPONENT:
        {

          break;
        }
#if 0
      //Add a component to an entity
      message AddComponent
        {
          required sint64 parent_entity_id=1;
          required Entity.Component component=2;
        }
      optional AddComponent add_component=6;

      message DeleteComponent
        {
          required sint64 component_id=1;
        }
      optional DeleteComponent delete_component=7;

      message AddController
        {
          required Controller controller=1;
        }
      optional AddController add_controller=8;

      message DeleteController
        {
          required sint64 controller_id=1;
        }
      optional DeleteController delete_controller=9;

      //Change the constant associated with a controller property
      message ControllerSetConst
        {
          required sint64 controller_id=1;
          required sint64 object_id=2;
          required int32 property_id=3;
          required PropertyValue value=4;
        }
      optional ControllerSetConst controller_set_const=10;

      //Add a new controller property path node / key frame
      message ControllerPathAddNode
        {
          required sint64 controller_id=1;
          required sint64 object_id=2;
          required int32 property_id=3;
          required float t=4;
          required PropertyValue value=5;
        }
      optional ControllerPathAddNode controller_path_add_node=11;

      //Remove a node / key frame from a controller property path
      message ControllerPathDeleteNode
        {
          required sint64 controller_id=1;
          required sint64 object_id=2;
          required int32 property_id=3;
          required float t=4;
        }
      optional ControllerPathDeleteNode controller_path_delete_node=12;

      //Change the value of a controller property path node / key frame
      message ControllerPathSetNode
        {
          required sint64 controller_id=1;
          required sint64 object_id=2;
          required int32 property_id=3;
          required float t=4;
          required PropertyValue value=5;
        }
      optional ControllerPathSetNode controller_path_set_node=13;

      //Associate a property with a controller
      message ControllerAddProperty
        {
          required sint64 controller_id=1;
          required sint64 object_id=2;
          required int32 property_id=3;
        }
      optional ControllerAddProperty controller_add_property=14;

      //Disassociate a property from a controller
      message ControllerRemoveProperty
        {
          required sint64 controller_id=1;
          required sint64 object_id=2;
          required int32 property_id=3;
        }
      optional ControllerRemoveProperty controller_remove_property=15;

      //Change the method of controlling a property
      message ControllerPropertySetMethod
        {
          required sint64 controller_id=1;
          required sint64 object_id=2;
          required int32 property_id=3;

          enum Method { CONSTANT=1; PATH=2; BINDING=3; }

          required Method method=4;
        }
      optional ControllerPropertySetMethod controller_property_set_method=16;

#endif

    case RIG_ENGINE_OP_TYPE_ADD_CONTROLLER:
        {

          break;
        }
    case RIG_ENGINE_OP_TYPE_DELETE_CONTROLLER:
        {

          break;
        }
    case RIG_ENGINE_OP_TYPE_CONTROLLER_SET_CONST:
        {

          break;
        }
    case RIG_ENGINE_OP_TYPE_CONTROLLER_PATH_ADD_NODE:
        {

          break;
        }
    case RIG_ENGINE_OP_TYPE_CONTROLLER_PATH_DELETE_NODE:
        {

          break;
        }
    case RIG_ENGINE_OP_TYPE_CONTROLLER_PATH_SET_NODE:
        {

          break;
        }
    case RIG_ENGINE_OP_TYPE_CONTROLLER_ADD_PROPERTY:
        {

          break;
        }
    case RIG_ENGINE_OP_TYPE_CONTROLLER_REMOVE_PROPERTY:
        {

          break;
        }
    case RIG_ENGINE_OP_TYPE_CONTROLLER_PROPERTY_SET_METHOD:
        {

          break;
        }
    case RIG_ENGINE_OP_TYPE_SET_PLAY_MODE:
        {
          bool play_mode_enabled = pb_op->set_play_mode->play_mode_enabled;
          rig_engine_set_play_mode_enabled (engine, play_mode_enabled);
          break;
        }
    }
}

bool
rig_engine_apply_pb_ui_edit (RigEngine *engine,
                             Rig__UIEdit *pb_ui_edit,
                             RigUI *ui,
                             RigEngineIdToObjectCallback id_to_object_cb,
                             RigEngineRegisterIdCallback register_id_cb,
                             RigEngineDeleteIdCallback delete_id_cb,
                             void *user_data)
{
  RigPBUnserializer unserializer;
  int i;

  rig_pb_unserializer_init (&unserializer, engine);

  for (i = 0; i < pb_ui_edit->n_ops; i++)
    {
      Rig__Operation *pb_op = pb_ui_edit->ops[i];

      if (!rig_engine_pb_op_apply (engine, pb_op, ui,
                                   id_to_object_cb,
                                   register_id_cb,
                                   delete_id_cb,
                                   user_data))
        {
          rig_pb_unserializer_destroy (&unserializer);
          return false;
        }
    }

  rig_pb_unserializer_destroy (&unserializer);

  return true;
}

void
rig_engine_register_play_mode_object (RigEngine *engine,
                                      uint64_t id,
                                      void *play_mode_object)
{
  void *edit_mode_object (void *)(intptr_t)id;

  g_hash_table_insert (engine->edit_to_play_object_map,
                       edit_mode_object, play_mode_object);
}

void
rig_engine_unregister_play_mode_object (RigEngine *engine,
                                        uint64_t id)
{
  void *edit_mode_object (void *)(intptr_t)id;

  g_hash_table_remove (engine->edit_to_play_object_map, edit_mode_object);
}
