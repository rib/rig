/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012,2013  Intel Corporation.
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
#include <cogl/cogl-sdl.h>

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
#include "rig-image-source.h"
#include "rig-inspector.h"

#include "components/rig-camera.h"

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

static RigObjectsSelection *
_rig_objects_selection_new (RigEngine *engine);

static RutTraverseVisitFlags
scenegraph_pre_paint_cb (RutObject *object,
                         int depth,
                         void *user_data)
{
  RutPaintContext *rut_paint_ctx = user_data;
  RutObject *camera = rut_paint_ctx->camera;
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
        rig_inspector_reload_property (l->data, property);
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

/* TODO: Move into rig-editor.c */
void
_rig_engine_update_inspector (RigEngine *engine)
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
rig_engine_dirty_properties_menu (RigImageSource *source,
                                  void *user_data)
{
#ifdef RIG_EDITOR_ENABLED
  RigEngine *engine = user_data;
  if (engine->frontend && engine->frontend_id == RIG_FRONTEND_ID_EDITOR)
    _rig_engine_update_inspector (engine);
#endif
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
rig_engine_set_current_ui (RigEngine *engine,
                           RigUI *ui)
{
  rig_camera_view_set_ui (engine->main_camera_view, ui);
  engine->current_ui = ui;
  rut_shell_queue_redraw (engine->ctx->shell);
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
    _rig_engine_update_inspector (engine);
}

static void
allocate (RigEngine *engine)
{
  //engine->main_width = engine->window_width - engine->left_bar_width - engine->right_bar_width;
  //engine->main_height = engine->window_height - engine->top_bar_height - engine->bottom_bar_height;

  rut_sizable_set_size (engine->top_stack, engine->window_width, engine->window_height);

#ifdef RIG_EDITOR_ENABLED
  if (engine->frontend && engine->frontend_id == RIG_FRONTEND_ID_EDITOR)
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

  g_return_if_fail (engine->simulator == NULL);

  rig_engine_resize (engine, width, height);
}

static void
load_builtin_assets (RigEngine *engine)
{

  engine->nine_slice_builtin_asset = rig_asset_new_builtin (engine->ctx, "nine-slice.png");
  rig_asset_add_inferred_tag (engine->nine_slice_builtin_asset, "nine-slice");
  rig_asset_add_inferred_tag (engine->nine_slice_builtin_asset, "builtin");
  rig_asset_add_inferred_tag (engine->nine_slice_builtin_asset, "geom");
  rig_asset_add_inferred_tag (engine->nine_slice_builtin_asset, "geometry");

  engine->diamond_builtin_asset = rig_asset_new_builtin (engine->ctx, "diamond.png");
  rig_asset_add_inferred_tag (engine->diamond_builtin_asset, "diamond");
  rig_asset_add_inferred_tag (engine->diamond_builtin_asset, "builtin");
  rig_asset_add_inferred_tag (engine->diamond_builtin_asset, "geom");
  rig_asset_add_inferred_tag (engine->diamond_builtin_asset, "geometry");

  engine->circle_builtin_asset = rig_asset_new_builtin (engine->ctx, "circle.png");
  rig_asset_add_inferred_tag (engine->circle_builtin_asset, "shape");
  rig_asset_add_inferred_tag (engine->circle_builtin_asset, "circle");
  rig_asset_add_inferred_tag (engine->circle_builtin_asset, "builtin");
  rig_asset_add_inferred_tag (engine->circle_builtin_asset, "geom");
  rig_asset_add_inferred_tag (engine->circle_builtin_asset, "geometry");

  engine->pointalism_grid_builtin_asset = rig_asset_new_builtin (engine->ctx,
                                            "pointalism.png");
  rig_asset_add_inferred_tag (engine->pointalism_grid_builtin_asset, "grid");
  rig_asset_add_inferred_tag (engine->pointalism_grid_builtin_asset,
                              "pointalism");
  rig_asset_add_inferred_tag (engine->pointalism_grid_builtin_asset, "builtin");
  rig_asset_add_inferred_tag (engine->pointalism_grid_builtin_asset, "geom");
  rig_asset_add_inferred_tag (engine->pointalism_grid_builtin_asset,
                              "geometry");

  engine->text_builtin_asset = rig_asset_new_builtin (engine->ctx, "fonts.png");
  rig_asset_add_inferred_tag (engine->text_builtin_asset, "text");
  rig_asset_add_inferred_tag (engine->text_builtin_asset, "label");
  rig_asset_add_inferred_tag (engine->text_builtin_asset, "builtin");
  rig_asset_add_inferred_tag (engine->text_builtin_asset, "geom");
  rig_asset_add_inferred_tag (engine->text_builtin_asset, "geometry");

  engine->hair_builtin_asset = rig_asset_new_builtin (engine->ctx, "hair.png");
  rig_asset_add_inferred_tag (engine->hair_builtin_asset, "hair");
  rig_asset_add_inferred_tag (engine->hair_builtin_asset, "builtin");

  engine->button_input_builtin_asset = rig_asset_new_builtin (engine->ctx,
                                                               "button.png");
  rig_asset_add_inferred_tag (engine->button_input_builtin_asset, "button");
  rig_asset_add_inferred_tag (engine->button_input_builtin_asset, "builtin");
  rig_asset_add_inferred_tag (engine->button_input_builtin_asset, "input");
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
    cogl_texture_2d_new_with_size (engine->ctx->cogl_context, 200, 200);

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
rig_engine_set_play_mode_ui (RigEngine *engine,
                             RigUI *ui)
{
  if (engine->frontend)
    g_return_if_fail (engine->frontend->ui_update_pending == false);

  //bool first_ui = (engine->edit_mode_ui == NULL &&
  //                 engine->play_mode_ui == NULL &&
  //                 ui != NULL);

  if (engine->play_mode_ui == ui)
    return;

  if (engine->play_mode_ui)
    {
      rig_ui_reap (engine->play_mode_ui);
      rut_object_release (engine->play_mode_ui, engine);
      engine->play_mode_ui = NULL;
    }

  if (ui)
    {
      engine->play_mode_ui = rut_object_claim (ui, engine);
      rig_code_update_dso (engine, ui->dso_data, ui->dso_len);
    }

  //if (engine->edit_mode_ui == NULL && engine->play_mode_ui == NULL)
  //  free_shared_ui_state (engine);

  if (engine->play_mode)
    {
      rig_engine_set_current_ui (engine, ui);
      rig_ui_resume (ui);
    }
  else if (ui)
    rig_ui_suspend (ui);

  //if (!ui)
  //  return;

  //if (first_ui)
  //  setup_shared_ui_state (engine);
}

void
rig_engine_set_edit_mode_ui (RigEngine *engine,
                             RigUI *ui)
{
  g_return_if_fail (engine->simulator ||
                    engine->frontend->ui_update_pending == false);
  g_return_if_fail (engine->play_mode == false);

  //bool first_ui = (engine->edit_mode_ui == NULL &&
  //                 engine->play_mode_ui == NULL &&
  //                 ui != NULL);

  if (engine->edit_mode_ui == ui)
    return;

  g_return_if_fail (engine->frontend_id == RIG_FRONTEND_ID_EDITOR);

#if RIG_EDITOR_ENABLED
  /* Updating the edit mode ui implies we need to also replace
   * any play mode ui too... */
  rig_engine_set_play_mode_ui (engine, NULL);

  if (engine->frontend)
    {
      rig_controller_view_set_controller (engine->controller_view,
                                          NULL);

      rig_editor_clear_search_results (engine);
      rig_editor_free_result_input_closures (engine);

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
    {
      rig_ui_reap (engine->edit_mode_ui);
      rut_object_release (engine->edit_mode_ui, engine);
    }
  engine->edit_mode_ui = rut_object_claim (ui, engine);

  //if (engine->edit_mode_ui == NULL && engine->play_mode_ui == NULL)
  //  free_shared_ui_state (engine);

  rig_engine_set_current_ui (engine, engine->edit_mode_ui);
  rig_ui_resume (ui);

  //if (!ui)
  //  return;

  //if (first_ui)
  //  setup_shared_ui_state (engine);

#endif /* RIG_EDITOR_ENABLED */
}

void
rig_engine_set_ui_load_callback (RigEngine *engine,
                                 void (*callback) (void *user_data),
                                 void *user_data)
{
  engine->ui_load_callback = callback;
  engine->ui_load_data = user_data;
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

  color_buffer = cogl_texture_2d_new_with_size (engine->ctx->cogl_context,
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
      _rig_code_fini (engine);

      rig_renderer_fini (engine);

      cogl_object_unref (engine->circle_node_attribute);

      free_shadow_map (engine);

      cogl_object_unref (engine->onscreen);

      cogl_object_unref (engine->default_pipeline);

      _rig_destroy_image_source_wrappers (engine);

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

  if (engine->queued_deletes->len)
    {
      g_warning ("Leaking %d un-garbage-collected objects",
                 engine->queued_deletes->len);
    }
  rut_queue_free (engine->queued_deletes);

  rig_pb_serializer_destroy (engine->ops_serializer);

  rut_memory_stack_free (engine->frame_stack);
  rut_memory_stack_free (engine->sim_frame_stack);

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

static void
finish_ui_load (RigEngine *engine,
                RigUI *ui)
{
  if (engine->frontend_id == RIG_FRONTEND_ID_EDITOR)
    rig_engine_set_edit_mode_ui (engine, ui);
  else
    rig_engine_set_play_mode_ui (engine, ui);

  rut_object_unref (ui);

  if (engine->ui_load_callback)
    engine->ui_load_callback (engine->ui_load_data);
}

static void
finish_ui_load_cb (RigFrontend *frontend,
                   void *user_data)
{
  RigUI *ui = user_data;
  RigEngine *engine = frontend->engine;

  rut_closure_disconnect (engine->finish_ui_load_closure);
  engine->finish_ui_load_closure = NULL;

  finish_ui_load (engine, ui);
}

void
rig_engine_load_file (RigEngine *engine,
                      const char *filename)
{
  RigUI *ui;

  g_return_if_fail (engine->frontend);

  engine->ui_filename = g_strdup (filename);

  ui = rig_load (engine, filename);

  if (!ui)
    {
      ui = rig_ui_new (engine);
      rig_ui_prepare (ui);
    }

  /*
   * Wait until the simulator is idle before swapping in a new UI...
   */
  if (!engine->frontend->ui_update_pending)
    finish_ui_load (engine, ui);
  else
    {
      /* Throw away any outstanding closure since it is now redundant... */
      if (engine->finish_ui_load_closure)
        rut_closure_disconnect (engine->finish_ui_load_closure);

      engine->finish_ui_load_closure =
        rig_frontend_add_ui_update_callback (engine->frontend,
                                             finish_ui_load_cb,
                                             ui,
                                             rut_object_unref); /* destroy */
    }
}

static RigEngine *
_rig_engine_new_full (RutShell *shell,
                      const char *ui_filename,
                      RigFrontend *frontend,
                      RigSimulator *simulator,
                      bool play_mode)
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

  engine->object_id_magazine = rut_magazine_new (sizeof (uint64_t), 1000);

  /* The frame stack is a very cheap way to allocate memory that will
   * be automatically freed at the end of the next frame (or current
   * frame if one is already being processed.)
   */
  engine->frame_stack = rut_memory_stack_new (8192);

  /* Since the frame rate of the frontend may not match the frame rate
   * of the simulator, we maintain a separate frame stack for
   * allocations whose lifetime is tied to a simulation frame, not a
   * frontend frame...
   */
  if (frontend)
    engine->sim_frame_stack = rut_memory_stack_new (8192);

  engine->ops_serializer = rig_pb_serializer_new (engine);

  if (frontend)
    {
      /* By default a RigPBSerializer will use engine->frame_stack,
       * but operations generated in a frontend need to be batched
       * until they can be sent to the simulator which may be longer
       * than one frontend frame so we need to use the sim_frame_stack
       * instead...
       */
      rig_pb_serializer_set_stack (engine->ops_serializer,
                                   engine->sim_frame_stack);
    }

  rig_pb_serializer_set_use_pointer_ids_enabled (engine->ops_serializer, true);

  engine->queued_deletes = rut_queue_new ();

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

  engine->camera_2d = rig_camera_new (engine,
                                      -1, /* ortho/vp width */
                                      -1, /* ortho/vp height */
                                      NULL);
  rut_camera_set_clear (engine->camera_2d, false);

  /* XXX: Basically just a hack for now. We should have a
   * RutShellWindow type that internally creates a RigCamera that can
   * be used when handling input events in device coordinates.
   */
  rut_shell_set_window_camera (shell, engine->camera_2d);

  rut_shell_add_input_camera (shell, engine->camera_2d, engine->root);

  _rig_code_init (engine);

#ifdef RIG_EDITOR_ENABLED
  /* NB: The simulator also needs to track selections when in support
   * of an editor. */
  engine->objects_selection = _rig_objects_selection_new (engine);

  if (frontend && engine->frontend_id == RIG_FRONTEND_ID_EDITOR)
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

      rig_editor_create_ui (engine);
    }
  else
#endif
    {
      engine->main_camera_view = rig_camera_view_new (engine);
      rut_stack_add (engine->top_stack, engine->main_camera_view);
    }

  /* Initialize the current mode */
  rig_engine_set_play_mode_enabled (engine, play_mode);

  if (frontend)
    {
      engine->default_pipeline = cogl_pipeline_new (engine->ctx->cogl_context);

      engine->circle_node_attribute =
        rut_create_circle_fan_p2 (engine->ctx, 20, &engine->circle_node_n_verts);

      _rig_init_image_source_wrappers_cache (engine);

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
              finish_ui_load (engine, ui);
            }
        }
#endif

#ifdef RIG_EDITOR_ENABLED
      if (engine->frontend_id == RIG_FRONTEND_ID_EDITOR)
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
      engine->window_height = cogl_framebuffer_get_height (fb);

      /* FIXME: avoid poking into engine->frontend here... */
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

RigEngine *
rig_engine_new_for_simulator (RutShell *shell,
                              RigSimulator *simulator,
                              bool play_mode)
{
  return _rig_engine_new_full (shell, NULL, NULL, simulator, play_mode);
}

RigEngine *
rig_engine_new_for_frontend (RutShell *shell,
                             RigFrontend *frontend,
                             const char *ui_filename,
                             bool play_mode)
{
  return _rig_engine_new_full (shell, ui_filename, frontend, NULL, play_mode);
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
      if (engine->frontend && engine->frontend_id == RIG_FRONTEND_ID_EDITOR &&
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
                  RigEntity *play_camera = engine->play_mode ?
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
    case RUT_INPUT_EVENT_TYPE_DROP_OFFER:
    case RUT_INPUT_EVENT_TYPE_DROP:
    case RUT_INPUT_EVENT_TYPE_DROP_CANCEL:
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
                    RigAsset *asset)
{
  char *key = g_strdup (rig_asset_get_path (asset));

  g_hash_table_insert (engine->assets_registry,
                       key,
                       rut_object_ref (asset));
}

RigAsset *
rig_lookup_asset (RigEngine *engine,
                  const char *path)
{
  return g_hash_table_lookup (engine->assets_registry, path);
}

RigAsset *
rig_load_asset (RigEngine *engine, GFileInfo *info, GFile *asset_file)
{
  GFile *assets_dir = g_file_new_for_path (engine->ctx->assets_location);
  GFile *dir = g_file_get_parent (asset_file);
  char *path = g_file_get_relative_path (assets_dir, asset_file);
  GList *inferred_tags = NULL;
  RigAsset *asset = NULL;

  inferred_tags = rut_infer_asset_tags (engine->ctx, info, asset_file);

  if (rut_util_find_tag (inferred_tags, "image") ||
      rut_util_find_tag (inferred_tags, "video"))
    {
      if (rut_util_find_tag (inferred_tags, "normal-maps"))
        asset = rig_asset_new_normal_map (engine->ctx, path, inferred_tags);
      else if (rut_util_find_tag (inferred_tags, "alpha-masks"))
        asset = rig_asset_new_alpha_mask (engine->ctx, path, inferred_tags);
      else
        asset = rig_asset_new_texture (engine->ctx, path, inferred_tags);
    }
  else if (rut_util_find_tag (inferred_tags, "ply"))
    asset = rig_asset_new_ply_model (engine->ctx, path, inferred_tags);

#ifdef RIG_EDITOR_ENABLED
  if (engine->frontend &&
      engine->frontend_id == RIG_FRONTEND_ID_EDITOR &&
      asset &&
      rig_asset_needs_thumbnail (asset))
    {
      rig_asset_thumbnail (asset, rig_editor_refresh_thumbnails, engine, NULL);
    }
#endif

  g_list_free (inferred_tags);

  g_object_unref (assets_dir);
  g_object_unref (dir);
  g_free (path);

  return asset;
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

void
rig_engine_set_apply_op_callback (RigEngine *engine,
                                  void (*callback) (Rig__Operation *pb_op,
                                                    void *user_data),
                                  void *user_data)
{
  engine->apply_op_callback = callback;
  engine->apply_op_data = user_data;
}

void
rig_engine_queue_delete (RigEngine *engine,
                         RutObject *object)
{
  rut_object_claim (object, engine);
  rut_queue_push_tail (engine->queued_deletes, object);
}

void
rig_engine_garbage_collect (RigEngine *engine,
                            void (*object_callback) (RutObject *object,
                                                     void *user_data),
                            void *user_data)
{
  RutQueueItem *item;

  rut_list_for_each (item, &engine->queued_deletes->items, list_node)
    {
      if (object_callback)
        object_callback (item->data, user_data);
      rut_object_release (item->data, engine);
    }
  rut_queue_clear (engine->queued_deletes);
}

void
rig_engine_set_play_mode_enabled (RigEngine *engine, bool enabled)
{
  engine->play_mode = enabled;

  if (engine->play_mode)
    {
      if (engine->play_mode_ui)
        rig_ui_resume (engine->play_mode_ui);
      rig_engine_set_current_ui (engine, engine->play_mode_ui);
      rig_camera_view_set_play_mode_enabled (engine->main_camera_view, true);
    }
  else
    {
      rig_engine_set_current_ui (engine, engine->edit_mode_ui);
      rig_camera_view_set_play_mode_enabled (engine->main_camera_view, false);
      if (engine->play_mode_ui)
        rig_ui_suspend (engine->play_mode_ui);
    }

  if (engine->play_mode_callback)
    engine->play_mode_callback (enabled, engine->play_mode_data);
}

char *
rig_engine_get_object_debug_name (RutObject *object)
{
  if (rut_object_get_type (object) == &rig_entity_type)
    return g_strdup_printf ("%p(label=\"%s\")", object, rig_entity_get_label (object));
  else if (rut_object_is (object, RUT_TRAIT_ID_COMPONENTABLE))
    {
      RutComponentableProps *component_props =
        rut_object_get_properties (object, RUT_TRAIT_ID_COMPONENTABLE);
      RigEntity *entity = component_props->entity;
      const char *entity_label;
      if (entity)
        entity_label = entity ? rig_entity_get_label (entity) : "";
      return g_strdup_printf ("%p(label=\"%s\"::%s)", entity, entity_label,
                              rut_object_get_type_name (object));
    }
  else
    return g_strdup_printf ("%p(%s)", object, rut_object_get_type_name (object));
}

void
rig_engine_set_play_mode_callback (RigEngine *engine,
                                   void (*callback) (bool play_mode,
                                                     void *user_data),
                                   void *user_data)
{
  engine->play_mode_callback = callback;
  engine->play_mode_data = user_data;
}

