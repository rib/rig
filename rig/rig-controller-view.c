/*
 * Rig
 *
 * Copyright (C) 2012 Intel Corporation.
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
 *
 */

#include <config.h>

#include <math.h>

#include <cogl-path/cogl-path.h>

#include "rut.h"

#include "rig-controller-view.h"
#include "rig-undo-journal.h"
#include "rig-engine.h"
#include "rig-binding-view.h"

#define RIG_CONTROLLER_VIEW_N_PROPERTY_COLUMNS 3

#define RIG_CONTROLLER_VIEW_PROPERTY_INDENTATION 10

#define RIG_CONTROLLER_VIEW_UNSELECTED_COLOR \
  GUINT32_TO_BE (0x000000ff)
#define RIG_CONTROLLER_VIEW_SELECTED_COLOR \
  GUINT32_TO_BE (0x007dc4ff)

#define RIG_CONTROLLER_VIEW_PADDING 2

typedef struct
{
  RutObject *transform;
  RutObject *control;
  RutClosure *control_preferred_size_closure;
} RigControllerViewColumn;

/* When the user clicks on the area with the dots then we'll delay
 * deciding what action to take until the next mouse event. This enum
 * tracks whether we've decided the action or not */
typedef enum
{
  /* The mouse button isn't down and we're not grabbing input */
  RIG_CONTROLLER_VIEW_GRAB_STATE_NO_GRAB,
  /* There hasn't been an event yet since the button press event */
  RIG_CONTROLLER_VIEW_GRAB_STATE_UNDECIDED,
  /* We've decided to grab the selected nodes */
  RIG_CONTROLLER_VIEW_GRAB_STATE_DRAGGING_NODES,
  /* We've decided to move the timeline position */
  RIG_CONTROLLER_VIEW_GRAB_STATE_MOVING_TIMELINE,
  /* The user is drawing a bounding box to select nodes */
  RIG_CONTROLLER_VIEW_GRAB_STATE_DRAW_BOX
} RigControllerViewGrabState;

typedef struct _RigControllerObjectView RigControllerObjectView;

typedef struct
{
  RutObjectBase _base;


  RutGraphableProps graphable;

  float width;
  float height;

  RutList preferred_size_cb_list;

  RutList list_node;

  /* Pointer back to the parent object */
  RigControllerObjectView *object;

  RigControllerPropData *prop_data;

  RutDropDown *method_drop_down;

  RigControllerViewColumn columns[RIG_CONTROLLER_VIEW_N_PROPERTY_COLUMNS];

  RutStack *stack;
  RutRectangle *bg;
  RutTransform *columns_parent;;

  bool selected;

  /* Used to temporarily ignore notifications of control changes in
   * cases where we are updating the controls ourselves, to avoid
   * recursion. */
  bool internal_method_change;
} RigControllerPropertyView;

struct _RigControllerObjectView
{
  RutObjectBase _base;


  RutGraphableProps graphable;

  RutList list_node;

  RutObject *object;

  RutProperty *label_property;

  GList *properties;

  RigControllerPropertyView *selected_property;

  RutStack *stack;
  RutFold *fold;
  RutBoxLayout *properties_vbox;

  RigControllerView *view;
};

typedef struct _RigPathView
{
  RutObjectBase _base;


  float width;
  float height;

  RutGraphableProps graphable;
  RutPaintableProps paintable;

  RutUIViewport *ui_viewport;
  RutInputRegion *input_region;

  RigControllerPropertyView *prop_view;

  RutList preferred_size_cb_list;

  RigPath *path;
  RutClosure *path_operation_closure;

  RutPropertyClosure *scale_offset_prop_closure;
  RutPropertyClosure *scale_prop_closure;
  RutPropertyClosure *scale_len_prop_closure;

  RutTransform *markers;

} RigPathView;

typedef struct _RigNodeMarker
{
  RutObjectBase _base;


  RutGraphableProps graphable;

  RigPathView *path_view;

  RigPath *path;
  RigNode *node;

  RutNineSlice *rect;
  RutInputRegion *input_region;

  bool selected;

} RigNodeMarker;

typedef struct _RigNodesSelection RigNodesSelection;

typedef struct _NodeGroup
{
  RigNodesSelection *selection;
  const RutPropertySpec *prop_spec;
  RigPath *path;
  GList *nodes;
} NodeGroup;

typedef struct _NodeMapping
{
  NodeGroup *node_group;
  RigNodeMarker *marker;
} NodeMapping;

struct _RigNodesSelection
{
  RutObjectBase _base;

  RigControllerView *view;
  GList *node_groups;

  /* Nodes aren't directly connected to markers since Nodes
   * aren't expected to have any associated UI at runtime
   * when deploying a UI so we use a hash table here to
   * create our own mapping from Nodes to NodeGroups and
   * to Markers.
   */
  GHashTable *node_map;
};

struct _RigControllerView
{
  RutObjectBase _base;


  RigEngine *engine;
  RutContext *context;

  RutGraphableProps graphable;

  RutBoxLayout *vbox;
  RutDropDown *controller_selector;
  RutUIViewport *properties_vp;
  RutBoxLayout *properties_vbox;
  RutBoxLayout *header_hbox;
  RutScale *scale;

  RigController *controller;
  RutClosure *controller_op_closure;
  RigUndoJournal *undo_journal;

  RigControllerObjectView *selected_object;

  RutList controller_changed_cb_list;

  /* Position and size of the current bounding box. The x positions
   * are in normalised time and the y positions are an integer row
   * number */
  float box_x1, box_x2;
  int box_y1, box_y2;

  CoglPipeline *box_pipeline;
  CoglPath *box_path;

  int nodes_x;
  int nodes_width;
  int total_width;
  int total_height;

  GList *object_views;

  RigNodesSelection *nodes_selection;

  CoglPipeline *separator_pipeline;
  int separator_width;

  CoglPipeline *path_bg_pipeline;
  int nodes_grid_width;
  int nodes_grid_height;

  RutShim *properties_label_shim;

  float column_widths[RIG_CONTROLLER_VIEW_N_PROPERTY_COLUMNS];
};

typedef struct
{
  RutList list_node;

  RigControllerPropertyView *prop_view;
  RigNode *node;

  /* While dragging nodes, this will be used to store the original
   * time that the node had */
  float original_time;
} RigControllerViewSelectedNode;


typedef void (* RigControllerViewNodeCallback) (RigPathView *path_view,
                                                RigNode *node,
                                                void *user_data);

static void
_rig_controller_view_foreach_node (RigControllerView *view,
                                   RigControllerViewNodeCallback callback,
                                   void *user_data);

static RigNodesSelection *
_rig_nodes_selection_new (RigControllerView *view);

static RigNodeMarker *
rig_path_view_find_node_marker (RigPathView *path_view,
                                RigNode *node);


static void
_rig_node_marker_free (RutObject *object)
{
  RigNodeMarker *marker = object;

  rut_graphable_destroy (marker);

  rut_object_free (RigNodeMarker, marker);
}

RutType rig_node_marker_type;

static void
_rig_node_marker_init_type (void)
{

  static RutGraphableVTable graphable_vtable = { 0 };

  RutType *type = &rig_node_marker_type;
#define TYPE RigNodeMarker

  rut_type_init (type, G_STRINGIFY (TYPE), _rig_node_marker_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_GRAPHABLE,
                      offsetof (TYPE, graphable),
                      &graphable_vtable);

#undef TYPE
}

static void
destroy_node_group (NodeGroup *node_group)
{
  GList *l;

  if (node_group->nodes)
    {
      for (l = node_group->nodes; l; l = l->next)
        rig_node_free (l->data);
      g_list_free (node_group->nodes);
    }

  if (node_group->path)
    rut_object_unref (node_group->path);

  g_slice_free (NodeGroup, node_group);
}

static void
_rig_node_marker_set_selected (RigNodeMarker *marker,
                               bool selected)
{
  CoglPipeline *pipeline;

  if (marker->selected == selected)
    return;

  if (selected)
    {
      pipeline = rut_nine_slice_get_pipeline (marker->rect);
      cogl_pipeline_set_color4f (pipeline, 1, 1, 0, 1);
    }
  else
    {
      pipeline = rut_nine_slice_get_pipeline (marker->rect);
      cogl_pipeline_set_color4f (pipeline, 1, 1, 1, 1);
    }

  marker->selected = selected;
}

static bool
unselect_node (RigNodesSelection *selection,
               RigNode *node)
{
  NodeMapping *mapping = g_hash_table_lookup (selection->node_map, node);
  NodeGroup *node_group;
  GList *l;

  if (!mapping)
    return false;

  node_group = mapping->node_group;

  for (l = node_group->nodes; l; l = l->next)
    {
      if (l->data == node)
        {
          node_group->nodes =
            g_list_remove_link (node_group->nodes, l);

          if (node_group->nodes == NULL)
            {
              selection->node_groups =
                g_list_remove (selection->node_groups, node_group);
              destroy_node_group (node_group);
            }

          break;
        }
    }

  _rig_node_marker_set_selected (mapping->marker, false);

  g_hash_table_remove (selection->node_map, node);

  return true;
}

static void
_rig_nodes_selection_cancel (RutObject *object)
{
  RigNodesSelection *selection = object;
  GList *l, *next;

  for (l = selection->node_groups; l; l = next)
    {
      NodeGroup *node_group = l->data;
      GList *l2, *next2;

      next = l->next;

      for (l2 = node_group->nodes; l2; l2 = next2)
        {
          next2 = l2->next;
          unselect_node (selection, l2->data);
        }
    }

  g_warn_if_fail (selection->node_groups == NULL);
}

static void
select_marker_node (RigNodesSelection *selection,
                    RigNodeMarker *marker)
{
  GList *l;
  NodeGroup *node_group;
  NodeMapping *mapping;

  for (l = selection->node_groups; l; l = l->next)
    {
      node_group = l->data;

      if (node_group->path == marker->path)
        {
          node_group->nodes =
            g_list_prepend (node_group->nodes, marker->node);
          goto grouped;
        }
    }

  node_group = g_slice_new (NodeGroup);
  node_group->selection = selection;
  node_group->path = rut_object_ref (marker->path);
  node_group->nodes = NULL;
  node_group->nodes = g_list_prepend (NULL, marker->node);

  selection->node_groups =
    g_list_prepend (selection->node_groups, node_group);

grouped:

  mapping = g_slice_new (NodeMapping);
  mapping->marker = rut_object_ref (marker);
  mapping->node_group = node_group;

  g_hash_table_insert (selection->node_map,
                       marker->node,
                       mapping);

  _rig_node_marker_set_selected (marker, true);
}

static void
_rig_controller_view_select_marker (RigControllerView *view,
                                    RigNodeMarker *marker,
                                    RutSelectAction action)
{
  RigNodesSelection *selection = view->nodes_selection;
  RutShell *shell = view->context->shell;

  switch (action)
    {
    case RUT_SELECT_ACTION_REPLACE:
      {
        _rig_nodes_selection_cancel (selection);

        if (marker)
          select_marker_node (selection, marker);
        break;
      }
    case RUT_SELECT_ACTION_TOGGLE:
      {
        g_return_if_fail (marker);

        if (!unselect_node (selection, marker->node))
          select_marker_node (selection, marker);
        break;
      }
    }

  if (selection->node_groups)
    rut_shell_set_selection (shell, selection);

  rut_shell_queue_redraw (shell);
}

typedef struct _MarkerGrabState
{
  RigControllerView *view;
  RigPathView *path_view;
  RigNodeMarker *marker;
  float grab_x;
  float current_dx;
  float to_pixel;
  float min_drag_offset;
  float max_drag_offset;

  RutCamera *camera;
  CoglMatrix transform;
  CoglMatrix inverse_transform;
} MarkerGrabState;

typedef void (*RigNodeSelectionCallback) (RigNode *node,
                                          NodeGroup *node_group,
                                          void *user_data);

static void
_rig_nodes_selection_foreach_node (RigNodesSelection *selection,
                                   RigNodeSelectionCallback callback,
                                   void *user_data)
{
  GList *l, *next;

  for (l = selection->node_groups; l; l = next)
    {
      NodeGroup *node_group = l->data;
      GList *l2, *next2;

      next = l->next;

      for (l2 = node_group->nodes; l2; l2 = next2)
        {
          next2 = l2->next;
          callback (l2->data, node_group, user_data);
        }
    }
}

static void
translate_node_marker_cb (RigNode *node,
                          NodeGroup *node_group,
                          void *user_data)
{
  float dx = *(float *)user_data;
  RigNodesSelection *selection = node_group->selection;
  NodeMapping *mapping = g_hash_table_lookup (selection->node_map, node);
  RigNodeMarker *marker = mapping->marker;
  RutTransform *transform = rut_graphable_get_parent (marker);

  rut_transform_translate (transform, dx, 0, 0);
}

static void
count_nodes_cb (RigNode *node,
                NodeGroup *node_group,
                void *user_data)
{
  int *n_nodes = user_data;
  (*n_nodes)++;
}

typedef struct _TmpNode
{
  RutBoxed boxed_value;
  float t;
  RigPathView *path_view;
} TmpNode;

typedef struct _MoveNodesState
{
  RigNodesSelection *selection;
  float length;
  TmpNode *tmp_nodes;
  int i;
} MoveNodesState;

static void
copy_nodes_cb (RigNode *node,
               NodeGroup *node_group,
               void *user_data)
{
  MoveNodesState *state = user_data;
  NodeMapping *mapping = g_hash_table_lookup (state->selection->node_map, node);
  RigNodeMarker *marker = mapping->marker;
  RigPathView *path_view = marker->path_view;

  rig_node_box (path_view->path->type,
                node, &state->tmp_nodes[state->i].boxed_value);
  state->tmp_nodes[state->i].t = node->t * state->length;
  state->tmp_nodes[state->i].path_view = path_view;
  state->i++;
}

static void
remove_nodes_cb (RigNode *node,
                 NodeGroup *node_group,
                 void *user_data)
{
  MoveNodesState *state = user_data;
  RigPathView *path_view = state->tmp_nodes[state->i].path_view;
  RigControllerPropertyView *prop_view = path_view->prop_view;
  RigControllerView *view = prop_view->object->view;
  RigEngine *engine = view->engine;
  RigControllerPropData *prop_data = prop_view->prop_data;

  /* NB: RigNodes store ->t normalized, but the journal api for setting
   * and removing path nodes works with unnormalized ->t values. */
  rig_undo_journal_remove_controller_path_node (engine->undo_journal,
                                                view->controller,
                                                prop_data->property,
                                                state->tmp_nodes[state->i].t);

  state->i++;
}

static void
apply_node_translations (RigControllerView *view,
                         MarkerGrabState *grab_state)
{
  RigEngine *engine = view->engine;
  RigUndoJournal *subjournal;
  int n_nodes;
  MoveNodesState state;
  float length = rig_controller_get_length (view->controller);
  float dx = grab_state->current_dx;
  float dt = (dx / grab_state->to_pixel) * length;
  int i;

  n_nodes = 0;
  _rig_nodes_selection_foreach_node (view->nodes_selection,
                                     count_nodes_cb,
                                     &n_nodes);

  state.selection = view->nodes_selection;
  state.tmp_nodes = g_alloca (sizeof (TmpNode) * n_nodes);
  state.length = length;

  /* Copy nodes */

  state.i = 0;
  _rig_nodes_selection_foreach_node (view->nodes_selection,
                                     copy_nodes_cb,
                                     &state);

  rig_engine_push_undo_subjournal (engine);

  /* Remove original nodes */

  state.i = 0;
  _rig_nodes_selection_foreach_node (view->nodes_selection,
                                     remove_nodes_cb,
                                     &state);

  /* XXX: actually this should be redundant... */
  /* Clear selection */

  _rig_nodes_selection_cancel (view->nodes_selection);

  /* Offset and add the new nodes */

  for (i = 0; i < n_nodes; i++)
    {
      TmpNode *tmp_node = &state.tmp_nodes[i];
      RigPathView *path_view = tmp_node->path_view;
      RigControllerPropData *prop_data = path_view->prop_view->prop_data;
      RutBoxed *boxed_value = &state.tmp_nodes[i].boxed_value;

      tmp_node->t += dt;

      rig_undo_journal_set_controller_path_node_value (engine->undo_journal,
                                                       false,
                                                       view->controller,
                                                       tmp_node->t,
                                                       boxed_value,
                                                       prop_data->property);
    }

  subjournal = rig_engine_pop_undo_subjournal (engine);
  rig_undo_journal_log_subjournal (engine->undo_journal, subjournal);

  /* NB: Adding nodes may have changed the length of the controller... */
  length = rig_controller_get_length (view->controller);

  /* Select the new nodes */

  for (i = 0; i < n_nodes; i++)
    {
      TmpNode *tmp_node = &state.tmp_nodes[i];
      RigPathView *path_view = tmp_node->path_view;
      float normalized_t = tmp_node->t / length;
      RigNode *node = rig_path_find_nearest (path_view->path, normalized_t);
      RigNodeMarker *marker = rig_path_view_find_node_marker (path_view, node);

      g_warn_if_fail (marker != NULL && marker->selected == false);

      _rig_controller_view_select_marker (view, marker, RUT_SELECT_ACTION_TOGGLE);
    }
}

static RutInputEventStatus
marker_grab_input_cb (RutInputEvent *event,
                      void *user_data)
{
  MarkerGrabState *state = user_data;
  RigNodeMarker *marker = state->marker;
  RigControllerView *view = marker->path_view->prop_view->object->view;

  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION)
    {
      RutContext *ctx = view->context;
      RutShell *shell = ctx->shell;
      RutCamera *camera = state->camera;
      float x = rut_motion_event_get_x (event);
      float y = rut_motion_event_get_y (event);

      rut_camera_unproject_coord (camera,
                                  &state->transform,
                                  &state->inverse_transform,
                                  0,
                                  &x,
                                  &y);

      if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_MOVE)
        {
          float dx = x - state->grab_x;

          if (state->current_dx)
            {
              float undo_dx = - state->current_dx;
              _rig_nodes_selection_foreach_node (view->nodes_selection,
                                                 translate_node_marker_cb,
                                                 &undo_dx);
            }

          if (dx > state->max_drag_offset)
            dx = state->max_drag_offset;
          else if (dx < state->min_drag_offset)
            dx = state->min_drag_offset;

          _rig_nodes_selection_foreach_node (view->nodes_selection,
                                             translate_node_marker_cb,
                                             &dx);
          state->current_dx = dx;

          rut_shell_queue_redraw (view->context->shell);
        }
      else if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_UP)
        {
          rut_shell_ungrab_input (shell, marker_grab_input_cb, user_data);

          if (state->current_dx)
            apply_node_translations (view, state);
          else if (!(rut_motion_event_get_modifier_state (event) &
                     RUT_MODIFIER_SHIFT_ON))
            {
              _rig_nodes_selection_cancel (view->nodes_selection);
              _rig_controller_view_select_marker (view, marker,
                                                  RUT_SELECT_ACTION_TOGGLE);
            }

          rut_scale_set_focus (view->scale,
                               marker->node->t *
                               rig_controller_get_length (view->controller));
          g_slice_free (MarkerGrabState, state);

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static RigNode *
find_unselected_neighbour (RigControllerView *view,
                           RutList *head,
                           RigNode *node,
                           CoglBool direction)
{
  while (TRUE)
    {
      RutList *next_link;
      RigNode *next_node;
      NodeMapping *mapping;

      if (direction)
        next_link = node->list_node.next;
      else
        next_link = node->list_node.prev;

      if (next_link == head)
        return NULL;

      next_node = rut_container_of (next_link, next_node, list_node);

      /* Ignore this node if it is also selected */
      mapping = g_hash_table_lookup (view->nodes_selection->node_map, next_node);
      if (mapping)
        goto node_is_selected;

      return next_node;

    node_is_selected:
      node = next_node;
    }
}

static void
calculate_drag_offset_range_cb (RigNode *node,
                                NodeGroup *node_group,
                                void *user_data)
{
  MarkerGrabState *state = user_data;
  RutList *node_list = &node_group->path->nodes;
  RigNode *next_node;
  float node_min, node_max;

  next_node = find_unselected_neighbour (state->view,
                                         node_list,
                                         node,
                                         false /* backwards */);

  if (next_node == NULL)
    node_min = 0.0f;
  else
    node_min = next_node->t + 0.0001;

  if (node_min > node->t)
    node_min = node->t;

  next_node = find_unselected_neighbour (state->view,
                                         node_list,
                                         node,
                                         true /* forwards */);

  if (next_node == NULL)
    node_max = G_MAXFLOAT;
  else
    node_max = next_node->t - 0.0001;

  if (node_max < node->t)
    node_max = node->t;

  if (node_min - node->t > state->min_drag_offset)
    state->min_drag_offset = node_min - node->t;
  if (node_max - node->t < state->max_drag_offset)
    state->max_drag_offset = node_max - node->t;
}

static void
calculate_drag_offset_range (RigControllerView *view,
                             MarkerGrabState *state)
{
  /* We want to limit the range that the user can drag the selected
   * nodes to so that it won't change the order of any of the nodes
   */

  state->min_drag_offset = -G_MAXFLOAT;
  state->max_drag_offset = G_MAXFLOAT;
  _rig_nodes_selection_foreach_node (view->nodes_selection,
                                     calculate_drag_offset_range_cb,
                                     state);

  state->min_drag_offset = (int)(state->min_drag_offset * state->to_pixel);

  if (state->max_drag_offset != G_MAXFLOAT)
    state->max_drag_offset = (int)(state->max_drag_offset * state->to_pixel);
}

static RutInputEventStatus
marker_input_cb (RutInputRegion *region,
                 RutInputEvent *event,
                 void *user_data)
{
  RigNodeMarker *marker = user_data;
  RutContext *ctx = marker->path_view->prop_view->object->view->context;

  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION &&
      rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_DOWN)
    {
      RutShell *shell = ctx->shell;
      MarkerGrabState *state = g_slice_new (MarkerGrabState);
      RigControllerView *view = marker->path_view->prop_view->object->view;
      float x = rut_motion_event_get_x (event);
      float y = rut_motion_event_get_y (event);

      state->camera = rut_input_event_get_camera (event);
      state->transform = *rut_camera_get_view_transform (state->camera);
      rut_graphable_apply_transform (marker->path_view, &state->transform);
      if (!cogl_matrix_get_inverse (&state->transform,
                                    &state->inverse_transform))
        {
          g_warning ("Failed to calculate inverse of path_view transform\n");
          g_slice_free (MarkerGrabState, state);
          return RUT_INPUT_EVENT_STATUS_UNHANDLED;
        }

      rut_camera_unproject_coord (state->camera,
                                  &state->transform,
                                  &state->inverse_transform,
                                  0,
                                  &x,
                                  &y);

      state->view = view;
      state->marker = marker;
      state->path_view = marker->path_view;
      state->grab_x = x;
      state->current_dx = 0;
      state->to_pixel = rut_scale_get_pixel_scale (view->scale) *
        rut_scale_get_length (view->scale);

      rut_scale_set_focus (view->scale,
                           marker->node->t *
                           rig_controller_get_length (view->controller));

      if (rut_motion_event_get_modifier_state (event) &
          RUT_MODIFIER_SHIFT_ON)
        _rig_controller_view_select_marker (view, marker,
                                            RUT_SELECT_ACTION_TOGGLE);
      else if (!marker->selected)
        {
          _rig_controller_view_select_marker (view, marker,
                                              RUT_SELECT_ACTION_REPLACE);
        }

      calculate_drag_offset_range (view, state);

      rut_shell_grab_input (shell,
                            rut_input_event_get_camera (event),
                            marker_grab_input_cb,
                            state);

      return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static RigNodeMarker *
_rig_node_marker_new (RigPathView *path_view,
                      RigPath *path,
                      RigNode *node)
{
  RigNodeMarker *marker =
    rut_object_alloc0 (RigNodeMarker, &rig_node_marker_type, _rig_node_marker_init_type);
  RutContext *ctx = path_view->prop_view->object->view->context;
  CoglTexture *tex;



  rut_graphable_init (marker);

  marker->path_view = path_view;
  marker->path = path;
  marker->node = node;

  tex = rut_load_texture_from_data_file (ctx, "dot.png", NULL);

  marker->rect = rut_nine_slice_new (ctx, tex,
                                     0, 0, 0, 0, 10, 10);
  rut_graphable_add_child (marker, marker->rect);
  rut_object_unref (marker->rect);

  marker->input_region = rut_input_region_new_rectangle (0, 0, 10, 10,
                                                         marker_input_cb,
                                                         marker);
  rut_graphable_add_child (marker, marker->input_region);
  rut_object_unref (marker->input_region);

  return marker;
}

static RutObject *
_rig_nodes_selection_copy (RutObject *object)
{
  RigNodesSelection *selection = object;
  RigNodesSelection *copy = _rig_nodes_selection_new (selection->view);
  GList *l;

  for (l = selection->node_groups; l; l = l->next)
    {
      NodeGroup *node_group = l->data;
      NodeGroup *new_node_group = g_slice_new0 (NodeGroup);
      GList *l2;

      new_node_group->prop_spec = node_group->prop_spec;

      new_node_group->path = NULL;

      for (l2 = node_group->nodes; l2; l2 = l2->next)
        {
          RigNode *new_node = rig_node_copy (l2->data);
          new_node_group->nodes =
            g_list_prepend (new_node_group->nodes, new_node);
        }
    }

  return copy;
}

static void
_rig_nodes_selection_delete (RutObject *object)
{
  RigNodesSelection *selection = object;
  RigControllerView *view = selection->view;

  if (!selection->node_groups)
    return;

  /* XXX: It's assumed that a selection either corresponds to
   * view->nodes_selection or to a derived selection due to the
   * selectable::copy vfunc.
   *
   * A copy should contain deep-copied entities that don't need to be
   * directly deleted with rig_undo_journal_delete_path_node() because
   * they won't be part of the UI.
   */

  if (selection == view->nodes_selection)
    {
      GList *l, *next;
      int len = g_list_length (selection->node_groups);
      RigController *controller = view->controller;
      RigEngine *engine = view->engine;
      RigUndoJournal *subjournal;
      float length = rig_controller_get_length (controller);

      rig_engine_push_undo_subjournal (engine);

      for (l = selection->node_groups; l; l = next)
        {
          NodeGroup *node_group = l->data;
          int n_nodes = g_list_length (node_group->nodes);
          GList *l2, *next2;

          next = l->next;

          for (l2 = node_group->nodes; l2; l2 = next2)
            {
              RigNode *node = l2->data;
              NodeMapping *mapping = g_hash_table_lookup (selection->node_map, node);
              RutProperty *property =
                mapping->marker->path_view->prop_view->prop_data->property;

              next2 = l2->next;

              rig_undo_journal_remove_controller_path_node (engine->undo_journal,
                                                            controller,
                                                            property,
                                                            node->t * length);
            }

          /* XXX: make sure that
           * rig_undo_journal_delete_path_node () doesn't change
           * the selection. */
          g_warn_if_fail (n_nodes == g_list_length (node_group->nodes));
        }

      subjournal = rig_engine_pop_undo_subjournal (engine);
      rig_undo_journal_log_subjournal (engine->undo_journal, subjournal);

      /* NB: that rig_undo_journal_delete_component () will
       * remove the entity from the scenegraph*/

      /* XXX: make sure that
       * rig_undo_journal_delete_path_node () doesn't change
       * the selection. */
      g_warn_if_fail (len == g_list_length (selection->node_groups));
    }

  g_list_free_full (selection->node_groups,
                    (GDestroyNotify)destroy_node_group);
  selection->node_groups = NULL;
}


static void
_rig_nodes_selection_free (void *object)
{
  RigNodesSelection *selection = object;

  _rig_nodes_selection_cancel (selection);

  g_hash_table_destroy (selection->node_map);
  rut_object_free (RigNodesSelection, selection);
}

RutType rig_nodes_selection_type;

static void
_rig_nodes_selection_init_type (void)
{
  static RutSelectableVTable selectable_vtable = {
      .cancel = _rig_nodes_selection_cancel,
      .copy = _rig_nodes_selection_copy,
      .del = _rig_nodes_selection_delete,
  };
  static RutMimableVTable mimable_vtable = {
      .copy = _rig_nodes_selection_copy
  };

  RutType *type = &rig_nodes_selection_type;
#define TYPE RigNodesSelection

  rut_type_init (type, G_STRINGIFY (TYPE), _rig_nodes_selection_free);
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

static void
destroy_node_mapping (NodeMapping *mapping)
{
  rut_object_unref (mapping->marker);
  g_slice_free (NodeMapping, mapping);
}

static RigNodesSelection *
_rig_nodes_selection_new (RigControllerView *view)
{
  RigNodesSelection *selection =
    rut_object_alloc0 (RigNodesSelection,
                       &rig_nodes_selection_type,
                       _rig_nodes_selection_init_type);

  selection->view = view;
  selection->node_groups = NULL;

  selection->node_map =
    g_hash_table_new_full (g_direct_hash, g_direct_equal,
                           NULL, /* key destroy func */
                           (GDestroyNotify) destroy_node_mapping);

  return selection;
}

typedef CoglVertexP2C4 RigControllerViewDotVertex;

static void
rig_controller_view_property_removed (RigControllerView *view,
                                      RutProperty *property);

#if 0
static RutInputEventStatus
rig_controller_view_grab_input_cb (RutInputEvent *event,
                                   void *user_data);
#endif

static void
_rig_path_view_free (void *object)
{
  RigPathView *path_view = object;
  RigControllerView *view = path_view->prop_view->object->view;

  rut_property_closure_destroy (path_view->scale_offset_prop_closure);
  rut_property_closure_destroy (path_view->scale_prop_closure);
  rut_property_closure_destroy (path_view->scale_len_prop_closure);

  rut_closure_list_disconnect_all (&path_view->preferred_size_cb_list);

  rut_closure_disconnect (path_view->path_operation_closure);
  rut_object_unref (path_view->path);

  rut_graphable_destroy (path_view);

  rut_shell_remove_pre_paint_callback_by_graphable (view->context->shell, path_view);

  rut_object_free (RigPathView, path_view);
}

static void
_rig_path_view_allocate_cb (RutObject *object,
                            void *user_data)
{
  RigPathView *path_view = object;
  RigControllerView *view = path_view->prop_view->object->view;
  RutGraphableProps *markers_graphable =
    rut_object_get_properties (path_view->markers,
                               RUT_TRAIT_ID_GRAPHABLE);
  float length = rig_controller_get_length (view->controller);
  float to_pixel = rut_scale_get_pixel_scale (view->scale);
  float origin = rut_scale_get_offset (view->scale);
  float origin_px = origin * to_pixel;
  GList *l;

  rut_sizable_set_size (path_view->ui_viewport,
                        path_view->width, path_view->height);

  rut_sizable_set_size (path_view->input_region,
                        path_view->width, path_view->height);

  for (l = markers_graphable->children.head; l; l = l->next)
    {
      RutTransform *transform = l->data;
      RutGraphableProps *transform_graphable =
        rut_object_get_properties (transform, RUT_TRAIT_ID_GRAPHABLE);
      RigNodeMarker *marker = transform_graphable->children.head->data;
      RigNode *node = marker->node;

      float t_px = node->t * length * to_pixel - origin_px;

      rut_transform_init_identity (transform);
      rut_transform_translate (transform, t_px, 0, 0);
    }

  rut_shell_queue_redraw (view->context->shell);
}

static void
_rig_path_view_queue_allocate (RigPathView *path_view)
{
  RigControllerView *view = path_view->prop_view->object->view;

  rut_shell_add_pre_paint_callback (view->context->shell,
                                    path_view,
                                    _rig_path_view_allocate_cb,
                                    NULL /* user_data */);
}

static void
rig_path_view_set_size (void *sizable,
                        float width,
                        float height)
{
  RigPathView *path_view = sizable;

  if (width == path_view->width &&
      height == path_view->height)
    return;

  path_view->width = width;
  path_view->height = height;

  _rig_path_view_queue_allocate (path_view);
}

static void
rig_path_view_get_size (void *sizable,
                        float *width,
                        float *height)
{
  RigPathView *path_view = sizable;

  *width = path_view->width;
  *height = path_view->height;
}

static RutClosure *
_rig_path_view_add_preferred_size_callback
                                   (void *sizable,
                                    RutSizablePreferredSizeCallback cb,
                                    void *user_data,
                                    RutClosureDestroyCallback destroy_cb)
{
  RigPathView *path_view = sizable;

  return rut_closure_list_add (&path_view->preferred_size_cb_list,
                               cb,
                               user_data,
                               destroy_cb);
}

static void
_rig_path_view_preferred_size_changed (RigPathView *path_view)
{
  rut_closure_list_invoke (&path_view->preferred_size_cb_list,
                           RutSizablePreferredSizeCallback,
                           path_view);
  _rig_path_view_queue_allocate (path_view);
}

static void
draw_timeline_background (RigPathView *path_view,
                          CoglFramebuffer *fb)
{
  RigControllerView *view = path_view->prop_view->object->view;
  float width;
  int tex_width = 200;

#if 0
  if (tex_width < 1)
    return;

  if ((view->nodes_grid_width != tex_width ||
       view->nodes_grid_height != tex_height) &&
      view->path_bg_pipeline)
    {
      cogl_object_unref (view->path_bg_pipeline);
      view->path_bg_pipeline = NULL;
    }
#endif

  if (view->path_bg_pipeline == NULL)
    {
      int tex_height = 4;
      int half_width = tex_width / 2;
      int quater_width = half_width / 2;
      CoglPipeline *pipeline;
      int rowstride;
      int y;
      CoglBitmap *bitmap;
      CoglPixelBuffer *buffer;
      CoglTexture *texture;
      uint8_t *tex_data;

      pipeline = cogl_pipeline_new (view->context->cogl_context);

      bitmap = cogl_bitmap_new_with_size (view->context->cogl_context,
                                          tex_width, tex_height,
                                          COGL_PIXEL_FORMAT_RGB_888);
      buffer = cogl_bitmap_get_buffer (bitmap);

      rowstride = cogl_bitmap_get_rowstride (bitmap);
      tex_data = cogl_buffer_map (buffer,
                                  COGL_BUFFER_ACCESS_WRITE,
                                  COGL_BUFFER_MAP_HINT_DISCARD,
                                  NULL);

      memset (tex_data, 0xff, rowstride * tex_height);
      for (y = 0; y < tex_height; y++)
        {
          uint8_t *p = tex_data + y * rowstride;

          memset (p, 0x63, 3 * half_width);
          memset (p + half_width * 3, 0x47, 3 * (tex_width - half_width));

          memset (p + quater_width * 3, 0x74, 3);
          memset (p + (half_width + quater_width) * 3, 0x74, 3);
        }
      //memset (tex_data + rowstride * (tex_height - 1), 0x74, tex_width * 3);

      cogl_buffer_unmap (buffer);

      texture = cogl_texture_2d_new_from_bitmap (bitmap);

      cogl_pipeline_set_layer_texture (pipeline,
                                       0, /* layer_num */
                                       texture);
      cogl_pipeline_set_layer_filters
        (pipeline,
         0, /* layer_num */
         COGL_PIPELINE_FILTER_LINEAR_MIPMAP_NEAREST,
         COGL_PIPELINE_FILTER_LINEAR);
      cogl_pipeline_set_layer_wrap_mode
        (pipeline,
         0, /* layer_num */
         COGL_PIPELINE_WRAP_MODE_REPEAT);

      cogl_object_unref (bitmap);
      cogl_object_unref (texture);

      view->nodes_grid_width = tex_width;
      view->nodes_grid_height = tex_height;

      view->path_bg_pipeline = pipeline;
    }

  width = path_view->width;

  cogl_framebuffer_draw_textured_rectangle (fb,
                                            view->path_bg_pipeline,
                                            0,
                                            0,
                                            path_view->width,
                                            path_view->height,
                                            0, 0, /* s1, t1 */
                                            width /
                                            (float) tex_width,
                                            1);
}

static void
_rig_path_view_paint (RutObject *object,
                      RutPaintContext *paint_ctx)
{
  RigPathView *path_view = object;
  CoglFramebuffer *fb = rut_camera_get_framebuffer (paint_ctx->camera);

  draw_timeline_background (path_view, fb);
}


RutType rig_path_view_type;

static void
_rig_path_view_init_type (void)
{
  static RutGraphableVTable graphable_vtable = { 0 };

  static RutSizableVTable sizable_vtable = {
      rig_path_view_set_size,
      rig_path_view_get_size,
      rut_simple_sizable_get_preferred_width,
      rut_simple_sizable_get_preferred_height,
      _rig_path_view_add_preferred_size_callback
  };

  static RutPaintableVTable paintable_vtable = {
      _rig_path_view_paint
  };

  RutType *type = &rig_path_view_type;
#define TYPE RigPathView

  rut_type_init (type, G_STRINGIFY (TYPE), _rig_path_view_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_GRAPHABLE,
                      offsetof (TYPE, graphable),
                      &graphable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_PAINTABLE,
                      offsetof (TYPE, paintable),
                      &paintable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_SIZABLE,
                      0, /* no implied properties */
                      &sizable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_COMPOSITE_SIZABLE,
                      offsetof (TYPE, ui_viewport),
                      NULL); /* no vtable */

#undef TYPE
}

static RigNodeMarker *
rig_path_view_add_node (RigPathView *path_view,
                        RigNode *node)
{
  RutContext *ctx = path_view->prop_view->object->view->context;
  RutTransform *transform = rut_transform_new (ctx);
  RigNodeMarker *marker;

  rut_graphable_add_child (path_view->markers, transform);
  rut_object_unref (transform);

  marker = _rig_node_marker_new (path_view,
                                 path_view->path,
                                 node);
  rut_graphable_add_child (transform, marker);
  rut_object_unref (marker);

  _rig_path_view_queue_allocate (path_view);

  return marker;
}

static void
add_path_marker_cb (RigNode *node,
                    void *user_data)
{
  RigPathView *path_view = user_data;
  rig_path_view_add_node (path_view, node);
}

static RigNodeMarker *
rig_path_view_find_node_marker (RigPathView *path_view,
                                RigNode *node)
{
  RutGraphableProps *graphable =
    rut_object_get_properties (path_view->markers, RUT_TRAIT_ID_GRAPHABLE);
  GList *l;

  for (l = graphable->children.head; l; l = l->next)
    {
      RutTransform *transform = l->data;
      RutGraphableProps *transform_graphable =
        rut_object_get_properties (transform, RUT_TRAIT_ID_GRAPHABLE);
      RigNodeMarker *marker = transform_graphable->children.head->data;

      if (marker->node == node)
        return marker;
    }

  return NULL;
}

static void
path_operation_cb (RigPath *path,
                   RigPathOperation op,
                   RigNode *node,
                   void *user_data)
{
  RigPathView *path_view = user_data;
  RigControllerObjectView *object_view = path_view->prop_view->object;
  RigControllerView *view = object_view->view;

  switch (op)
    {
    case RIG_PATH_OPERATION_MODIFIED:

      rut_shell_queue_redraw (view->context->shell);
      break;

    case RIG_PATH_OPERATION_ADDED:
      add_path_marker_cb (node, path_view);

      rut_shell_queue_redraw (view->context->shell);
      break;

    case RIG_PATH_OPERATION_REMOVED:
      {
        RigNodeMarker *marker;
        RutTransform *transform;

        unselect_node (view->nodes_selection, node);

        marker = rig_path_view_find_node_marker (path_view, node);
        transform = rut_graphable_get_parent (marker);
        rut_graphable_remove_child (transform);
      }

      rut_shell_queue_redraw (view->context->shell);
      break;
    }
}

/* Called if the ::offset or ::scale change for view->scale... */
static void
scale_changed_cb (RutProperty *property, void *user_data)
{
  _rig_path_view_preferred_size_changed (user_data);
}

typedef struct _PathViewGrabState
{
  RigControllerView *view;
  RigPathView *path_view;

  RutCamera *camera;
  CoglMatrix transform;
  CoglMatrix inverse_transform;
} PathViewGrabState;

static RutInputEventStatus
path_view_grab_input_cb (RutInputEvent *event,
                         void *user_data)
{
  PathViewGrabState *state = user_data;
  RigControllerView *view = state->view;

  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION)
    {
      RutContext *ctx = view->context;
      RutShell *shell = ctx->shell;
      RutCamera *camera = state->camera;
      float x = rut_motion_event_get_x (event);
      float y = rut_motion_event_get_y (event);
      float focus_offset;

      rut_camera_unproject_coord (camera,
                                  &state->transform,
                                  &state->inverse_transform,
                                  0,
                                  &x,
                                  &y);

      focus_offset = rut_scale_pixel_to_offset (view->scale, x);
      rut_scale_set_focus (view->scale, focus_offset);

      if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_UP)
        {
          rut_shell_ungrab_input (shell, path_view_grab_input_cb, user_data);
          g_slice_free (PathViewGrabState, state);
        }

      return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static RutInputEventStatus
path_view_input_region_cb (RutInputRegion *region,
                           RutInputEvent *event,
                           void *user_data)
{
  RigPathView *path_view = user_data;
  RutContext *ctx = path_view->prop_view->object->view->context;

  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION &&
      rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_DOWN)
    {
      RutShell *shell = ctx->shell;
      PathViewGrabState *state = g_slice_new (PathViewGrabState);
      RigControllerView *view = path_view->prop_view->object->view;
      float x = rut_motion_event_get_x (event);
      float y = rut_motion_event_get_y (event);
      float focus_offset;

      state->view = view;
      state->path_view = path_view;

      state->camera = rut_input_event_get_camera (event);
      state->transform = *rut_camera_get_view_transform (state->camera);
      rut_graphable_apply_transform (path_view, &state->transform);
      if (!cogl_matrix_get_inverse (&state->transform,
                                    &state->inverse_transform))
        {
          g_warning ("Failed to calculate inverse of path_view transform\n");
          g_slice_free (PathViewGrabState, state);
          return RUT_INPUT_EVENT_STATUS_UNHANDLED;
        }

      rut_camera_unproject_coord (state->camera,
                                  &state->transform,
                                  &state->inverse_transform,
                                  0,
                                  &x,
                                  &y);

      focus_offset = rut_scale_pixel_to_offset (view->scale, x);
      rut_scale_set_focus (view->scale, focus_offset);

      rut_shell_grab_input (shell,
                            rut_input_event_get_camera (event),
                            path_view_grab_input_cb,
                            state);

      return RUT_INPUT_EVENT_STATUS_HANDLED;
    }
  else if(rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_KEY &&
          rut_key_event_get_action (event) == RUT_KEY_EVENT_ACTION_DOWN)
    {
      RigControllerView *view = path_view->prop_view->object->view;

      switch (rut_key_event_get_keysym (event))
        {
        case RUT_KEY_equal:
          rut_scale_user_zoom_in (view->scale);
          return RUT_INPUT_EVENT_STATUS_HANDLED;
        case RUT_KEY_minus:
          rut_scale_user_zoom_out (view->scale);
          return RUT_INPUT_EVENT_STATUS_HANDLED;
        case RUT_KEY_0:
          rut_scale_user_zoom_reset (view->scale);
          return RUT_INPUT_EVENT_STATUS_HANDLED;
        default:
          break;
        }
    }


  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static RigPathView *
rig_path_view_new (RigControllerPropertyView *prop_view)
{
  RigPathView *path_view = rut_object_alloc0 (RigPathView,
                                              &rig_path_view_type,
                                              _rig_path_view_init_type);
  RigControllerView *view= prop_view->object->view;
  RutProperty *property = prop_view->prop_data->property;
  RutProperty *offset_prop;
  RutProperty *scale_prop;
  RutProperty *len_prop;
  RigPath *path;


  rut_graphable_init (path_view);
  rut_paintable_init (path_view);

  path_view->prop_view = prop_view;

  rut_list_init (&path_view->preferred_size_cb_list);

  path_view->ui_viewport = rut_ui_viewport_new (view->context, 1, 1);
  rut_graphable_add_child (path_view, path_view->ui_viewport);
  rut_object_unref (path_view->ui_viewport);

  path_view->input_region =
    rut_input_region_new_rectangle (0.0, 0.0, /* x0/y0 */
                                    0.0, 0.0, /* x1/y1 */
                                    path_view_input_region_cb,
                                    path_view);
  rut_graphable_add_child (path_view->ui_viewport, path_view->input_region);
  rut_object_unref (path_view->input_region);

  path_view->markers = rut_transform_new (view->context);
  rut_graphable_add_child (path_view->ui_viewport, path_view->markers);
  rut_object_unref (path_view->markers);

  path = rig_controller_get_path_for_property (view->controller,
                                               property);
  path_view->path = rut_object_ref (path);

  rut_path_foreach_node (path,
                         add_path_marker_cb,
                         path_view);

  path_view->path_operation_closure =
    rig_path_add_operation_callback (path,
                                     path_operation_cb,
                                     path_view,
                                     NULL /* destroy_cb */);

  offset_prop = rut_introspectable_lookup_property (view->scale, "offset");
  path_view->scale_offset_prop_closure =
    rut_property_connect_callback (offset_prop,
                                   scale_changed_cb,
                                   path_view);

  scale_prop = rut_introspectable_lookup_property (view->scale, "user_scale");
  path_view->scale_prop_closure =
    rut_property_connect_callback (scale_prop,
                                   scale_changed_cb,
                                   path_view);

  len_prop = rut_introspectable_lookup_property (view->scale, "length");
  path_view->scale_len_prop_closure =
    rut_property_connect_callback (len_prop,
                                   scale_changed_cb,
                                   path_view);

  return path_view;
}

static void
_rig_controller_property_view_free (void *object)
{
  RigControllerPropertyView *prop_view = object;
  int i;

  rut_closure_list_disconnect_all (&prop_view->preferred_size_cb_list);

  for (i = 0; i < RIG_CONTROLLER_VIEW_N_PROPERTY_COLUMNS; i++)
    {
      RigControllerViewColumn *column = &prop_view->columns[i];
      rut_closure_disconnect (column->control_preferred_size_closure);
    }

  rut_graphable_destroy (prop_view);

  rut_shell_remove_pre_paint_callback_by_graphable (prop_view->object->view->context->shell,
                                       prop_view);

  rut_object_free (RigControllerPropertyView, prop_view);
}

float
calculate_column_width (RigControllerView *view, int column_index)
{
  float column_width = 0;
  GList *l, *l2;

  for (l = view->object_views; l; l = l->next)
    {
      RigControllerObjectView *object_view = l->data;

      for (l2 = object_view->properties; l2; l2 = l2->next)
        {
          RigControllerPropertyView *prop_view = l2->data;
          RigControllerViewColumn *column = &prop_view->columns[column_index];
          float min_width, natural_width;

          rut_sizable_get_preferred_width (column->control,
                                           -1, /* for_height */
                                           &min_width,
                                           &natural_width);
          if (natural_width > column_width)
            column_width = natural_width;
        }
    }

  return column_width;
}

static void
update_column_widths (RigControllerView *view)
{
  int i;

  for (i = 0; i < RIG_CONTROLLER_VIEW_N_PROPERTY_COLUMNS; i++)
    view->column_widths[i] = calculate_column_width (view, i);

  rut_shim_set_width (view->properties_label_shim,
                      view->column_widths[0] + view->column_widths[1]);
}

static float
calculate_row_height (RigControllerPropertyView *prop_view)
{
  RigControllerView *view = prop_view->object->view;
  float max_height = 0;
  int i;

  for (i = 0; i < RIG_CONTROLLER_VIEW_N_PROPERTY_COLUMNS; i++)
    {
      RigControllerViewColumn *column = &prop_view->columns[i];
      float column_width = view->column_widths[i];
      float min_height, natural_height;

      rut_sizable_get_preferred_height (column->control,
                                        column_width, /* for width */
                                        &min_height,
                                        &natural_height);

      if (natural_height > max_height)
        max_height = natural_height;
    }

  return max_height;
}

static void
_rig_controller_property_view_allocate_cb (RutObject *graphable,
                                           void *user_data)
{
  RigControllerPropertyView *prop_view = graphable;
  RigControllerView *view = prop_view->object->view;
  RigControllerViewColumn *column;
  float row_height;
  float column_width;
  float dx;
  int i;

  update_column_widths (view);

  /* Give the last column the remaining width */

  dx = 0;
  for (i = 0; i < (RIG_CONTROLLER_VIEW_N_PROPERTY_COLUMNS - 1); i++)
    dx += view->column_widths[i];

  column_width = prop_view->width - dx;
  view->column_widths[RIG_CONTROLLER_VIEW_N_PROPERTY_COLUMNS - 1] =
    MAX (1, column_width);

  /* NB: must be done after we know the column widths */
  row_height = calculate_row_height (prop_view);

  dx = 0;
  for (i = 0; i < RIG_CONTROLLER_VIEW_N_PROPERTY_COLUMNS; i++)
    {
      column = &prop_view->columns[i];
      column_width = view->column_widths[i];

      rut_transform_init_identity (column->transform);
      rut_transform_translate (column->transform, dx, 0, 0);

      rut_sizable_set_size (column->control,
                            column_width,
                            row_height);

      dx += column_width;
    }

  rut_sizable_set_size (prop_view->stack, prop_view->width, prop_view->height);

  rut_shell_queue_redraw (view->context->shell);
}

static void
_rig_controller_property_view_queue_allocate (
                                        RigControllerPropertyView *prop_view)
{
  RigControllerView *view = prop_view->object->view;

  rut_shell_add_pre_paint_callback (view->context->shell,
                                    prop_view,
                                    _rig_controller_property_view_allocate_cb,
                                    NULL /* user_data */);
}

static void
rig_controller_property_view_set_size (void *sizable,
                                       float width,
                                       float height)
{
  RigControllerPropertyView *prop_view = sizable;

  if (width == prop_view->width &&
      height == prop_view->height)
    return;

  prop_view->width = width;
  prop_view->height = height;

  _rig_controller_property_view_queue_allocate (prop_view);
}

static void
rig_controller_property_view_get_size (void *sizable,
                                       float *width,
                                       float *height)
{
  RigControllerPropertyView *prop_view = sizable;

  *width = prop_view->width;
  *height = prop_view->height;
}

static void
rig_controller_property_view_get_preferred_width (void *sizable,
                                                  float for_height,
                                                  float *min_width_p,
                                                  float *natural_width_p)
{
  RigControllerPropertyView *prop_view = sizable;
  RigControllerView *view= prop_view->object->view;
  float total_width = 0;
  int i;

  update_column_widths (view);

  for (i = 0; i < RIG_CONTROLLER_VIEW_N_PROPERTY_COLUMNS; i++)
    total_width += view->column_widths[i];

  *natural_width_p = total_width;
  *min_width_p = total_width;
}

static void
rig_controller_property_view_get_preferred_height (void *sizable,
                                                   float for_width,
                                                   float *min_height_p,
                                                   float *natural_height_p)
{
  RigControllerPropertyView *prop_view = sizable;
  RigControllerView *view= prop_view->object->view;

  update_column_widths (view);

  *natural_height_p = calculate_row_height (prop_view);
  *min_height_p = *natural_height_p;
}

static RutClosure *
rig_controller_property_view_add_preferred_size_callback
                                   (void *sizable,
                                    RutSizablePreferredSizeCallback cb,
                                    void *user_data,
                                    RutClosureDestroyCallback destroy_cb)
{
  RigControllerPropertyView *prop_view = sizable;

  return rut_closure_list_add (&prop_view->preferred_size_cb_list,
                               cb,
                               user_data,
                               destroy_cb);
}

static void
_rig_controller_property_view_preferred_size_changed
                                    (RigControllerPropertyView *prop_view)
{
  rut_closure_list_invoke (&prop_view->preferred_size_cb_list,
                           RutSizablePreferredSizeCallback,
                           prop_view);
}

RutType rig_controller_property_view_type;

static void
_rig_controller_property_view_init_type (void)
{

  static RutGraphableVTable graphable_vtable = {
      NULL, /* child removed */
      NULL, /* child addded */
      NULL /* parent changed */
  };

  static RutSizableVTable sizable_vtable = {
      rig_controller_property_view_set_size,
      rig_controller_property_view_get_size,
      rig_controller_property_view_get_preferred_width,
      rig_controller_property_view_get_preferred_height,
      rig_controller_property_view_add_preferred_size_callback
  };

  RutType *type = &rig_controller_property_view_type;
#define TYPE RigControllerPropertyView

  rut_type_init (type, G_STRINGIFY (TYPE), _rig_controller_property_view_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_GRAPHABLE,
                      offsetof (TYPE, graphable),
                      &graphable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_SIZABLE,
                      0, /* no implied properties */
                      &sizable_vtable);

#undef TYPE
}

static void
control_preferred_size_cb (RutObject *sizable, void *user_data)
{
  RigControllerPropertyView *prop_view = user_data;

  _rig_controller_property_view_preferred_size_changed (prop_view);

  _rig_controller_property_view_queue_allocate (prop_view);
}

static void
setup_label_column (RigControllerPropertyView *prop_view,
                    const char *text)
{
  RigControllerViewColumn *column = &prop_view->columns[0];
  RigControllerView *view = prop_view->object->view;
  RutContext *ctx = view->context;
  RutBin *bin = rut_bin_new (ctx);
  RutText *label = rut_text_new (ctx);

  rut_bin_set_left_padding (bin, 20);
  rut_bin_set_child (bin, label);
  rut_object_unref (label);

  if (text)
    rut_text_set_text (label, text);

  rut_text_set_color_u32 (label, 0xffffffff);

  column->transform = rut_transform_new (ctx);
  rut_graphable_add_child (prop_view->columns_parent, column->transform);
  rut_object_unref (column->transform);

  column->control = bin;

  column->control_preferred_size_closure =
    rut_sizable_add_preferred_size_callback (bin,
                                             control_preferred_size_cb,
                                             prop_view,
                                             NULL /* destroy */);

  rut_graphable_add_child (column->transform, bin);
  rut_object_unref (bin);
}

static void
const_property_changed_cb (RutProperty *primary_target_prop,
                           RutProperty *source_prop,
                           void *user_data)
{

}

static void
update_method_control (RigControllerPropertyView *prop_view)
{
  RigControllerView *view = prop_view->object->view;
  RutContext *ctx = view->context;
  RigControllerViewColumn *column = &prop_view->columns[2];

  if (!column->transform)
    {
      column->transform = rut_transform_new (ctx);
      rut_graphable_add_child (prop_view->columns_parent, column->transform);
      rut_object_unref (column->transform);
    }

  if (column->control)
    {
      rut_graphable_remove_child (column->control);
      column->control = NULL;
    }

  switch (prop_view->prop_data->method)
    {
    case RIG_CONTROLLER_METHOD_CONSTANT:
      column->control = rut_prop_inspector_new (ctx,
                                                prop_view->prop_data->property,
                                                const_property_changed_cb,
                                                NULL, /* controlled changed */
                                                false, /* without a label */
                                                view);
      break;
    case RIG_CONTROLLER_METHOD_PATH:
      column->control = rig_path_view_new (prop_view);
      break;
    case RIG_CONTROLLER_METHOD_BINDING:
      column->control = rig_binding_view_new (ctx);
      break;
    }

  column->control_preferred_size_closure =
    rut_sizable_add_preferred_size_callback (column->control,
                                             control_preferred_size_cb,
                                             prop_view,
                                             NULL /* destroy */);

  rut_graphable_add_child (column->transform, column->control);
  rut_object_unref (column->control);

  _rig_controller_property_view_queue_allocate (prop_view);
}

static void
method_drop_down_change_cb (RutProperty *value, void *user_data)
{
  RigControllerPropertyView *prop_view = user_data;
  RigControllerObjectView *object_view = prop_view->object;
  RigControllerView *view = object_view->view;
  RutProperty *property = prop_view->prop_data->property;
  RigControllerMethod method = rut_property_get_integer (value);
  RigEngine *engine = view->engine;
  RigUndoJournal *subjournal;
  RigPath *path;

  /* If it's not a user action then we can assume that the controller
   * method has already been changed and we only need to update out
   * visual representation of the method...
   */
  if (prop_view->internal_method_change)
    {
      update_method_control (prop_view);
      return;
    }

  subjournal = rig_undo_journal_new (engine);

  /* We want the change in control method to be applied immediately
   * here otherwise in the case where we try and add an initial key
   * frame below then rig_controller_view_edit_property() won't see
   * that the property currently has an associated path.
   */
  rig_undo_journal_set_apply_on_insert (subjournal, true);

  rig_undo_journal_set_control_method (subjournal,
                                       view->controller,
                                       property,
                                       method);

  /* If the property is being initially marked as animated and the
   * path is empty then for convenience we want to create a node for
   * the current time. We want this to be undone as a single action so
   * we'll represent the pair of actions in a subjournal */
  if (method == RIG_CONTROLLER_METHOD_PATH &&
      (path = rig_controller_get_path_for_property (view->controller,
                                                    property)) &&
      path->length == 0)
    {
      RutBoxed property_value;

      rut_property_box (property, &property_value);

      rig_controller_view_edit_property (view, FALSE, /* mergable */
                                         property, &property_value);

      rut_boxed_destroy (&property_value);
    }

  rig_undo_journal_log_subjournal (engine->undo_journal, subjournal);

  update_method_control (prop_view);
}

static void
setup_method_drop_down (RigControllerPropertyView *prop_view)
{
  RigControllerView *view = prop_view->object->view;
  RutContext *ctx = view->context;
  RigControllerViewColumn *column = &prop_view->columns[1];
  RutDropDownValue values[] = {
    { "Const", RIG_CONTROLLER_METHOD_CONSTANT },
    { "Path", RIG_CONTROLLER_METHOD_PATH },
    { "Bind", RIG_CONTROLLER_METHOD_BINDING }
  };
  RutBin *bin = rut_bin_new (ctx);
  RutDropDown *drop_down = rut_drop_down_new (ctx);
  RutProperty *drop_property;

  prop_view->method_drop_down = drop_down;

  rut_drop_down_set_values_array (drop_down,
                                  values,
                                  3);

  rut_bin_set_child (bin, drop_down);
  rut_object_unref (drop_down);
  rut_bin_set_left_padding (bin, 5);
  rut_bin_set_right_padding (bin, 5);

  column->transform = rut_transform_new (ctx);
  rut_graphable_add_child (prop_view->columns_parent, column->transform);
  rut_object_unref (column->transform);

  column->control = bin;
  column->control_preferred_size_closure =
    rut_sizable_add_preferred_size_callback (bin,
                                             control_preferred_size_cb,
                                             prop_view,
                                             NULL /* destroy */);

  rut_graphable_add_child (column->transform, bin);
  rut_object_unref (bin);

  rut_drop_down_set_value (drop_down, prop_view->prop_data->method);

  drop_property = rut_introspectable_lookup_property (drop_down, "value");
  rut_property_connect_callback (drop_property,
                                 method_drop_down_change_cb,
                                 prop_view);
}

#if 0
static void
rig_controller_view_unselect_node (RigControllerView *view,
                                   RigControllerPropertyView *prop_view,
                                   RigNode *node)
{
  if (prop_view->has_selected_nodes)
    {
      RigControllerViewSelectedNode *selected_node, *tmp;
      CoglBool has_nodes = FALSE;

      rut_list_for_each_safe (selected_node,
                              tmp,
                              &view->selected_nodes,
                              list_node)
        {
          if (selected_node->prop_view == prop_view)
            {
              if (selected_node->node == node)
                {
                  rut_list_remove (&selected_node->list_node);
                  g_slice_free (RigControllerViewSelectedNode, selected_node);
                  view->dots_dirty = TRUE;
                  /* we don't want to break here because we want to
                   * continue searching so that we can update the
                   * has_nodes value */
                }
              else
                has_nodes = TRUE;
            }
        }

      prop_view->has_selected_nodes = has_nodes;
    }
}
#endif

static RigControllerPropertyView *
rig_controller_property_view_new (RigControllerView *view,
                                  RigControllerPropData *prop_data,
                                  RigControllerObjectView *object_view)
{
  RigControllerPropertyView *prop_view =
    rut_object_alloc0 (RigControllerPropertyView,
                       &rig_controller_property_view_type,
                       _rig_controller_property_view_init_type);
  RutProperty *property = prop_data->property;
  const RutPropertySpec *spec = property->spec;

  rut_graphable_init (prop_view);

  rut_list_init (&prop_view->preferred_size_cb_list);

  prop_view->object = object_view;
  prop_view->prop_data = prop_data;
  prop_view->prop_data->property = property;
  prop_view->internal_method_change = false;

  prop_view->stack = rut_stack_new (view->context, 1, 1);
  rut_graphable_add_child (prop_view, prop_view->stack);
  rut_object_unref (prop_view->stack);

  prop_view->bg = rut_rectangle_new4f (view->context, 1, 1,
                                       0.5, 0.5, 0.5, 1);
  rut_stack_add (prop_view->stack, prop_view->bg);
  rut_object_unref (prop_view->bg);

  prop_view->columns_parent = rut_transform_new (view->context);
  rut_stack_add (prop_view->stack, prop_view->columns_parent);
  rut_object_unref (prop_view->columns_parent);

  setup_label_column (prop_view, spec->nick ? spec->nick : spec->name);

  setup_method_drop_down (prop_view);

  update_method_control (prop_view);

  return prop_view;
}

static int
compare_properties_cb (RigControllerPropertyView *prop_view_a,
                       RigControllerPropertyView *prop_view_b)
{
  RutProperty *prop_a = prop_view_a->prop_data->property;
  RutProperty *prop_b = prop_view_b->prop_data->property;
  RutObject *object_a = prop_a->object;
  RutObject *object_b = prop_b->object;
  const RutType *object_type_a = rut_object_get_type (object_a);
  const RutType *object_type_b = rut_object_get_type (object_b);

  if (object_a != object_b)
    {
      /* Make sure to list entity properties first */
      if (object_type_a == &rut_entity_type &&
          object_type_b != &rut_entity_type)
        return -1;
      else if (object_type_b == &rut_entity_type &&
               object_type_a != &rut_entity_type)
        return 1;
      else
        return (long)object_a - (long)object_b;
    }

  return strcmp (prop_a->spec->nick ?
                 prop_a->spec->nick :
                 prop_a->spec->name,
                 prop_b->spec->nick ?
                 prop_b->spec->nick :
                 prop_b->spec->name);
}

static void
_rig_controller_object_view_sort_properties (RigControllerObjectView *object_view)
{
  GList *l;

  object_view->properties =
    g_list_sort (object_view->properties, (GCompareFunc)compare_properties_cb);

  for (l = object_view->properties; l; l = l->next)
    rut_box_layout_remove (object_view->properties_vbox, l->data);

  for (l = object_view->properties; l; l = l->next)
    rut_box_layout_add (object_view->properties_vbox, FALSE, l->data);
}

static void
_rig_controller_object_view_add_property (RigControllerObjectView *object_view,
                                          RigControllerPropertyView *prop_view)
{
  object_view->properties =
    g_list_prepend (object_view->properties, prop_view);

  rut_box_layout_add (object_view->properties_vbox, FALSE, prop_view);

  _rig_controller_object_view_sort_properties (object_view);
}

static void
_rig_controller_object_view_free (void *object)
{
  RigControllerObjectView *object_view = object;
  GList *l, *next;

  for (l = object_view->properties; l; l = next)
    {
      RigControllerPropertyView *prop_view = l->data;
      next = l->next;

      rut_box_layout_remove (object_view->properties_vbox, prop_view);
      rut_object_unref (prop_view);
    }
  g_list_free (object_view->properties);

  rut_graphable_destroy (object_view);

  rut_object_free (RigControllerObjectView, object_view);
}

RutType rig_controller_object_view_type;

static void
_rig_controller_object_view_init_type (void)
{

  static RutGraphableVTable graphable_vtable = {
      NULL, /* child removed */
      NULL, /* child addded */
      NULL /* parent changed */
  };

  static RutSizableVTable sizable_vtable = {
      rut_composite_sizable_set_size,
      rut_composite_sizable_get_size,
      rut_composite_sizable_get_preferred_width,
      rut_composite_sizable_get_preferred_height,
      rut_composite_sizable_add_preferred_size_callback
  };

  RutType *type = &rig_controller_object_view_type;
#define TYPE RigControllerObjectView

  rut_type_init (type, G_STRINGIFY (TYPE), _rig_controller_object_view_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_GRAPHABLE,
                      offsetof (TYPE, graphable),
                      &graphable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_SIZABLE,
                      0, /* no implied properties */
                      &sizable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_COMPOSITE_SIZABLE,
                      offsetof (TYPE, stack),
                      NULL); /* no vtable */

#undef TYPE
}

static int
compare_objects_cb (RigControllerObjectView *object_a,
                    RigControllerObjectView *object_b)
{
  const char *label_a = object_a->label_property ?
    rut_property_get_text (object_a->label_property) : NULL;
  const char *label_b = object_b->label_property ?
    rut_property_get_text (object_b->label_property) : NULL;

  g_return_val_if_fail (rut_object_get_type (object_a) ==
                        &rig_controller_object_view_type, 0);

  if (*label_a == 0)
    label_a = NULL;
  if (*label_b == 0)
    label_b = NULL;

  if (label_a && label_b)
    return strcmp (label_a, label_b);
  else if (label_a)
    return -1;
  else if (label_b)
    return 1;
  else
    return 0;
}

static void
_rig_controller_view_sort_objects (RigControllerView *view)
{
  GList *l;

  view->object_views =
    g_list_sort (view->object_views, (GCompareFunc)compare_objects_cb);

  for (l = view->object_views; l; l = l->next)
    rut_box_layout_remove (view->properties_vbox, l->data);

  for (l = view->object_views; l; l = l->next)
    rut_box_layout_add (view->properties_vbox, FALSE, l->data);
}

static void
update_object_label_cb (RutProperty *target_property, void *user_data)
{
  RigControllerObjectView *object_view = user_data;
  RigControllerView *view = object_view->view;
  const char *label = NULL;

  if (object_view->label_property)
    label = rut_property_get_text (object_view->label_property);

  if (label == NULL || *label == 0)
    label = "Object";

  rut_property_set_text (&view->context->property_ctx,
                         target_property, label);

  _rig_controller_view_sort_objects (view);
}

static RigControllerObjectView *
rig_controller_object_view_new (RigControllerView *view,
                                RutObject *object)
{
  RigControllerObjectView *object_view =
    rut_object_alloc0 (RigControllerObjectView,
                       &rig_controller_object_view_type,
                       _rig_controller_object_view_init_type);
  RutProperty *fold_label_property;
  RutProperty *label_property;
  //RutBoxLayout *hbox;
  //RutIconToggle *eye_toggle;

  rut_graphable_init (object_view);

  object_view->object = object;
  object_view->view = view;

  object_view->stack = rut_stack_new (view->context, 1, 1);
  rut_graphable_add_child (object_view, object_view->stack);
  rut_object_unref (object_view->stack);

  object_view->fold = rut_fold_new (view->context, "<Object>");
  rut_fold_set_font_name (object_view->fold, "Sans Bold");
  rut_stack_add (object_view->stack, object_view->fold);

#if 0
  hbox = rut_box_layout_new (view->context, RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);
  rut_fold_set_header_child (object_view->fold, hbox);
  rut_object_unref (hbox);

  eye_toggle = rut_icon_toggle_new (view->context,
                                    "eye-white.png",
                                    "eye.png");
  rut_box_layout_add (hbox, FALSE, eye_toggle);
  rut_object_unref (eye_toggle);
#endif

  fold_label_property =
    rut_introspectable_lookup_property (object_view->fold, "label");

  label_property = rut_introspectable_lookup_property (object, "label");
  object_view->label_property = label_property;

  if (label_property)
    {
      update_object_label_cb (fold_label_property, object_view);

      rut_property_set_binding (fold_label_property,
                                update_object_label_cb,
                                object_view,
                                label_property,
                                NULL);
    }

  object_view->properties_vbox =
    rut_box_layout_new (view->context, RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM);
  rut_fold_set_child (object_view->fold, object_view->properties_vbox);

  return object_view;
}

#if 0
static void
rig_controller_view_ungrab_input (RigControllerView *view)
{
  if (view->grab_state != RIG_CONTROLLER_VIEW_GRAB_STATE_NO_GRAB)
    {
      rut_shell_ungrab_input (view->context->shell,
                              rig_controller_view_grab_input_cb,
                              view);
      view->grab_state = RIG_CONTROLLER_VIEW_GRAB_STATE_NO_GRAB;
    }
}
#endif

#if 0
static void
rig_controller_view_clear_selected_nodes (RigControllerView *view)
{
  RigControllerViewSelectedNode *selected_node, *t;

  if (rut_list_empty (&view->selected_nodes))
    return;

  rut_list_for_each_safe (selected_node, t, &view->selected_nodes, list_node)
    {
      selected_node->prop_view->has_selected_nodes = FALSE;
      g_slice_free (RigControllerViewSelectedNode, selected_node);
    }

  rut_list_init (&view->selected_nodes);

  view->dots_dirty = TRUE;
}
#endif

static void
rig_controller_view_clear_object_views (RigControllerView *view)
{
  GList *l, *next;

  for (l = view->object_views; l; l = next)
    {
      RigControllerObjectView *object_view = l->data;
      next = l->next;

      rut_object_unref (object_view);
      rut_box_layout_remove (view->properties_vbox, object_view);
    }

  g_list_free (view->object_views);
  view->object_views = NULL;
}

static void
_rig_controller_view_free (void *object)
{
  RigControllerView *view = object;

  //rig_controller_view_ungrab_input (view);

  if (view->separator_pipeline)
    cogl_object_unref (view->separator_pipeline);
  if (view->path_bg_pipeline)
    cogl_object_unref (view->path_bg_pipeline);
  if (view->box_pipeline)
    cogl_object_unref (view->box_pipeline);
  if (view->box_path)
    cogl_object_unref (view->box_path);

  rut_object_unref (view->nodes_selection);

  //rig_controller_view_clear_selected_nodes (view);

  rig_controller_view_clear_object_views (view);

  rut_shell_remove_pre_paint_callback_by_graphable (view->context->shell, view);

  rut_graphable_destroy (view);

  rut_object_free (RigControllerView, view);
}

#if 0
static CoglAttributeBuffer *
rig_controller_view_create_dots_buffer (RigControllerView *view)
{
  size_t size = MAX (8, view->n_dots) * sizeof (RigControllerViewDotVertex);

  return cogl_attribute_buffer_new_with_size (view->context->cogl_context,
                                              size);
}
#endif

#if 0
static CoglPrimitive *
rig_controller_view_create_dots_primitive (RigControllerView *view)
{
  CoglAttribute *attributes[2];
  CoglPrimitive *prim;

  attributes[0] = cogl_attribute_new (view->dots_buffer,
                                      "cogl_position_in",
                                      sizeof (RigControllerViewDotVertex),
                                      offsetof (RigControllerViewDotVertex, x),
                                      2, /* n_components */
                                      COGL_ATTRIBUTE_TYPE_FLOAT);
  attributes[1] = cogl_attribute_new (view->dots_buffer,
                                      "cogl_color_in",
                                      sizeof (RigControllerViewDotVertex),
                                      offsetof (RigControllerViewDotVertex, r),
                                      4, /* n_components */
                                      COGL_ATTRIBUTE_TYPE_UNSIGNED_BYTE);

  prim = cogl_primitive_new_with_attributes (COGL_VERTICES_MODE_POINTS,
                                             view->n_dots,
                                             attributes,
                                             2 /* n_attributes */);

  cogl_object_unref (attributes[0]);
  cogl_object_unref (attributes[1]);

  return prim;
}
#endif

#if 0
typedef struct
{
  RigControllerView *view;
  RigControllerPropertyView *prop_view;

  RigControllerViewDotVertex *v;
  int row_pos;
} RigControllerViewDotData;

static void
rig_controller_view_add_dot_unselected (RigControllerViewDotData *dot_data,
                                        RigNode *node)
{
  RigControllerViewDotVertex *v = dot_data->v;

  v->x = node->t;
  v->y = dot_data->row_pos;
  *(uint32_t *) &v->r = RIG_CONTROLLER_VIEW_UNSELECTED_COLOR;

  dot_data->v++;
}
#endif

#if 0
static void
rig_controller_view_add_dot_selected (RigControllerViewDotData *dot_data,
                                      RigNode *node)
{
  RigControllerViewDotVertex *v = dot_data->v;
  uint32_t color = RIG_CONTROLLER_VIEW_UNSELECTED_COLOR;
  RigControllerViewSelectedNode *selected_node;

  rut_list_for_each (selected_node, &dot_data->view->selected_nodes, list_node)
    {
      if (selected_node->prop_view == dot_data->prop_view &&
          selected_node->node == node)
        {
          color = RIG_CONTROLLER_VIEW_SELECTED_COLOR;
          break;
        }
    }

  v->x = node->t;
  v->y = dot_data->row_pos;
  *(uint32_t *) &v->r = color;

  dot_data->v++;
}
#endif

#if 0
static void
rig_controller_view_update_dots_buffer (RigControllerView *view)
{
  RigControllerObjectView *object;
  CoglBool buffer_is_mapped;
  RigControllerViewDotVertex *buffer_data;
  RigControllerViewDotData dot_data;
  size_t map_size = sizeof (RigControllerViewDotVertex) * view->n_dots;
  CoglError *ignore_error = NULL;

  if (view->n_dots == 0)
    return;

  buffer_data = cogl_buffer_map_range (view->dots_buffer,
                                       0, /* offset */
                                       map_size,
                                       COGL_BUFFER_ACCESS_WRITE,
                                       COGL_BUFFER_MAP_HINT_DISCARD,
                                       &ignore_error);

  if (buffer_data == NULL)
    {
      buffer_data = g_malloc (map_size);
      buffer_is_mapped = FALSE;
      cogl_error_free (ignore_error);
    }
  else
    buffer_is_mapped = TRUE;

  dot_data.view = view;
  dot_data.v = buffer_data;
  dot_data.row_pos = 0;

  rut_list_for_each (object, &view->object_views, list_node)
    {
      dot_data.row_pos++;

      rut_list_for_each (dot_data.prop_view, &object->properties, list_node)
        {
          RigControllerPropData *prop_data = dot_data.prop_view->prop_data;
          RigPath *path;
          RigNode *node;

          if (prop_data->method != RIG_CONTROLLER_METHOD_PATH)
            {
              dot_data.row_pos++;
              continue;
            }

          path = rig_controller_get_path_for_property (view->controller,
                                                       prop_data->property);

          if (dot_data.prop_view->has_selected_nodes)
            rut_list_for_each (node, &path->nodes, list_node)
              rig_controller_view_add_dot_selected (&dot_data,
                                                    node);
          else
            rut_list_for_each (node, &path->nodes, list_node)
              rig_controller_view_add_dot_unselected (&dot_data,
                                                      node);

          dot_data.row_pos++;
        }
    }

  if (buffer_is_mapped)
    cogl_buffer_unmap (view->dots_buffer);
  else
    {
      cogl_buffer_set_data (view->dots_buffer,
                            0, /* offset */
                            buffer_data,
                            map_size,
                            NULL);
      g_free (buffer_data);
    }
}
#endif

#if 0
static void
rig_controller_view_draw_box (RigControllerView *view,
                              CoglFramebuffer *fb)
{
  if (view->box_pipeline == NULL)
    {
      view->box_pipeline = cogl_pipeline_new (view->context->cogl_context);
      cogl_pipeline_set_color4ub (view->box_pipeline, 0, 0, 0, 255);
    }

  if (view->box_path == NULL)
    {
      view->box_path = cogl_path_new (view->context->cogl_context);
      cogl_path_rectangle (view->box_path,
                           view->nodes_x + view->box_x1 * view->nodes_width,
                           view->box_y1 * view->row_height,
                           view->nodes_x + view->box_x2 * view->nodes_width,
                           view->box_y2 * view->row_height);
    }

  cogl_path_stroke (view->box_path, fb, view->box_pipeline);
}
#endif

#if 0
static void
_rig_controller_view_paint (RutObject *object,
                            RutPaintContext *paint_ctx)
{
  RigControllerView *view = object;
  CoglFramebuffer *fb = rut_camera_get_framebuffer (paint_ctx->camera);

  if (view->separator_pipeline)
    cogl_framebuffer_draw_rectangle (fb,
                                     view->separator_pipeline,
                                     view->nodes_x -
                                     view->separator_width,
                                     0,
                                     view->nodes_x,
                                     view->total_height);

  draw_timeline_background (view, fb);

  if (view->dots_dirty)
    {
      if (view->dots_buffer == NULL)
        view->dots_buffer = rig_controller_view_create_dots_buffer (view);
      else
        {
          int old_n_vertices =
            (cogl_buffer_get_size (view->dots_buffer) /
             sizeof (RigControllerViewDotVertex));

          if (old_n_vertices < view->n_dots)
            {
              cogl_object_unref (view->dots_buffer);
              cogl_object_unref (view->dots_primitive);
              view->dots_primitive = NULL;
              view->dots_buffer = rig_controller_view_create_dots_buffer (view);
            }
        }

      if (view->dots_primitive == NULL)
        view->dots_primitive = rig_controller_view_create_dots_primitive (view);
      else
        cogl_primitive_set_n_vertices (view->dots_primitive, view->n_dots);

      rig_controller_view_update_dots_buffer (view);

      view->dots_dirty = FALSE;
    }

  /* The transform is set up so that 0→1 along the x-axis extends
   * across the whole timeline. Along the y-axis 1 unit represents the
   * height of one row. This is done so that the changing the size of
   * the controller view doesn't require updating the dots buffer. It
   * doesn't matter that the scale isn't uniform because the dots are
   * drawn as points which are always sized in framebuffer pixels
   * regardless of the transformation */

  cogl_framebuffer_push_rectangle_clip (fb,
                                        view->nodes_x,
                                        0.0f,
                                        view->nodes_x + view->nodes_width,
                                        view->total_height);

  if (view->n_dots > 0)
    {
      cogl_framebuffer_push_matrix (fb);

      cogl_framebuffer_translate (fb,
                                  view->nodes_x,
                                  view->row_height * 0.5f,
                                  0.0f);
      cogl_framebuffer_scale (fb,
                              view->nodes_width,
                              view->row_height,
                              1.0f);

      cogl_primitive_draw (view->dots_primitive,
                           fb,
                           view->dots_pipeline);

      cogl_framebuffer_pop_matrix (fb);
    }

  {
    float progress_x =
      view->nodes_x +
      rut_timeline_get_progress (view->timeline) *
      view->nodes_width;

    cogl_framebuffer_draw_rectangle (fb,
                                     view->progress_pipeline,
                                     progress_x -
                                     RIG_CONTROLLER_VIEW_PROGRESS_WIDTH / 2.0f,
                                     -10000.0f,
                                     progress_x +
                                     RIG_CONTROLLER_VIEW_PROGRESS_WIDTH / 2.0f,
                                     10000.0f);
  }

  if (view->grab_state == RIG_CONTROLLER_VIEW_GRAB_STATE_DRAW_BOX)
    rig_controller_view_draw_box (view, fb);

  cogl_framebuffer_pop_clip (fb);
}
#endif

#if 0
static void
rig_controller_view_allocate_cb (RutObject *graphable,
                                 void *user_data)
{
  RigControllerView *view = RIG_CONTROLLER_VIEW (graphable);
  float column_widths[RIG_CONTROLLER_VIEW_N_COLUMNS];
  float row_height = 0.0f;
  RigControllerObjectView *object;
  int i;
  int row_num;

  memset (column_widths, 0, sizeof (column_widths));

  /* Everything in a single column will be allocated to the same
   * width and everything will be allocated to the same height */

  rut_list_for_each (object, &view->objects, list_node)
    {
      RigControllerPropertyView *prop_view;

      row_height = 0;
      for (i = 0; i < RIG_CONTROLLER_VIEW_N_OBJECT_COLUMNS; i++)
        {
          RigControllerViewColumn *column = object->columns + i;
          float width, height;

          rut_sizable_get_preferred_width (column->control,
                                           -1, /* for_height */
                                           NULL, /* min_width_p */
                                           &width);
          rut_sizable_get_preferred_height (column->control,
                                            width,
                                            NULL, /* min_height_p */
                                            &height);

          if (width > column_widths[i])
            column_widths[i] = width + RIG_CONTROLLER_VIEW_PADDING;
          if (height > row_height)
            row_height = height;
        }

      rut_list_for_each (prop_view, &object->properties, list_node)
        {
          for (i = 0; i < RIG_CONTROLLER_VIEW_N_PROPERTY_COLUMNS; i++)
            {
              RigControllerViewColumn *column = prop_view->columns + i;
              float width, height;

              rut_sizable_get_preferred_width (column->control,
                                               -1, /* for_height */
                                               NULL, /* min_width_p */
                                               &width);
              rut_sizable_get_preferred_height (column->control,
                                                width,
                                                NULL, /* min_height_p */
                                                &height);

              if (i == 0)
                width += RIG_CONTROLLER_VIEW_PROPERTY_INDENTATION;

              if (width > column_widths[i])
                column_widths[i] = width + RIG_CONTROLLER_VIEW_PADDING;
              if (height > row_height)
                row_height = height;
            }
        }
    }

  row_num = 0;

  rut_list_for_each (object, &view->objects, list_node)
    {
      RigControllerPropertyView *prop_view;
      float x = 0.0f;

      for (i = 0; i < RIG_CONTROLLER_VIEW_N_OBJECT_COLUMNS; i++)
        {
          RigControllerViewColumn *column = object->columns + i;

          rut_transform_init_identity (column->transform);
          rut_transform_translate (column->transform,
                                   nearbyintf (x + RIG_CONTROLLER_VIEW_PADDING),
                                   nearbyintf (row_num * row_height),
                                   0.0f);
          rut_sizable_set_size (column->control,
                                nearbyintf (column_widths[i]),
                                nearbyintf (row_height));

          x += column_widths[i];

          row_num++;
        }

      rut_list_for_each (prop_view, &object->properties, list_node)
        {
          x = 0.0f;

          for (i = 0; i < RIG_CONTROLLER_VIEW_N_PROPERTY_COLUMNS; i++)
            {
              RigControllerViewColumn *column = prop_view->columns + i;
              int width = nearbyintf (column_widths[i]);

              if (i == 0)
                {
                  x += RIG_CONTROLLER_VIEW_PROPERTY_INDENTATION;
                  width -= RIG_CONTROLLER_VIEW_PROPERTY_INDENTATION;
                }

              rut_transform_init_identity (column->transform);
              rut_transform_translate (column->transform,
                                       x,
                                       nearbyintf (row_num * row_height),
                                       0.0f);
              rut_sizable_set_size (column->control,
                                    width,
                                    nearbyintf (row_height));
              x = nearbyintf (x + RIG_CONTROLLER_VIEW_PADDING + width);
            }
          row_num++;
        }
    }

  {
    float controls_width = 0;
    for (i = 0; i < RIG_CONTROLLER_VIEW_N_COLUMNS; i++)
      controls_width += column_widths[i];
    controls_width = nearbyintf (controls_width + RIG_CONTROLLER_VIEW_PADDING);

    view->nodes_x = controls_width + view->separator_width;
    view->nodes_width = view->total_width - view->nodes_x;
  }

  rut_input_region_set_rectangle (view->input_region,
                                  view->nodes_x, /* x0 */
                                  0.0f, /* y0 */
                                  view->nodes_x + view->nodes_width,
                                  view->total_height /* y1 */);

  view->row_height = nearbyintf (row_height);
}
#endif

#if 0
static void
handle_control_width (RigControllerViewColumn *column,
                      float indentation,
                      float *min_width_p,
                      float *natural_width_p)
{
  float min_width, natural_width;

  rut_sizable_get_preferred_width (column->control,
                                   -1, /* for_height */
                                   &min_width,
                                   &natural_width);

  min_width += indentation;
  natural_width += indentation;

  if (natural_width > *natural_width_p)
    *natural_width_p = natural_width;
  if (min_width > *min_width_p)
    *min_width_p = min_width;
}

static void
handle_control_height (RigControllerViewColumn *column,
                       float *row_height)
{
  float natural_height;

  rut_sizable_get_preferred_height (column->control,
                                    -1, /* for_width */
                                    NULL, /* min_height */
                                    &natural_height);

  if (natural_height > *row_height)
    *row_height = natural_height;
}
#endif

RutType rig_controller_view_type;

static void
_rig_controller_view_init_type (void)
{

  static RutGraphableVTable graphable_vtable = {
      NULL, /* child removed */
      NULL, /* child addded */
      NULL /* parent changed */
  };

  static RutSizableVTable sizable_vtable = {
      rut_composite_sizable_set_size,
      rut_composite_sizable_get_size,
      rut_composite_sizable_get_preferred_width,
      rut_composite_sizable_get_preferred_height,
      rut_composite_sizable_add_preferred_size_callback
  };

  RutType *type = &rig_controller_view_type;
#define TYPE RigControllerView

  rut_type_init (type, G_STRINGIFY (TYPE), _rig_controller_view_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_GRAPHABLE,
                      offsetof (TYPE, graphable),
                      &graphable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_SIZABLE,
                      0, /* no implied properties */
                      &sizable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_COMPOSITE_SIZABLE,
                      offsetof (TYPE, vbox),
                      NULL); /* no vtable */

#undef TYPE
}

#if 0
static CoglBool
rig_controller_view_select_node (RigControllerView *view,
                                 RigControllerPropertyView *prop_view,
                                 RigNode *node)
{
  RigControllerViewSelectedNode *selected_node;

  /* Check if the node is already selected */
  if (prop_view->has_selected_nodes)
    {
      rut_list_for_each (selected_node, &view->selected_nodes, list_node)
        {
          if (selected_node->prop_view == prop_view &&
              selected_node->node == node)
            return TRUE;
        }
    }

  selected_node = g_slice_new0 (RigControllerViewSelectedNode);
  selected_node->prop_view = prop_view;
  selected_node->node = node;

  prop_view->has_selected_nodes = TRUE;
  view->dots_dirty = TRUE;

  rut_list_insert (view->selected_nodes.prev, &selected_node->list_node);

  return FALSE;
}
#endif

static void
rig_controller_view_property_added (RigControllerView *view,
                                    RigControllerPropData *prop_data)
{
  RutProperty *property = prop_data->property;
  RigControllerObjectView *object_view;
  RigControllerPropertyView *prop_view;
  RutObject *object;
  GList *l;

  object = property->object;

  /* If the property belongs to a component then we'll group the
   * property according to the component's object instead */
  if (rut_object_is (object, RUT_TRAIT_ID_COMPONENTABLE))
    {
      RutComponentableProps *component =
        rut_object_get_properties (object, RUT_TRAIT_ID_COMPONENTABLE);

      if (component->entity)
        object = component->entity;
    }

  /* Check if we already have this object */
  for (l = view->object_views; l; l= l->next)
    {
      object_view = l->data;
      if (object_view->object == object)
        goto have_object;
    }

  object_view = rig_controller_object_view_new (view, object);
  view->object_views = g_list_prepend (view->object_views, object_view);

  rut_box_layout_add (view->properties_vbox, FALSE, object_view);

  _rig_controller_view_sort_objects (view);

 have_object:

  prop_view = rig_controller_property_view_new (view, prop_data, object_view);

  _rig_controller_object_view_add_property (object_view, prop_view);
}

static RigControllerPropertyView *
rig_controller_view_find_property (RigControllerView *view,
                                   RutProperty *property)
{
  RutObject *object = property->object;
  GList *l, *l2;

  /* If the property belongs to a component then it is grouped by
   * component's entity instead */
  if (rut_object_is (object, RUT_TRAIT_ID_COMPONENTABLE))
    {
      RutComponentableProps *component =
        rut_object_get_properties (object, RUT_TRAIT_ID_COMPONENTABLE);

      if (component->entity)
        object = component->entity;
    }

  for (l = view->object_views; l; l = l->next)
    {
      RigControllerObjectView *object_view = l->data;

      if (object_view->object == object)
        {
          for (l2 = object_view->properties; l2; l2 = l2->next)
            {
              RigControllerPropertyView *prop_view = l2->data;
              if (prop_view->prop_data->property == property)
                return prop_view;
            }
        }
    }

  return NULL;
}

static void
rig_controller_view_property_removed (RigControllerView *view,
                                      RutProperty *property)
{
  RigControllerPropertyView *prop_view =
    rig_controller_view_find_property (view, property);
  RigControllerObjectView *object_view;

  if (prop_view == NULL)
    return;

  object_view = prop_view->object;

  object_view->properties =
    g_list_remove (object_view->properties, prop_view);
  rut_object_unref (prop_view);
  rut_box_layout_remove (object_view->properties_vbox, prop_view);

  /* If that was the last property on the object then we'll also
   * remove the object */
  if (!object_view->properties)
    {
      view->object_views = g_list_remove (view->object_views, object_view);
      rut_object_unref (object_view);
      rut_box_layout_remove (view->properties_vbox, object_view);
    }

  rut_shell_queue_redraw (view->context->shell);
}

#if 0
static CoglPipeline *
rig_controller_view_create_dots_pipeline (RigControllerView *view)
{
  CoglPipeline *pipeline = cogl_pipeline_new (view->context->cogl_context);
  char *dot_filename;
  CoglBitmap *bitmap;
  CoglError *error = NULL;

  dot_filename = rut_find_data_file ("dot.png");

  if (dot_filename == NULL)
    {
      g_warning ("Couldn't find dot.png");
      bitmap = NULL;
    }
  else
    {
      bitmap = cogl_bitmap_new_from_file (view->context->cogl_context,
                                          dot_filename,
                                          &error);
      g_free (dot_filename);
    }

  if (bitmap == NULL)
    {
      g_warning ("Error loading dot.png: %s", error->message);
      cogl_error_free (error);
    }
  else
    {
      CoglTexture2D *texture =
        cogl_texture_2d_new_from_bitmap (bitmap,
                                         COGL_PIXEL_FORMAT_ANY,
                                         &error);

      if (texture == NULL)
        {
          g_warning ("Error loading dot.png: %s", error->message);
          cogl_error_free (error);
        }
      else
        {
          if (!cogl_pipeline_set_layer_point_sprite_coords_enabled (pipeline,
                                                                    0,
                                                                    TRUE,
                                                                    &error))
            {
              g_warning ("Error enabling point sprite coords: %s",
                         error->message);
              cogl_error_free (error);
              cogl_pipeline_remove_layer (pipeline, 0);
            }
          else
            {
              cogl_pipeline_set_layer_texture (pipeline,
                                               0, /* layer_num */
                                               texture);
              cogl_pipeline_set_layer_filters
                (pipeline,
                 0, /* layer_num */
                 COGL_PIPELINE_FILTER_LINEAR_MIPMAP_NEAREST,
                 COGL_PIPELINE_FILTER_LINEAR);
              cogl_pipeline_set_layer_wrap_mode
                (pipeline,
                 0, /* layer_num */
                 COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);
            }

          cogl_object_unref (texture);
        }

      cogl_object_unref (bitmap);
    }

  return pipeline;
}
#endif

static void
rig_controller_view_create_separator_pipeline (RigControllerView *view)
{
  GError *error = NULL;
  CoglTexture *texture;

  texture = rut_load_texture_from_data_file (view->context,
                                             "controller-view-separator.png",
                                             &error);

  if (texture)
    {
      CoglPipeline *pipeline = cogl_pipeline_new (view->context->cogl_context);

      view->separator_pipeline = pipeline;
      view->separator_width = cogl_texture_get_width (texture);

      cogl_pipeline_set_layer_texture (pipeline,
                                       0, /* layer_num */
                                       texture);
      cogl_pipeline_set_layer_filters
        (pipeline,
         0, /* layer_num */
         COGL_PIPELINE_FILTER_LINEAR_MIPMAP_NEAREST,
         COGL_PIPELINE_FILTER_LINEAR);
      cogl_pipeline_set_layer_wrap_mode
        (pipeline,
         0, /* layer_num */
         COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);

      cogl_object_unref (texture);
    }
  else
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }
}

#if 0
static void
rig_controller_view_get_time_from_event (RigControllerView *view,
                                         RutInputEvent *event,
                                         float *time,
                                         int *row)
{
  float x = rut_motion_event_get_x (event);
  float y = rut_motion_event_get_y (event);

  if (!rut_motion_event_unproject (event, view, &x, &y))
    g_error ("Failed to get inverse transform");

  if (time)
    *time = (x - view->nodes_x) / view->nodes_width;
  if (row)
    *row = nearbyintf (y / view->row_height);
}

static void
rig_controller_view_update_timeline_progress (RigControllerView *view,
                                              RutInputEvent *event)
{
  float progress;
  rig_controller_view_get_time_from_event (view, event, &progress, NULL);
  rut_timeline_set_progress (view->timeline, progress);
  rut_shell_queue_redraw (view->context->shell);
}
#endif

static RigNode *
rig_controller_view_find_node_in_path (RigControllerView *view,
                                       RigPath *path,
                                       float min_progress,
                                       float max_progress)
{
  RigNode *node;

  rut_list_for_each (node, &path->nodes, list_node)
    if (node->t >= min_progress && node->t <= max_progress)
      return node;

  return NULL;
}

#if 0
static CoglBool
rig_controller_view_find_node (RigControllerView *view,
                               RutInputEvent *event,
                               RigControllerPropertyView **prop_view_out,
                               RigNode **node_out)
{
  float x = rut_motion_event_get_x (event);
  float y = rut_motion_event_get_y (event);
  float progress;
  RigControllerObjectView *object_view;
  int row_num = 0;

  if (!rut_motion_event_unproject (event, view, &x, &y))
    {
      g_error ("Failed to get inverse transform");
      return FALSE;
    }

  progress = (x - view->nodes_x) / view->nodes_width;

  if (progress < 0.0f || progress > 1.0f)
    return FALSE;

  rut_list_for_each (object_view, &view->object_views, list_node)
    {
      RigControllerPropertyView *prop_view;

      row_num++;

      rut_list_for_each (prop_view, &object_view->properties, list_node)
        {
          if (row_num == (int) (y / view->row_height))
            {
              float scaled_dot_size =
                view->node_size / (float) view->nodes_width;
              RigNode *node;

              node =
                rig_controller_view_find_node_in_path (view,
                                                       prop_view->path,
                                                       progress -
                                                       scaled_dot_size / 2.0f,
                                                       progress +
                                                       scaled_dot_size / 2.0f);
              if (node)
                {
                  *node_out = node;
                  *prop_view_out = prop_view;
                  return TRUE;
                }

              return FALSE;
            }

          row_num++;
        }
    }

  return FALSE;
}

static void
rig_controller_view_handle_select_event (RigControllerView *view,
                                         RutInputEvent *event)
{
  RigControllerPropertyView *prop_view;
  RigNode *node;

  if (rig_controller_view_find_node (view, event, &prop_view, &node))
    {
      if ((rut_motion_event_get_modifier_state (event) &
           (RUT_MODIFIER_LEFT_SHIFT_ON |
            RUT_MODIFIER_RIGHT_SHIFT_ON)) == 0)
        rig_controller_view_clear_selected_nodes (view);

      /* If shift is down then we actually want to toggle the node. If
       * the node is already selected then trying to select it again
       * will return TRUE so we know to remove it. If shift wasn't
       * down then it definitely won't be selected because we'll have
       * just cleared the selection above so it doesn't matter if we
       * toggle it */
      if (rig_controller_view_select_node (view, prop_view, node))
        rig_controller_view_unselect_node (view, prop_view, node);

      rut_timeline_set_progress (view->timeline, node->t);

      rut_shell_queue_redraw (view->context->shell);
    }
  else
    {
      rig_controller_view_clear_selected_nodes (view);
      rig_controller_view_update_timeline_progress (view, event);
    }
}
#endif

#if 0
static void
rig_controller_view_decide_grab_state (RigControllerView *view,
                                       RutInputEvent *event)
{
  RigControllerPropertyView *prop_view;
  RigNode *node;

  if ((rut_motion_event_get_modifier_state (event) &
       (RUT_MODIFIER_LEFT_SHIFT_ON |
        RUT_MODIFIER_RIGHT_SHIFT_ON)))
    {
      rig_controller_view_get_time_from_event (view,
                                               event,
                                               &view->box_x1,
                                               &view->box_y1);
      view->box_x2 = view->box_x1;
      view->box_y2 = view->box_y1;

      view->grab_state = RIG_CONTROLLER_VIEW_GRAB_STATE_DRAW_BOX;
    }
  else if (rig_controller_view_find_node (view, event, &prop_view, &node))
    {
      if (!rig_controller_view_select_node (view, prop_view, node))
        {
          /* If the node wasn't already selected then we only want
           * this node to be selected */
          rig_controller_view_clear_selected_nodes (view);
          rig_controller_view_select_node (view, prop_view, node);
        }

      rig_controller_view_get_time_from_event (view,
                                               event,
                                               &view->drag_start_position,
                                               NULL);

      rig_controller_view_calculate_drag_offset_range (view);

      rut_shell_queue_redraw (view->context->shell);

      view->grab_state = RIG_CONTROLLER_VIEW_GRAB_STATE_DRAGGING_NODES;
    }
  else
    {
      rig_controller_view_clear_selected_nodes (view);

      view->grab_state = RIG_CONTROLLER_VIEW_GRAB_STATE_MOVING_TIMELINE;
    }
}

static void
rig_controller_view_drag_nodes (RigControllerView *view,
                                RutInputEvent *event)
{
  RigControllerViewSelectedNode *selected_node;
  float position;
  float offset;
  RigControllerObjectView *object_view;

  rig_controller_view_get_time_from_event (view, event, &position, NULL);
  offset = position - view->drag_start_position;

  offset = CLAMP (offset, view->min_drag_offset, view->max_drag_offset);

  rut_list_for_each (selected_node, &view->selected_nodes, list_node)
    rig_path_move_node (selected_node->prop_view->path,
                        selected_node->node,
                        selected_node->original_time + offset);

  view->drag_offset = offset;

  /* Update all the properties that have selected nodes according to
   * the new node positions */
  rut_list_for_each (object_view, &view->object_views, list_node)
    {
      RigControllerPropertyView *prop_view;

      rut_list_for_each (prop_view, &object_view->properties, list_node)
        {
          if (prop_view->has_selected_nodes)
            rig_controller_update_property (view->controller,
                                            prop_view->prop_data->property);
        }
    }
}
#endif

#if 0
static void
rig_controller_view_commit_dragged_nodes (RigControllerView *view)
{
  RigControllerViewSelectedNode *selected_node;
  int n_nodes, i;
  RigUndoJournalPathNode *nodes;

  n_nodes = rut_list_length (&view->selected_nodes);

  nodes = g_alloca (sizeof (RigUndoJournalPathNode) * n_nodes);

  i = 0;
  rut_list_for_each (selected_node, &view->selected_nodes, list_node)
    {
      /* Reset all of the nodes to their original position so that the
       * undo journal can see it */
      selected_node->node->t = selected_node->original_time;

      nodes[i].property = selected_node->prop_view->prop_data->property;
      nodes[i].node = selected_node->node;

      i++;
    }

  rig_undo_journal_move_path_nodes (view->undo_journal,
                                    view->controller,
                                    view->drag_offset,
                                    nodes,
                                    n_nodes);
}
#endif

#if 0
static void
rig_controller_view_update_box (RigControllerView *view,
                                RutInputEvent *event)
{
  rig_controller_view_get_time_from_event (view,
                                           event,
                                           &view->box_x2,
                                           &view->box_y2);

  if (view->box_path)
    {
      cogl_object_unref (view->box_path);
      view->box_path = NULL;
    }

  rut_shell_queue_redraw (view->context->shell);
}
#endif

#if 0
static void
rig_controller_view_commit_box (RigControllerView *view)
{
  RigControllerObjectView *object;
  RigControllerPropertyView *prop_view;
  float x1, x2;
  int y1, y2;
  int row_pos = 0;

  if (view->box_x2 < view->box_x1)
    {
      x1 = view->box_x2;
      x2 = view->box_x1;
    }
  else
    {
      x1 = view->box_x1;
      x2 = view->box_x2;
    }

  if (view->box_y2 < view->box_y1)
    {
      y1 = view->box_y2;
      y2 = view->box_y1;
    }
  else
    {
      y1 = view->box_y1;
      y2 = view->box_y2;
    }

  rut_list_for_each (object, &view->object_views, list_node)
    {
      row_pos++;

      rut_list_for_each (prop_view, &object->properties, list_node)
        {
          if (row_pos >= y1 && row_pos < y2)
            {
              RigNode *node;
              RigControllerPropData *prop_data = prop_view->prop_data;
              RigPath *path =
                rig_controller_get_path_for_property (view->controller,
                                                      prop_data->property);

              rut_list_for_each (node, &path->nodes, list_node)
                if (node->t >= x1 && node->t < x2)
                  rig_controller_view_select_node (view, prop_view, node);
            }

          row_pos++;
        }
    }

  rut_shell_queue_redraw (view->context->shell);
}
#endif

#if 0
static RutInputEventStatus
rig_controller_view_grab_input_cb (RutInputEvent *event,
                                   void *user_data)
{
  RigControllerView *view = user_data;

  if (rut_input_event_get_type (event) != RUT_INPUT_EVENT_TYPE_MOTION)
    return RUT_INPUT_EVENT_STATUS_UNHANDLED;

  if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_MOVE)
    {
      if (view->grab_state == RIG_CONTROLLER_VIEW_GRAB_STATE_UNDECIDED)
        rig_controller_view_decide_grab_state (view, event);

      switch (view->grab_state)
        {
	case RIG_CONTROLLER_VIEW_GRAB_STATE_NO_GRAB:
	case RIG_CONTROLLER_VIEW_GRAB_STATE_UNDECIDED:
          g_assert_not_reached ();

        case RIG_CONTROLLER_VIEW_GRAB_STATE_DRAGGING_NODES:
          rig_controller_view_drag_nodes (view, event);
          break;

        case RIG_CONTROLLER_VIEW_GRAB_STATE_MOVING_TIMELINE:
          rig_controller_view_update_timeline_progress (view, event);
          break;

        case RIG_CONTROLLER_VIEW_GRAB_STATE_DRAW_BOX:
          rig_controller_view_update_box (view, event);
          break;
        }

      return RUT_INPUT_EVENT_STATUS_HANDLED;
    }
  else if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_UP &&
           (rut_motion_event_get_button_state (event) &
            RUT_BUTTON_STATE_1) == 0)
    {
      switch (view->grab_state)
        {
	case RIG_CONTROLLER_VIEW_GRAB_STATE_NO_GRAB:
          g_assert_not_reached ();
          break;

        case RIG_CONTROLLER_VIEW_GRAB_STATE_MOVING_TIMELINE:
          break;

        case RIG_CONTROLLER_VIEW_GRAB_STATE_UNDECIDED:
          rig_controller_view_handle_select_event (view, event);
          break;

        case RIG_CONTROLLER_VIEW_GRAB_STATE_DRAGGING_NODES:
          rig_controller_view_commit_dragged_nodes (view);
          break;

        case RIG_CONTROLLER_VIEW_GRAB_STATE_DRAW_BOX:
          rig_controller_view_commit_box (view);
          break;
        }

      rig_controller_view_ungrab_input (view);

      return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}
#endif

#if 0
static void
rig_controller_view_delete_selected_nodes (RigControllerView *view)
{
  if (!rut_list_empty (&view->selected_nodes))
    {
      RigUndoJournal *journal;

      /* If there is only one selected node then we'll just make a
       * single entry directly in the main undo journal. Otherwise
       * we'll create a subjournal to lump together all of the deletes
       * as one action */
      if (view->selected_nodes.next == view->selected_nodes.prev)
        journal = view->undo_journal;
      else
        journal = rig_undo_journal_new (view->undo_journal->engine);

      while (!rut_list_empty (&view->selected_nodes))
        {
          RigControllerViewSelectedNode *node =
            rut_container_of (view->selected_nodes.next, node, list_node);
          RigControllerPropData *prop_data = node->prop_view->prop_data;

          rig_undo_journal_delete_path_node (journal,
                                             view->controller,
                                             prop_data->property,
                                             node->node);
        }

      if (journal != view->undo_journal)
        rig_undo_journal_log_subjournal (view->undo_journal, journal, FALSE);
    }
}
#endif

#if 0
static RutInputEventStatus
rig_controller_view_input_region_cb (RutInputRegion *region,
                                     RutInputEvent *event,
                                     void *user_data)
{
  RigControllerView *view = user_data;

  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION)
    {
      if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_DOWN &&
          (rut_motion_event_get_button_state (event) & RUT_BUTTON_STATE_1) &&
          view->grab_state == RIG_CONTROLLER_VIEW_GRAB_STATE_NO_GRAB)
        {
          /* We want to delay doing anything in response to the click
           * until the grab callback because we will do different
           * things depending on whether the next event is a move or a
           * release */
          view->grab_state = RIG_CONTROLLER_VIEW_GRAB_STATE_UNDECIDED;
          rut_shell_grab_input (view->context->shell,
                                rut_input_event_get_camera (event),
                                rig_controller_view_grab_input_cb,
                                view);

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }
  else if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_KEY &&
           rut_key_event_get_action (event) == RUT_KEY_EVENT_ACTION_DOWN)
    {
      switch (rut_key_event_get_keysym (event))
        {
        case RUT_KEY_Delete:
          rig_controller_view_delete_selected_nodes (view);
          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}
#endif

RutClosure *
rig_controller_view_add_controller_changed_callback (
                              RigControllerView *view,
                              RigControllerViewControllerChangedCallback callback,
                              void *user_data,
                              RutClosureDestroyCallback destroy_cb)
{
  return rut_closure_list_add (&view->controller_changed_cb_list,
                               callback,
                               user_data,
                               destroy_cb);
}

static void
add_property_cb (RigControllerPropData *prop_data,
                 void *user_data)
{
  RigControllerView *view = user_data;

  rig_controller_view_property_added (view, prop_data);
}

static void
controller_operation_cb (RigController *controller,
                         RigControllerOperation op,
                         RigControllerPropData *prop_data,
                         void *user_data)
{
  RigControllerView *view = user_data;

  switch (op)
    {
    case RIG_CONTROLLER_OPERATION_ADDED:
      rig_controller_view_property_added (view, prop_data);
      break;

    case RIG_CONTROLLER_OPERATION_REMOVED:
      rig_controller_view_property_removed (view, prop_data->property);
      break;

    case RIG_CONTROLLER_OPERATION_METHOD_CHANGED:
      {
        RigControllerPropertyView *prop_view =
          rig_controller_view_find_property (view, prop_data->property);
        RutBin *bin = prop_view->columns[1].control;
        RutDropDown *drop_down = rut_bin_get_child (bin);

        /* Normally we listen for _drop_down changes, but in this case
         * where we are updating the drop down ourselves we need to
         * know to ignore the corresponding notification about the
         * _drop_down changing, otherwise - for example - we'll end up
         * logging into the journal recursively. */
        prop_view->internal_method_change = true;
        rut_drop_down_set_value (drop_down, prop_data->method);
        prop_view->internal_method_change = false;
        break;
      }
    }
}

static void
on_scale_focus_change_cb (RutProperty *target_property,
                          void *user_data)
{
  RigControllerView *view = user_data;

  if (!rig_controller_get_running (view->controller))
    {
      rig_controller_set_elapsed (view->controller,
                                  rut_property_get_float (target_property));
    }
}

RigController *
rig_controller_view_get_controller (RigControllerView *view)
{
  return view->controller;
}

void
rig_controller_view_set_controller (RigControllerView *view,
                                    RigController *controller)
{
  RutProperty *scale_len_prop;

  if (view->controller == controller)
    return;

  rut_closure_list_invoke (&view->controller_changed_cb_list,
                           RigControllerViewControllerChangedCallback,
                           view,
                           controller);

  scale_len_prop =
    rut_introspectable_lookup_property (view->scale, "length");

  if (view->controller)
    {
      RutProperty *controller_elapsed_prop =
        rut_introspectable_lookup_property (view->controller, "elapsed");

      rig_controller_view_clear_object_views (view);

      rut_closure_disconnect (view->controller_op_closure);
      rut_property_remove_binding (scale_len_prop);
      rut_property_remove_binding (controller_elapsed_prop);
#warning "FIXME: clean up more state when switching controllers"

      _rig_nodes_selection_cancel (view->nodes_selection);

      rut_object_unref (view->controller);
    }

  view->controller = controller;
  if (controller)
    {
      RutProperty *controller_len_prop;
      RutProperty *scale_focus_prop;

      rut_object_ref (controller);

      rig_controller_set_active (controller, true);

      /* Add all of the existing properties from the controller */
      rig_controller_foreach_property (controller,
                                       add_property_cb,
                                       view);

      /* Listen for properties that are added/removed so we can update the list */
      view->controller_op_closure =
        rig_controller_add_operation_callback (controller,
                                               controller_operation_cb,
                                               view,
                                               NULL /* destroy */);

      controller_len_prop =
        rut_introspectable_lookup_property (controller, "length");
      rut_property_set_copy_binding (&view->engine->ctx->property_ctx,
                                     scale_len_prop,
                                     controller_len_prop);

      scale_focus_prop =
        rut_introspectable_lookup_property (view->scale, "focus");
      rut_property_connect_callback (scale_focus_prop,
                                     on_scale_focus_change_cb,
                                     view);
    }

  _rig_engine_update_inspector (view->engine);
}

static void
controller_select_cb (RutProperty *value_property,
                      void *user_data)
{
  RigControllerView *view = user_data;
  RigEngine *engine = view->engine;
  int value = rut_property_get_integer (value_property);
  RigController *controller = g_list_nth_data (engine->controllers, value);
  rig_controller_view_set_controller (view, controller);
}

static void
on_controller_add_button_click_cb (RutIconButton *button, void *user_data)
{
  RigControllerView *view = user_data;
  RigEngine *engine = view->engine;
  RigController *controller;
  char *name;
  int i;

  for (i = 0; true; i++)
    {
      bool clash = false;
      GList *l;

      name = g_strdup_printf ("Controller %i", i);

      for (l = engine->controllers; l; l = l->next)
        {
          controller = l->data;
          if (strcmp (controller->label, name) == 0)
            {
              clash = true;
              break;
            }
        }

      if (!clash)
        break;
    }

  controller = rig_controller_new (engine, name);

  rig_undo_journal_log_add_controller (engine->undo_journal, controller);

  rut_object_unref (controller);

  rig_controller_view_set_controller (view, controller);
}

static void
on_controller_delete_button_click_cb (RutIconButton *button, void *user_data)
{
  RigControllerView *view = user_data;
  RigEngine *engine = view->engine;

  rig_undo_journal_log_remove_controller (engine->undo_journal,
                                          view->controller);
}

typedef struct _ScaleSelectState
{
  RigControllerView *view;
  float start_t;
  float end_t;
} ScaleSelectState;

static void
scale_select_nodes_cb (RigPathView *path_view,
                       RigNode *node,
                       void *user_data)
{
  ScaleSelectState *state = user_data;

  if (node->t >= state->start_t && node->t <= state->end_t)
    {
      RigControllerView *view = state->view;
      RigNodeMarker *marker = rig_path_view_find_node_marker (path_view, node);

      _rig_controller_view_select_marker (view, marker, RUT_SELECT_ACTION_TOGGLE);
    }
}

static void
on_scale_select_cb (RutScale *scale,
                    float start_t,
                    float end_t,
                    void *user_data)
{
  RigControllerView *view = user_data;
  float length = rig_controller_get_length (view->controller);
  ScaleSelectState state;

  _rig_nodes_selection_cancel (view->nodes_selection);

  state.view = view;
  state.start_t = start_t / length;
  state.end_t = end_t / length;

  _rig_controller_view_foreach_node (view,
                                     scale_select_nodes_cb,
                                     &state);
}

RigControllerView *
rig_controller_view_new (RigEngine *engine,
                         RigUndoJournal *undo_journal)
{
  RigControllerView *view = rut_object_alloc0 (RigControllerView,
                                               &rig_controller_view_type,
                                               _rig_controller_view_init_type);
  RutStack *top_stack;
  RutStack *stack;
  RutBoxLayout *selector_hbox;
  RutIconButton *add_button;
  RutIconButton *delete_button;
  RutDropDown *controller_selector;
  RutProperty *value_prop;
  RutRectangle *bg;
  RutText *label;

  rut_graphable_init (view);


  view->engine = engine;
  view->context = engine->ctx;
  view->controller = NULL;
  view->undo_journal = undo_journal;

  rut_list_init (&view->controller_changed_cb_list);

  view->vbox = rut_box_layout_new (engine->ctx,
                                   RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM);
  rut_graphable_add_child (view, view->vbox);
  rut_object_unref (view->vbox);

  top_stack = rut_stack_new (engine->ctx, 0, 0);
  rut_box_layout_add (view->vbox, FALSE, top_stack);
  rut_object_unref (top_stack);

  bg = rut_rectangle_new4f (engine->ctx, 0, 0, 0.65, 0.65, 0.65, 1);
  rut_stack_add (top_stack, bg);
  rut_object_unref (bg);

  selector_hbox = rut_box_layout_new (engine->ctx,
                                      RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);
  rut_stack_add (top_stack, selector_hbox);
  rut_object_unref (selector_hbox);

  controller_selector = rut_drop_down_new (engine->ctx);
  view->controller_selector = controller_selector;
  value_prop = rut_introspectable_lookup_property (controller_selector, "value");
  rut_property_connect_callback (value_prop,
                                 controller_select_cb,
                                 view);
  rut_box_layout_add (selector_hbox, false, controller_selector);
  rut_object_unref (controller_selector);

  add_button = rut_icon_button_new (engine->ctx,
                                    NULL, /* label */
                                    0, /* ignore label position */
                                    "add.png", /* normal */
                                    "add.png", /* hover */
                                    "add-white.png", /* active */
                                    "add.png"); /* disabled */
  rut_box_layout_add (selector_hbox, false, add_button);
  rut_object_unref (add_button);
  rut_icon_button_add_on_click_callback (add_button,
                                         on_controller_add_button_click_cb,
                                         view,
                                         NULL); /* destroy notify */

  delete_button = rut_icon_button_new (engine->ctx,
                                       NULL, /* label */
                                       0, /* ignore label position */
                                       "delete.png", /* normal */
                                       "delete.png", /* hover */
                                       "delete-white.png", /* active */
                                       "delete.png"); /* disabled */
  rut_box_layout_add (selector_hbox, false, delete_button);
  rut_object_unref (delete_button);
  rut_icon_button_add_on_click_callback (delete_button,
                                         on_controller_delete_button_click_cb,
                                         view,
                                         NULL); /* destroy notify */


  view->header_hbox = rut_box_layout_new (engine->ctx,
                                          RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);
  rut_box_layout_add (view->vbox, FALSE, view->header_hbox);
  rut_object_unref (view->header_hbox);

  view->properties_label_shim = rut_shim_new (engine->ctx, 1, 1);
  rut_shim_set_shim_axis (view->properties_label_shim, RUT_SHIM_AXIS_X);
  rut_box_layout_add (view->header_hbox, FALSE, view->properties_label_shim);
  rut_object_unref (view->properties_label_shim);

  label = rut_text_new_with_text (engine->ctx, NULL, "Properties");
  rut_shim_set_child (view->properties_label_shim, label);
  rut_object_unref (label);

  view->scale = rut_scale_new (engine->ctx, 0, 10);
  rut_box_layout_add (view->header_hbox, TRUE, view->scale);

  rut_scale_add_select_callback (view->scale,
                                 on_scale_select_cb,
                                 view,
                                 NULL); /* destroy notify */

  stack = rut_stack_new (engine->ctx, 0, 0);
  rut_box_layout_add (view->vbox, TRUE, stack);
  rut_object_unref (stack);

  bg = rut_rectangle_new4f (engine->ctx, 0, 0, 0.52, 0.52, 0.52, 1);
  rut_stack_add (stack, bg);
  rut_object_unref (bg);

  view->properties_vp = rut_ui_viewport_new (engine->ctx, 0, 0);
  rut_ui_viewport_set_x_pannable (view->properties_vp, FALSE);
  rut_stack_add (stack, view->properties_vp);
  rut_object_unref (view->properties_vp);

  view->properties_vbox =
    rut_box_layout_new (view->context,
                        RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM);

  rut_ui_viewport_add (view->properties_vp, view->properties_vbox);
  rut_ui_viewport_set_sync_widget (view->properties_vp,
                                   view->properties_vbox);
  rut_object_unref (view->properties_vbox);


  rig_controller_view_create_separator_pipeline (view);

  view->nodes_selection = _rig_nodes_selection_new (view);

  return view;
}

void
rig_controller_view_update_controller_list (RigControllerView *view)
{
  RigEngine *engine = view->engine;
  int n_controllers;
  RutDropDownValue *controller_values;
  GList *l;
  int i;

  n_controllers = g_list_length (engine->controllers);
  controller_values = g_malloc (sizeof (RutDropDownValue) * n_controllers);

  for (l = engine->controllers, i = 0; l; l = l->next, i++)
    {
      RigController *controller = l->data;
      controller_values[i].name = controller->label;
      controller_values[i].value = i;
    }

  rut_drop_down_set_values_array (view->controller_selector,
                                  controller_values, n_controllers);

  g_free (controller_values);
}

static void
_rig_controller_view_foreach_node (RigControllerView *view,
                                   RigControllerViewNodeCallback callback,
                                   void *user_data)
{
  GList *l, *l2;

  for (l = view->object_views; l; l = l->next)
    {
      RigControllerObjectView *object_view = l->data;

      for (l2 = object_view->properties; l2; l2 = l2->next)
        {
          RigControllerPropertyView *prop_view = l2->data;

          if (prop_view->prop_data->method == RIG_CONTROLLER_METHOD_PATH)
            {
              RigPathView *path_view = prop_view->columns[2].control;
              RigNode *node;

              g_assert (rut_object_get_type (path_view) == &rig_path_view_type);

              rut_list_for_each (node, &path_view->path->nodes, list_node)
                callback (path_view, node, user_data);
            }
        }
    }
}

double
rig_controller_view_get_focus (RigControllerView *view)
{
  return rut_scale_get_focus (view->scale);
}

void
rig_controller_view_edit_property (RigControllerView *view,
                                   bool mergable,
                                   RutProperty *property,
                                   RutBoxed *boxed_value)
{
  RigEngine *engine = view->engine;
  RigControllerPropData *prop_data =
    rig_controller_find_prop_data_for_property (view->controller,
                                                property);

  if (prop_data)
    {
      switch (prop_data->method)
        {
        case RIG_CONTROLLER_METHOD_CONSTANT:
          rig_undo_journal_set_controller_constant (engine->undo_journal,
                                                    mergable,
                                                    view->controller,
                                                    boxed_value,
                                                    property);
          break;
        case RIG_CONTROLLER_METHOD_PATH:
          {
            float focus_offset =
              rig_controller_view_get_focus (engine->controller_view);

            rig_undo_journal_set_controller_path_node_value (engine->undo_journal,
                                                             mergable,
                                                             view->controller,
                                                             focus_offset,
                                                             boxed_value,
                                                             property);

            /* It's possible that this change also has the side effect
             * of changing the length of the controller and so we
             * re-set the scale focus offset as the controller's elapsed
             * time so the new value will be asserted by the
             * controller.
             */
            if (!rig_controller_get_running (view->controller))
              {
                rig_controller_set_elapsed (view->controller,
                                            rut_scale_get_focus (view->scale));
              }

          break;
          }
        case RIG_CONTROLLER_METHOD_BINDING:
          g_warning ("Ignoring property change while controlled by binding");
          break;
        }
    }
  else
    {
      rig_undo_journal_set_property (engine->undo_journal,
                                     mergable,
                                     boxed_value,
                                     property);
    }
}


