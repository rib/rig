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

//#define DEVICE_WIDTH 480.0
//#define DEVICE_HEIGHT 800.0
#define DEVICE_WIDTH 720.0
#define DEVICE_HEIGHT 1280.0

static RutPropertySpec rut_data_property_specs[] = {
  {
    .name = "width",
    .flags = RUT_PROPERTY_FLAG_READABLE,
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RigEngine, width)
  },
  {
    .name = "height",
    .flags = RUT_PROPERTY_FLAG_READABLE,
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RigEngine, height)
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
CoglBool _rig_in_device_mode = FALSE;
#endif

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

  if (rut_object_is (object, RUT_INTERFACE_ID_TRANSFORMABLE))
    {
      //g_print ("%*sTransformable = %p\n", depth, "", object);
      const CoglMatrix *matrix = rut_transformable_get_matrix (object);
      //cogl_debug_matrix_print (matrix);
      cogl_framebuffer_push_matrix (fb);
      cogl_framebuffer_transform (fb, matrix);
    }

  if (rut_object_is (object, RUT_INTERFACE_ID_PAINTABLE))
    {
      RutPaintableVTable *vtable =
        rut_object_get_vtable (object, RUT_INTERFACE_ID_PAINTABLE);
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

  if (rut_object_is (object, RUT_INTERFACE_ID_TRANSFORMABLE))
    {
      cogl_framebuffer_pop_matrix (fb);
    }

  return RUT_TRAVERSE_VISIT_CONTINUE;
}

CoglBool
rig_engine_paint (RutShell *shell, void *user_data)
{
  RigEngine *engine = user_data;
  CoglFramebuffer *fb = engine->onscreen;
  RigPaintContext paint_ctx;
  RutPaintContext *rut_paint_ctx = &paint_ctx._parent;

  rut_camera_set_framebuffer (engine->camera, fb);

  cogl_framebuffer_clear4f (fb,
                            COGL_BUFFER_BIT_COLOR|COGL_BUFFER_BIT_DEPTH,
                            0.9, 0.9, 0.9, 1);

  paint_ctx.engine = engine;
  paint_ctx.renderer = engine->renderer;

  paint_ctx.pass = RIG_PASS_COLOR_BLENDED;
  rut_paint_ctx->camera = engine->camera;

  rut_camera_flush (engine->camera);
  rut_paint_graph_with_layers (engine->root,
                               scenegraph_pre_paint_cb,
                               scenegraph_post_paint_cb,
                               rut_paint_ctx);
  rut_camera_end_frame (engine->camera);

  cogl_onscreen_swap_buffers (COGL_ONSCREEN (fb));

  return FALSE;
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

  if (rut_object_is (reference_object, RUT_INTERFACE_ID_INTROSPECTABLE))
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
  rut_refable_unref (inspector);

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
  rut_refable_unref (delete_button);

  rut_box_layout_add (engine->inspector_box_layout, FALSE, fold);
  rut_refable_unref (fold);

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
        rut_object_get_properties (component, RUT_INTERFACE_ID_COMPONENTABLE);

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
                               RUT_INTERFACE_ID_COMPONENTABLE);
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
  if (!_rig_in_device_mode)
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

void
rig_set_play_mode_enabled (RigEngine *engine, CoglBool enabled)
{
  engine->play_mode = enabled;

  if (engine->play_mode)
    {
      engine->enable_dof = TRUE;
      //engine->debug_pick_ray = 0;
      if (engine->light_handle)
        rut_graphable_remove_child (engine->light_handle);
    }
  else
    {
      engine->enable_dof = FALSE;
      //engine->debug_pick_ray = 1;
      if (engine->light && engine->light_handle)
        rut_graphable_add_child (engine->light, engine->light_handle);
    }

  rut_shell_queue_redraw (engine->ctx->shell);
}

static void
_rig_objects_selection_cancel (RutObject *object)
{
  RigObjectsSelection *selection = object;
  g_list_free_full (selection->objects, (GDestroyNotify)rut_refable_unref);
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
                        (GDestroyNotify)rut_refable_unref);
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

  g_slice_free (RigObjectsSelection, selection);
}

RutType rig_objects_selection_type;

static void
_rig_objects_selection_init_type (void)
{
  static RutRefableVTable refable_vtable = {
      rut_refable_simple_ref,
      rut_refable_simple_unref,
      _rig_objects_selection_free
  };
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

  rut_type_init (type, G_STRINGIFY (TYPE));
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (TYPE, ref_count),
                          &refable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_SELECTABLE,
                          0, /* no associated properties */
                          &selectable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_MIMABLE,
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

  selection->ref_count = 1;
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
  rut_refable_unref (object);
}

void
rig_select_object (RigEngine *engine,
                   RutObject *object,
                   RutSelectAction action)
{
  RigObjectsSelection *selection = engine->objects_selection;

  /* For now we only support selecting multiple entities... */
  if (object && rut_object_get_type (object) != &rut_entity_type)
    action = RUT_SELECT_ACTION_REPLACE;

  if (object == engine->light_handle)
    object = engine->light;
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
                                                 rut_refable_ref (object));
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
            rut_refable_unref (link->data);
          }
        else if (object)
          {
            rut_closure_list_invoke (&selection->selection_events_cb_list,
                                     RigObjectsSelectionEventCallback,
                                     selection,
                                     RIG_OBJECTS_SELECTION_ADD_EVENT,
                                     object);

            rut_refable_ref (object);
            selection->objects =
              g_list_prepend (selection->objects, object);
          }
      }
      break;
    }

  if (selection->objects)
    rut_shell_set_selection (engine->shell, engine->objects_selection);

  rut_shell_queue_redraw (engine->ctx->shell);
  _rig_engine_update_inspector (engine);
}

static void
allocate (RigEngine *engine)
{
  //engine->main_width = engine->width - engine->left_bar_width - engine->right_bar_width;
  //engine->main_height = engine->height - engine->top_bar_height - engine->bottom_bar_height;

  rut_sizable_set_size (engine->top_stack, engine->width, engine->height);

#ifdef RIG_EDITOR_ENABLED
  if (!_rig_in_device_mode)
    {
      if (engine->resize_handle_transform)
        {
          RutTransform *transform = engine->resize_handle_transform;

          rut_transform_init_identity (transform);
          rut_transform_translate (transform,
                                   engine->width - 18.0f,
                                   engine->height - 18.0f,
                                   0.0f);
        }
    }
#endif

  /* Update the window camera */
  rut_camera_set_projection_mode (engine->camera, RUT_PROJECTION_ORTHOGRAPHIC);
  rut_camera_set_orthographic_coordinates (engine->camera,
                                           0, 0, engine->width, engine->height);
  rut_camera_set_near_plane (engine->camera, -1);
  rut_camera_set_far_plane (engine->camera, 100);

  rut_camera_set_viewport (engine->camera, 0, 0, engine->width, engine->height);
}

static void
data_onscreen_resize (CoglOnscreen *onscreen,
                      int width,
                      int height,
                      void *user_data)
{
  RigEngine *engine = user_data;

  engine->width = width;
  engine->height = height;

  rut_property_dirty (&engine->ctx->property_ctx, &engine->properties[RIG_ENGINE_PROP_WIDTH]);
  rut_property_dirty (&engine->ctx->property_ctx, &engine->properties[RIG_ENGINE_PROP_HEIGHT]);

  allocate (engine);
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
                                           engine->scene,
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
  rut_refable_unref (text);

  rut_color_init_from_uint32 (&color, 0xffffffff);
  rut_text_set_color (text, &color);

  rut_box_layout_add (vbox, FALSE, label_bin);
  rut_refable_unref (label_bin);

  rut_flow_layout_set_x_padding (flow, 5);
  rut_flow_layout_set_y_padding (flow, 5);
  rut_flow_layout_set_max_child_height (flow, 100);

  //rut_bin_set_left_padding (flow_bin, 5);
  rut_bin_set_child (flow_bin, flow);
  rut_refable_unref (flow);

  rut_box_layout_add (vbox, TRUE, flow_bin);
  rut_refable_unref (flow_bin);

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
  rut_refable_unref (drag_bin);

  stack = rut_stack_new (engine->ctx, 0, 0);
  rut_drag_bin_set_child (drag_bin, stack);
  rut_refable_unref (stack);

  region = rut_input_region_new_rectangle (0, 0, 100, 100,
                                           result_input_cb,
                                           closure);
  rut_stack_add (stack, region);
  rut_refable_unref (region);

  if (rut_object_get_type (result) == &rut_asset_type)
    {
      RutAsset *asset = result;

      texture = rut_asset_get_texture (asset);

      if (texture)
        {
          RutImage *image = rut_image_new (engine->ctx, texture);
          rut_stack_add (stack, image);
          rut_refable_unref (image);
        }
      else
        {
          char *basename = g_path_get_basename (rut_asset_get_path (asset));
          RutText *text = rut_text_new_with_text (engine->ctx, NULL, basename);
          rut_stack_add (stack, text);
          rut_refable_unref (text);
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
      rut_refable_unref (vbox);

#warning "Create a sensible icon to represent entities"
      texture = rut_load_texture_from_data_file (engine->ctx,
                                                 "transparency-grid.png", NULL);
      image = rut_image_new (engine->ctx, texture);
      cogl_object_unref (texture);

      rut_box_layout_add (vbox, FALSE, image);
      rut_refable_unref (image);

      text = rut_text_new_with_text (engine->ctx, NULL, entity->label);
      rut_box_layout_add (vbox, false, text);
      rut_refable_unref (text);
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
      rut_refable_unref (vbox);

#warning "Create a sensible icon to represent controllers"
      texture = rut_load_texture_from_data_file (engine->ctx,
                                                 "transparency-grid.png", NULL);
      image = rut_image_new (engine->ctx, texture);
      cogl_object_unref (texture);

      rut_box_layout_add (vbox, false, image);
      rut_refable_unref (image);

      text = rut_text_new_with_text (engine->ctx, NULL, controller->label);
      rut_box_layout_add (vbox, FALSE, text);
      rut_refable_unref (text);
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
          rut_refable_unref (bin);
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
          rut_refable_unref (bin);
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
          rut_refable_unref (bin);
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
          rut_refable_unref (bin);
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
      rut_refable_unref (bin);
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
      rut_refable_unref (bin);
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
  rut_refable_unref (engine->search_results_vbox);

  for (l = engine->assets, i= 0; l; l = l->next, i++)
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
      rut_graphable_traverse (engine->scene,
                              RUT_TRAVERSE_DEPTH_FIRST,
                              add_matching_entity_cb,
                              NULL, /* post visit */
                              &state);
    }

  if (!engine->required_search_tags ||
      rut_util_find_tag (engine->required_search_tags, "controller"))
    {
      for (l = engine->controllers; l; l = l->next)
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

      rut_refable_unref (engine->resize_handle_transform);
      rut_refable_unref (resize_handle);
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
  rut_refable_unref (engine->nine_slice_builtin_asset);
  rut_refable_unref (engine->diamond_builtin_asset);
  rut_refable_unref (engine->circle_builtin_asset);
  rut_refable_unref (engine->pointalism_grid_builtin_asset);
  rut_refable_unref (engine->text_builtin_asset);
  rut_refable_unref (engine->hair_builtin_asset);
  rut_refable_unref (engine->button_input_builtin_asset);
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
  rut_refable_unref (gradient);

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
  rut_refable_unref (connect_button);
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

  rut_refable_unref (bottom_shim);
  rut_refable_unref (bottom_stack);
  rut_refable_unref (bottom_drop);

  rut_refable_unref (left_shim);
  rut_refable_unref (left_stack);
  rut_refable_unref (left_drop);

  cogl_object_unref (bottom_drop_shadow);
  cogl_object_unref (left_drop_shadow);

  rut_refable_unref (vbox);
  rut_refable_unref (hbox);
  rut_refable_unref (gradient);
  rut_refable_unref (bin);
  rut_refable_unref (stack);
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
  rut_refable_unref (gradient);

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
  rut_refable_unref (pointer_toggle);
  rut_icon_toggle_set_add (toggle_set, rotate_toggle,
                           RIG_TOOL_ID_ROTATION);
  rut_refable_unref (rotate_toggle);

  rut_icon_toggle_set_set_selection (toggle_set, RIG_TOOL_ID_SELECTION);

  rut_icon_toggle_set_add_on_change_callback (toggle_set,
                                              tool_changed_cb,
                                              engine,
                                              NULL);  /* destroy notify */

  rut_box_layout_add (engine->toolbar_vbox, false, toggle_set);
  rut_refable_unref (toggle_set);

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
  rut_refable_unref (gradient);

  rut_bin_set_left_padding (bin, 10);
  rut_bin_set_right_padding (bin, 5);
  rut_bin_set_bottom_padding (bin, 10);
  rut_bin_set_top_padding (bin, 5);
  rut_stack_add (stack0, bin);
  rut_refable_unref (bin);

  rut_bin_set_child (bin, stack1);

  bg = rut_rectangle_new4f (engine->ctx,
                            0, 0, /* size */
                            1, 1, 1, 1);
  rut_stack_add (stack1, bg);
  rut_refable_unref (bg);

  properties_vp = rut_ui_viewport_new (engine->ctx, 0, 0);
  engine->properties_vp = properties_vp;

  rut_stack_add (stack1, properties_vp);
  rut_refable_unref (properties_vp);

  rut_ui_viewport_set_x_pannable (properties_vp, FALSE);
  rut_ui_viewport_set_y_pannable (properties_vp, TRUE);

  engine->inspector_bin = rut_bin_new (engine->ctx);
  rut_ui_viewport_add (engine->properties_vp, engine->inspector_bin);

  rut_ui_viewport_set_sync_widget (properties_vp, engine->inspector_bin);

  rut_box_layout_add (engine->properties_hbox, FALSE, stack0);
  rut_refable_unref (stack0);
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
  rut_refable_unref (toggle);

  toggle = create_search_toggle (engine,
                                 "image-white.png",
                                 "image.png",
                                 "image");
  rut_box_layout_add (hbox, FALSE, toggle);
  rut_refable_unref (toggle);

  toggle = create_search_toggle (engine,
                                 "video-white.png",
                                 "video.png",
                                 "video");
  rut_box_layout_add (hbox, FALSE, toggle);
  rut_refable_unref (toggle);

  toggle = create_search_toggle (engine,
                                 "entity-white.png",
                                 "entity.png",
                                 "entity");
  rut_box_layout_add (hbox, FALSE, toggle);
  rut_refable_unref (toggle);

  toggle = create_search_toggle (engine,
                                 "logic-white.png",
                                 "logic.png",
                                 "logic");
  rut_box_layout_add (hbox, FALSE, toggle);
  rut_refable_unref (toggle);

  rut_stack_add (icons_stack, hbox);
  rut_refable_unref (hbox);
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
  rut_refable_unref (bg);

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
  rut_refable_unref (entry);

  rut_stack_add (search_stack, search_bin);
  rut_bin_set_left_padding (search_bin, 10);
  rut_bin_set_right_padding (search_bin, 10);
  rut_bin_set_top_padding (search_bin, 2);
  rut_bin_set_bottom_padding (search_bin, 2);
  rut_refable_unref (search_bin);

  rut_box_layout_add (vbox, FALSE, search_stack);
  rut_refable_unref (search_stack);

  bg = rut_rectangle_new4f (engine->ctx, 0, 0, 0.57, 0.57, 0.57, 1);
  rut_stack_add (icons_stack, bg);
  rut_refable_unref (bg);

  create_asset_selectors (engine, icons_stack);

  rut_box_layout_add (vbox, FALSE, icons_stack);
  rut_refable_unref (icons_stack);

  rut_box_layout_add (vbox, TRUE, stack);
  rut_refable_unref (stack);

  rut_stack_add (stack, gradient);
  rut_refable_unref (gradient);


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
  rut_refable_unref (vbox);
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
controller_changed_cb (RigControllerView *view,
                       RigController *controller,
                       void *user_data)
{
  RigEngine *engine = user_data;

  if (engine->selected_controller == controller)
    return;

  if (engine->selected_controller)
    {
      rut_property_closure_destroy (engine->controller_progress_closure);
      rut_refable_unref (engine->selected_controller);
    }

  engine->selected_controller = controller;

  if (controller)
    {
      rut_refable_ref (controller);

      engine->controller_progress_closure =
        rut_property_connect_callback (&controller->props[RIG_CONTROLLER_PROP_PROGRESS],
                                       controller_progress_changed_cb,
                                       engine);
    }
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
  rut_refable_unref (engine->controller_view);
}
#endif /* RIG_EDITOR_ENABLED */

static void
ensure_light (RigEngine *engine)
{
  RutCamera *camera;

  if (!engine->light)
    {
      RutLight *light;
      float vector3[3];
      CoglColor color;

      engine->light = rut_entity_new (engine->ctx);
      rut_entity_set_label (engine->light, "light");

      vector3[0] = 0;
      vector3[1] = 0;
      vector3[2] = 500;
      rut_entity_set_position (engine->light, vector3);

      rut_entity_rotate_x_axis (engine->light, 20);
      rut_entity_rotate_y_axis (engine->light, -20);

      light = rut_light_new (engine->ctx);
      cogl_color_init_from_4f (&color, .2f, .2f, .2f, 1.f);
      rut_light_set_ambient (light, &color);
      cogl_color_init_from_4f (&color, .6f, .6f, .6f, 1.f);
      rut_light_set_diffuse (light, &color);
      cogl_color_init_from_4f (&color, .4f, .4f, .4f, 1.f);
      rut_light_set_specular (light, &color);

      rut_entity_add_component (engine->light, light);

      rut_graphable_add_child (engine->scene, engine->light);
    }

  camera = rut_entity_get_component (engine->light, RUT_COMPONENT_TYPE_CAMERA);
  if (!camera)
    {
      camera = rut_camera_new (engine->ctx, engine->shadow_fb);

      rut_camera_set_background_color4f (camera, 0.f, .3f, 0.f, 1.f);
      rut_camera_set_projection_mode (camera,
                                      RUT_PROJECTION_ORTHOGRAPHIC);
      rut_camera_set_orthographic_coordinates (camera,
                                               -1000, -1000, 1000, 1000);
      rut_camera_set_near_plane (camera, 1.1f);
      rut_camera_set_far_plane (camera, 1500.f);

      rut_entity_add_component (engine->light, camera);
    }
  else
    {
      CoglFramebuffer *fb = engine->shadow_fb;
      int width = cogl_framebuffer_get_width (fb);
      int height = cogl_framebuffer_get_height (fb);
      rut_camera_set_framebuffer (camera, fb);
      rut_camera_set_viewport (camera, 0, 0, width, height);
    }


#ifdef RIG_EDITOR_ENABLED
  if (!_rig_in_device_mode)
    {
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
          rut_graphable_add_child (engine->light, engine->light_handle);

          rut_entity_add_component (engine->light_handle, model);

          rut_entity_add_component (engine->light_handle, material);
          rut_material_set_receive_shadow (material, false);
          rut_material_set_cast_shadow (material, false);

          rut_refable_unref (model);
          rut_refable_unref (material);
        }
      else
        g_critical ("could not load model %s: %s", full_path, error->message);

      g_free (full_path);
    }
#endif

}

typedef struct
{
  const char *label;
  RutEntity *entity;
} FindEntityData;

static RutTraverseVisitFlags
find_entity_cb (RutObject *object,
                int depth,
                void *user_data)
{
  FindEntityData *data = user_data;

  if (rut_object_get_type (object) == &rut_entity_type &&
      !strcmp (data->label, rut_entity_get_label (object)))
    {
      data->entity = object;
      return RUT_TRAVERSE_VISIT_BREAK;
    }

  return RUT_TRAVERSE_VISIT_CONTINUE;
}

static RutEntity *
find_entity (RutObject *root,
             const char *label)
{
  FindEntityData data = { .label = label, .entity = NULL };

  rut_graphable_traverse (root,
                          RUT_TRAVERSE_DEPTH_FIRST,
                          find_entity_cb,
                          NULL, /* after_children_cb */
                          &data);

  return data.entity;
}

static void
initialise_play_camera_position (RigEngine *engine)
{
  float fov_y = 10; /* y-axis field of view */
  float aspect = (float) engine->device_width / (float) engine->device_height;
  float z_near = 10; /* distance to near clipping plane */
  float z_2d = 30;
  float position[3];
  float left, right, top;
  float left_2d_plane, right_2d_plane;
  float width_scale;
  float width_2d_start;

  /* Initialise the camera to the center of the device with a z
   * position that will give it pixel aligned coordinates at the
   * origin */
  top = z_near * tan (fov_y * G_PI / 360.0);
  left = -top * aspect;
  right = top * aspect;

  left_2d_plane = left / z_near * z_2d;
  right_2d_plane = right / z_near * z_2d;

  width_2d_start = right_2d_plane - left_2d_plane;

  width_scale = width_2d_start / engine->device_width;

  position[0] = engine->device_width / 2.0f;
  position[1] = engine->device_height / 2.0f;
  position[2] = z_2d / width_scale;

  rut_entity_set_position (engine->play_camera, position);
}

static void
ensure_play_camera (RigEngine *engine)
{
  if (!engine->play_camera)
    {
      RutObject *entity;

      /* Check if there is already something labelled ‘play-camera’
       * loaded from the project file */
      entity = find_entity (engine->scene, "play-camera");

      if (entity)
        engine->play_camera = rut_refable_ref (entity);
      else
        {
          engine->play_camera = rut_entity_new (engine->ctx);
          rut_entity_set_label (engine->play_camera, "play-camera");

          initialise_play_camera_position (engine);

          rut_graphable_add_child (engine->scene, engine->play_camera);
        }
    }

  if (engine->play_camera_component == NULL)
    {
      engine->play_camera_component =
        rut_entity_get_component (engine->play_camera,
                                  RUT_COMPONENT_TYPE_CAMERA);

      if (engine->play_camera_component)
        rut_refable_ref (engine->play_camera_component);
      else
        {
          engine->play_camera_component =
            rut_camera_new (engine->ctx,
                            engine->onscreen);

          rut_entity_add_component (engine->play_camera,
                                    engine->play_camera_component);
        }

      rut_camera_set_clear (engine->play_camera_component, FALSE);
    }

  rig_camera_view_set_play_camera (engine->main_camera_view,
                                   engine->play_camera);

#ifdef RIG_EDITOR_ENABLED
  if (!_rig_in_device_mode &&
      engine->play_camera_handle == NULL)
    {
      RutPLYAttributeStatus padding_status[G_N_ELEMENTS (ply_attributes)];
      char *model_path;

      model_path = rut_find_data_file ("camera-model.ply");

      if (model_path == NULL)
        g_critical ("could not find model \"camera-model.ply\"");
      else
        {
          RutMesh *mesh;
          GError *error = NULL;

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

              rut_refable_unref (model);
              rut_refable_unref (material);
              rut_refable_unref (mesh);
#endif
            }
        }
    }
#endif /* RIG_EDITOR_ENABLED */
}

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
    cogl_texture_2d_new_with_size (rut_cogl_context,
                                   200, 200,
                                   COGL_PIXEL_FORMAT_ANY);

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

void
rig_engine_handle_ui_update (RigEngine *engine)
{
  CoglTexture2D *color_buffer;

  rig_camera_view_set_scene (engine->main_camera_view, engine->scene);

  /*
   * Shadow mapping
   */

  /* Setup the shadow map */

  g_warn_if_fail (engine->shadow_color == NULL);

  color_buffer = cogl_texture_2d_new_with_size (rut_cogl_context,
                                                engine->device_width * 2,
                                                engine->device_height * 2,
                                                COGL_PIXEL_FORMAT_ANY);

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

  /* Note: we currently require having exactly one scene light and
   * play camera, so if we didn't already load them we create a default
   * light and camera...
   */
  ensure_light (engine);
  ensure_play_camera (engine);

#ifdef RIG_EDITOR_ENABLED
  if (!_rig_in_device_mode)
    {
      engine->grid_prim = rut_create_create_grid (engine->ctx,
                                                  engine->device_width,
                                                  engine->device_height,
                                                  100,
                                                  100);
    }

  if (!engine->controllers)
    {
      RigController *controller = rig_controller_new (engine, "Controller 0");
      rig_controller_set_active (controller, true);
      engine->controllers = g_list_prepend (engine->controllers, controller);
    }

  if (!_rig_in_device_mode)
    {
      rig_controller_view_update_controller_list (engine->controller_view);

      rig_controller_view_set_controller (engine->controller_view,
                                          engine->controllers->data);

      rig_load_asset_list (engine);
    }
#endif
}

void
rig_engine_free_ui (RigEngine *engine)
{
  GList *l;

#ifdef RIG_EDITOR_ENABLED
  if (!_rig_in_device_mode)
    {
      rig_controller_view_set_controller (engine->controller_view,
                                          NULL);

      if (engine->grid_prim)
        {
          cogl_object_unref (engine->grid_prim);
          engine->grid_prim = NULL;
        }

      clear_search_results (engine);
    }
#endif

  if (engine->shadow_color)
    {
      cogl_object_unref (engine->shadow_color);
      engine->shadow_color = NULL;
    }

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

  for (l = engine->controllers; l; l = l->next)
    rut_refable_unref (l->data);
  g_list_free (engine->controllers);
  engine->controllers = NULL;
  engine->selected_controller = NULL;

  for (l = engine->assets; l; l = l->next)
    rut_refable_unref (l->data);
  g_list_free (engine->assets);
  engine->assets = NULL;

  free_result_input_closures (engine);

  /* NB: no extra reference is held on the light other than the
   * reference for it being in the scenegraph. */
  engine->light = NULL;

  if (engine->scene)
    {
      rut_refable_unref (engine->scene);
      engine->scene = NULL;
    }

  if (engine->play_camera)
    {
      rut_refable_unref (engine->play_camera);
      engine->play_camera = NULL;
    }
  if (engine->play_camera_component)
    {
      rut_refable_unref (engine->play_camera_component);
      engine->play_camera_component = NULL;
    }
#ifdef RIG_EDITOR_ENABLED
  if (engine->play_camera_handle)
   {
     rut_refable_unref (engine->play_camera_handle);
     engine->play_camera_handle = NULL;
   }
#endif /* RIG_EDITOR_ENABLED */
}

void
rig_engine_set_onscreen_size (RigEngine *engine,
                              int width,
                              int height)
{
  if (engine->width == width && engine->height == height)
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

void
rig_engine_init (RutShell *shell, void *user_data)
{
  RigEngine *engine = user_data;
  CoglFramebuffer *fb;
  int i;

  cogl_matrix_init_identity (&engine->identity);

  for (i = 0; i < RIG_ENGINE_N_PROPS; i++)
    rut_property_init (&engine->properties[i],
                       &rut_data_property_specs[i],
                       engine);

#ifdef RIG_EDITOR_ENABLED
  engine->objects_selection = _rig_objects_selection_new (engine);
  engine->serialization_stack = rut_memory_stack_new (8192);

  rut_list_init (&engine->tool_changed_cb_list);

  if (!_rig_in_device_mode)
    {
      rig_engine_push_undo_subjournal (engine);

      /* Create a color gradient texture that can be used for debugging
       * shadow mapping.
       *
       * XXX: This should probably simply be #ifdef DEBUG code.
       */
      create_debug_gradient (engine);
    }
#endif /* RIG_EDITOR_ENABLED */

  engine->assets_registry = g_hash_table_new_full (g_str_hash,
                                                   g_str_equal,
                                                   g_free,
                                                   rut_refable_unref);

  load_builtin_assets (engine);

  engine->scene = rut_graph_new (engine->ctx);

  engine->root = rut_graph_new (engine->ctx);

  engine->top_stack = rut_stack_new (engine->ctx, 1, 1);
  rut_graphable_add_child (engine->root, engine->top_stack);
  rut_refable_unref (engine->top_stack);


  engine->default_pipeline = cogl_pipeline_new (engine->ctx->cogl_context);

  /*
   * Depth of Field
   */

  engine->dof = rut_dof_effect_new (engine->ctx);
  engine->enable_dof = FALSE;

  engine->circle_node_attribute =
    rut_create_circle_fan_p2 (engine->ctx, 20, &engine->circle_node_n_verts);

  /* picking ray */
  engine->picking_ray_color = cogl_pipeline_new (engine->ctx->cogl_context);
  cogl_pipeline_set_color4f (engine->picking_ray_color, 1.0, 0.0, 0.0, 1.0);

#ifdef RIG_EDITOR_ENABLED
  if (!_rig_in_device_mode)
    rig_set_play_mode_enabled (engine, FALSE);
  else
#endif
    rig_set_play_mode_enabled (engine, TRUE);

  engine->camera = rut_camera_new (engine->ctx, NULL);
  rut_camera_set_clear (engine->camera, FALSE);

  /* XXX: Basically just a hack for now. We should have a
   * RutShellWindow type that internally creates a RutCamera that can
   * be used when handling input events in device coordinates.
   */
  rut_shell_set_window_camera (shell, engine->camera);

  rut_shell_add_input_camera (shell, engine->camera, engine->root);

#ifdef RIG_EDITOR_ENABLED
  if (!_rig_in_device_mode)
      create_editor_ui (engine);
  else
#endif
    {
      engine->main_camera_view = rig_camera_view_new (engine);
      rut_stack_add (engine->top_stack, engine->main_camera_view);
    }

  engine->renderer = rig_renderer_new (engine);
  rig_renderer_init (engine);

  engine->device_width = DEVICE_WIDTH;
  engine->device_height = DEVICE_HEIGHT;
  cogl_color_init_from_4f (&engine->background_color, 0, 0, 0, 1);



#ifndef __ANDROID__
  if (engine->ui_filename)
    {
      struct stat st;

      stat (engine->ui_filename, &st);
      if (S_ISREG (st.st_mode))
        rig_load (engine, engine->ui_filename);
      else
        rig_engine_handle_ui_update (engine);
    }
#endif

#ifdef RIG_EDITOR_ENABLED
  if (!_rig_in_device_mode)
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
                                     data_onscreen_resize,
                                     engine,
                                     NULL);

  cogl_framebuffer_allocate (engine->onscreen, NULL);

  fb = engine->onscreen;
  engine->width = cogl_framebuffer_get_width (fb);
  engine->height  = cogl_framebuffer_get_height (fb);

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

  allocate (engine);
}

void
rig_engine_fini (RutShell *shell, void *user_data)
{
  RigEngine *engine = user_data;
  int i;

  rig_renderer_fini (engine);

  rig_engine_free_ui (engine);

  free_builtin_assets (engine);

  rut_shell_remove_input_camera (shell, engine->camera, engine->root);

  rut_refable_unref (engine->camera);
  rut_refable_unref (engine->root);
  rut_refable_unref (engine->main_camera_view);

  for (i = 0; i < RIG_ENGINE_N_PROPS; i++)
    rut_property_destroy (&engine->properties[i]);

  cogl_object_unref (engine->circle_node_attribute);

  rut_dof_effect_free (engine->dof);

#ifdef RIG_EDITOR_ENABLED
  if (!_rig_in_device_mode)
    {
      for (i = 0; i < G_N_ELEMENTS (engine->splits); i++)
        rut_refable_unref (engine->splits[i]);

      rut_refable_unref (engine->top_vbox);
      rut_refable_unref (engine->top_hbox);
      rut_refable_unref (engine->asset_panel_hbox);
      rut_refable_unref (engine->properties_hbox);

      if (engine->transparency_grid)
        rut_refable_unref (engine->transparency_grid);
    }

  rut_refable_unref (engine->objects_selection);

  rut_closure_list_disconnect_all (&engine->tool_changed_cb_list);
#endif

  cogl_object_unref (engine->onscreen);

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
      if (!_rig_in_device_mode &&
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
                  rig_select_object (engine, engine->play_camera, RUT_SELECT_ACTION_REPLACE);
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
                       rut_refable_ref (asset));
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

  if (asset && !_rig_in_device_mode && rut_asset_needs_thumbnail (asset))
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
  for (l = engine->assets; l; l = l->next)
    {
      RutAsset *existing = l->data;

      if (strcmp (rut_asset_get_path (existing), path) == 0)
        return;
    }

  asset = rig_load_asset (engine, info, asset_file);
  if (asset)
    engine->assets = g_list_prepend (engine->assets, asset);
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

  rut_refable_ref (engine->nine_slice_builtin_asset);
  engine->assets = g_list_prepend (engine->assets,
                                   engine->nine_slice_builtin_asset);

  rut_refable_ref (engine->diamond_builtin_asset);
  engine->assets = g_list_prepend (engine->assets,
                                   engine->diamond_builtin_asset);

  rut_refable_ref (engine->circle_builtin_asset);
  engine->assets = g_list_prepend (engine->assets,
                                   engine->circle_builtin_asset);

  rut_refable_ref (engine->pointalism_grid_builtin_asset);
  engine->assets = g_list_prepend (engine->assets,
                                   engine->pointalism_grid_builtin_asset);

  rut_refable_ref (engine->text_builtin_asset);
  engine->assets = g_list_prepend (engine->assets,
                                   engine->text_builtin_asset);

  rut_refable_ref (engine->hair_builtin_asset);
  engine->assets = g_list_prepend (engine->assets,
                                   engine->hair_builtin_asset);

  rut_refable_ref (engine->button_input_builtin_asset);
  engine->assets = g_list_prepend (engine->assets,
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
    rig_slave_master_sync_ui (l->data);
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
