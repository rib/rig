/*
 * Rut
 *
 * Copyright (C) 2012,2013  Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#include <rut.h>

#include "rig-camera-view.h"
#include "rig-selection-tool.h"

typedef struct _EntityState EntityState;

typedef struct _ControlPoint
{
  EntityState *entity_state;
  float x, y, z;

  RutTransform *transform;
  RutNineSlice *marker;
  RutInputRegion *input_region;

  float position[3];    /* transformed position of the selected entity */
  float screen_pos[2];

} ControlPoint;

struct _EntityState
{
  RigSelectionTool *tool;
  RutEntity *entity;

  GList *control_points;
};

typedef struct _GrabState
{
  RigSelectionTool *tool;
  EntityState *entity_state;
  ControlPoint *point;
} GrabState;

static RutInputEventStatus
control_point_grab_cb (RutInputEvent *event,
                       void *user_data)
{
  GrabState *state = user_data;
  RigSelectionTool *tool = state->tool;
  //EntityState *entity_state = state->entity_state;
  //ControlPoint *point = state->point;
  RutInputEventStatus status = RUT_INPUT_EVENT_STATUS_UNHANDLED;
  RutMotionEventAction action;

  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_KEY &&
      rut_key_event_get_keysym (event) == RUT_KEY_Escape)
    {
      rut_shell_ungrab_input (tool->view->context->shell,
                              control_point_grab_cb,
                              state);

      g_slice_free (GrabState, state);

      return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

  if (rut_input_event_get_type (event) != RUT_INPUT_EVENT_TYPE_MOTION)
    return RUT_INPUT_EVENT_STATUS_UNHANDLED;

  action = rut_motion_event_get_action (event);

  switch (action)
    {
    case RUT_MOTION_EVENT_ACTION_MOVE:
    case RUT_MOTION_EVENT_ACTION_UP:
      {
        //float x = rut_motion_event_get_x (event);
        //float y = rut_motion_event_get_y (event);

        if (action == RUT_MOTION_EVENT_ACTION_UP)
          {
            if ((rut_motion_event_get_button_state (event) &
                 RUT_BUTTON_STATE_1) == 0)
              {
                status = RUT_INPUT_EVENT_STATUS_HANDLED;

                rut_shell_ungrab_input (tool->ctx->shell,
                                        control_point_grab_cb,
                                        state);

                g_slice_free (GrabState, state);
              }
          }
        else
          status = RUT_INPUT_EVENT_STATUS_HANDLED;
      }
      break;

    default:
      break;
    }

  return status;
}

static RutInputEventStatus
control_point_input_cb (RutInputRegion *region,
                        RutInputEvent *event,
                        void *user_data)
{
  ControlPoint *point = user_data;
  EntityState *entity_state = point->entity_state;
  RigSelectionTool *tool = entity_state->tool;

  g_return_if_fail (tool->selected_entities != NULL);

  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION &&
      rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_DOWN &&
      rut_motion_event_get_button_state (event) == RUT_BUTTON_STATE_1)
    {
      //float x = rut_motion_event_get_x (event);
      //float y = rut_motion_event_get_y (event);
      GrabState *state = g_slice_new0 (GrabState);

      state->tool = tool;
      state->entity_state = entity_state;
      state->point = point;

      rut_shell_grab_input (tool->ctx->shell,
                            rut_input_event_get_camera (event),
                            control_point_grab_cb,
                            state);

      return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
create_dummy_control_points (EntityState *entity_state)
{
  RigSelectionTool *tool = entity_state->tool;
  CoglTexture *tex = rut_load_texture_from_data_file (tool->ctx, "dot.png", NULL);
  ControlPoint *point;

  point = g_slice_new0 (ControlPoint);
  point->entity_state = entity_state;
  point->x = 0;
  point->y = 0;
  point->z = 0;

  point->transform = rut_transform_new (tool->ctx);
  rut_graphable_add_child (tool->tool_overlay, point->transform);
  rut_refable_unref (point->transform);

  point->marker = rut_nine_slice_new (tool->ctx, tex, 0, 0, 0, 0, 10, 10);
  rut_graphable_add_child (point->transform, point->marker);
  rut_refable_unref (point->marker);

  point->input_region =
    rut_input_region_new_circle (0, 0, 5,
                                 control_point_input_cb,
                                 point);
  rut_graphable_add_child (tool->tool_overlay, point->input_region);
  rut_refable_unref (point->input_region);
  entity_state->control_points =
    g_list_prepend (entity_state->control_points, point);


  point = g_slice_new0 (ControlPoint);
  point->entity_state = entity_state;
  point->x = 100;
  point->y = 0;
  point->z = 0;

  point->transform = rut_transform_new (tool->ctx);
  rut_graphable_add_child (tool->tool_overlay, point->transform);
  rut_refable_unref (point->transform);

  point->marker = rut_nine_slice_new (tool->ctx, tex, 0, 0, 0, 0, 10, 10);
  rut_graphable_add_child (point->transform, point->marker);
  rut_refable_unref (point->marker);

  point->input_region =
    rut_input_region_new_circle (0, 0, 5,
                                 control_point_input_cb,
                                 point);
  rut_graphable_add_child (tool->tool_overlay, point->input_region);
  rut_refable_unref (point->input_region);
  entity_state->control_points =
    g_list_prepend (entity_state->control_points, point);

  cogl_object_unref (tex);
}

static void
entity_state_destroy (EntityState *entity_state)
{
  GList *l;

  for (l = entity_state->control_points; l; l = l->next)
    {
      ControlPoint *point = l->data;
      rut_graphable_remove_child (point->input_region);
      rut_graphable_remove_child (point->transform);
    }

  rut_refable_unref (entity_state->entity);

  g_slice_free (EntityState, entity_state);
}

static void
objects_selection_event_cb (RigObjectsSelection *selection,
                            RigObjectsSelectionEvent event,
                            RutObject *object,
                            void *user_data)
{
  RigSelectionTool *tool = user_data;
  EntityState *entity_state;
  GList *l;

  if (!tool->active && event == RIG_OBJECTS_SELECTION_ADD_EVENT)
    return;

  if (rut_object_get_type (object) != &rut_entity_type)
    return;

  for (l = tool->selected_entities; l; l = l->next)
    {
      entity_state = l->data;
      if (entity_state->entity == object)
        break;
    }
  if (l == NULL)
    entity_state = NULL;

  switch (event)
    {
    case RIG_OBJECTS_SELECTION_ADD_EVENT:
      {
        g_return_if_fail (entity_state == NULL);

        entity_state = g_slice_new0 (EntityState);
        entity_state->tool = tool;
        entity_state->entity = rut_refable_ref (object);
        entity_state->control_points = NULL;

        tool->selected_entities = g_list_prepend (tool->selected_entities,
                                                  entity_state);

#warning "TODO: create meaningful control points!"
        create_dummy_control_points (entity_state);

        break;
      }

    case RIG_OBJECTS_SELECTION_REMOVE_EVENT:
      g_return_if_fail (entity_state != NULL);

      tool->selected_entities = g_list_remove (tool->selected_entities, entity_state);
      entity_state_destroy (entity_state);
      break;
    }
}

RigSelectionTool *
rig_selection_tool_new (RigCameraView *view,
                        RutObject *overlay)
{
  RigSelectionTool *tool = g_slice_new0 (RigSelectionTool);
  RutContext *ctx = view->context;

  tool->view = view;
  tool->ctx = ctx;

  /* Note: we don't take a reference on this overlay to avoid creating
   * a circular reference. */
  tool->tool_overlay = overlay;

  tool->camera = view->view_camera;
  tool->camera_component =
    rut_entity_get_component (tool->camera, RUT_COMPONENT_TYPE_CAMERA);

  rut_list_init (&tool->selection_event_cb_list);

  /* pipeline to draw the tool */
  tool->default_pipeline = cogl_pipeline_new (rut_cogl_context);

  return tool;
}

void
rig_selection_tool_set_active (RigSelectionTool *tool,
                               bool active)
{
  RigObjectsSelection *selection = tool->view->engine->objects_selection;
  GList *l;

  if (tool->active == active)
    return;

  tool->active = active;

  if (active)
    {
      tool->objects_selection_closure =
        rig_objects_selection_add_event_callback (selection,
                                                  objects_selection_event_cb,
                                                  tool,
                                                  NULL); /* destroy notify */

      for (l = selection->objects; l; l = l->next)
        {
          objects_selection_event_cb (selection,
                                      RIG_OBJECTS_SELECTION_ADD_EVENT,
                                      l->data,
                                      tool);
        }
    }
  else
    {
      for (l = selection->objects; l; l = l->next)
        {
          objects_selection_event_cb (selection,
                                      RIG_OBJECTS_SELECTION_REMOVE_EVENT,
                                      l->data,
                                      tool);
        }

      rut_closure_disconnect (tool->objects_selection_closure);
      tool->objects_selection_closure = NULL;
    }
}

static void
get_modelview_matrix (RutEntity  *camera,
                      RutEntity  *entity,
                      CoglMatrix *modelview)
{
  RutCamera *camera_component =
    rut_entity_get_component (camera, RUT_COMPONENT_TYPE_CAMERA);
  *modelview = *rut_camera_get_view_transform (camera_component);

  cogl_matrix_multiply (modelview,
                        modelview,
                        rut_entity_get_transform (entity));
}

bool
map_window_coords_to_overlay_coord (RutCamera *camera, /* 2d ui camera */
                                    RutObject *overlay, /* camera-view overlay */
                                    float *x,
                                    float *y)
{
  CoglMatrix transform;
  CoglMatrix inverse_transform;

  rut_graphable_get_modelview (overlay, camera, &transform);

  if (!cogl_matrix_get_inverse (&transform, &inverse_transform))
    return FALSE;

  rut_camera_unproject_coord (camera,
                              &transform,
                              &inverse_transform,
                              0, /* object_coord_z */
                              x,
                              y);

  return TRUE;
}

/* Scale from OpenGL normalized device coordinates (ranging from -1 to 1)
 * to Cogl window/framebuffer coordinates (ranging from 0 to buffer-size) with
 * (0,0) being top left. */
#define VIEWPORT_TRANSFORM_X(x, vp_origin_x, vp_width) \
    (  ( ((x) + 1.0) * ((vp_width) / 2.0) ) + (vp_origin_x)  )
/* Note: for Y we first flip all coordinates around the X axis while in
 * normalized device coodinates */
#define VIEWPORT_TRANSFORM_Y(y, vp_origin_y, vp_height) \
    (  ( ((-(y)) + 1.0) * ((vp_height) / 2.0) ) + (vp_origin_y)  )

void
update_control_point_positions (RigSelectionTool *tool,
                                RutCamera *paint_camera) /* 2d ui camera */
{
  RutCamera *camera = tool->camera_component;
  GList *l;

  for (l = tool->selected_entities; l; l = l->next)
    {
      EntityState *entity_state = l->data;
      CoglMatrix transform;
      const CoglMatrix *projection;
      float screen_space[4], x, y;
      const float *viewport;
      GList *l2;

      get_modelview_matrix (tool->camera,
                            entity_state->entity,
                            &transform);

      projection = rut_camera_get_projection (camera);

      viewport = rut_camera_get_viewport (camera);

      for (l2 = entity_state->control_points; l2; l2 = l2->next)
        {
          ControlPoint *point = l2->data;

          point->position[0] = point->x;
          point->position[1] = point->y;
          point->position[2] = point->z;

          cogl_matrix_transform_points (&transform,
                                        3, /* num components for input */
                                        sizeof (float) * 3, /* input stride */
                                        point->position,
                                        sizeof (float) * 3, /* output stride */
                                        point->position,
                                        1 /* n_points */);


          /* update the input region, need project the transformed point and do
           * the viewport transform */
          screen_space[0] = point->position[0];
          screen_space[1] = point->position[1];
          screen_space[2] = point->position[2];
          cogl_matrix_project_points (projection,
                                      3, /* num components for input */
                                      sizeof (float) * 3, /* input stride */
                                      screen_space,
                                      sizeof (float) * 4, /* output stride */
                                      screen_space,
                                      1 /* n_points */);

          /* perspective divide */
          screen_space[0] /= screen_space[3];
          screen_space[1] /= screen_space[3];

          /* apply viewport transform */
          x = VIEWPORT_TRANSFORM_X (screen_space[0], viewport[0], viewport[2]);
          y = VIEWPORT_TRANSFORM_Y (screen_space[1], viewport[1], viewport[3]);

          point->screen_pos[0] = x;
          point->screen_pos[1] = y;

          map_window_coords_to_overlay_coord (paint_camera,
                                              tool->tool_overlay, &x, &y);

          rut_transform_init_identity (point->transform);
          rut_transform_translate (point->transform, x, y, 0);
          rut_input_region_set_circle (point->input_region, x, y, 10);
        }
    }
}

void
rig_selection_tool_update (RigSelectionTool *tool,
                           RutCamera *paint_camera)
{
  g_return_if_fail (tool->active);

  if (!tool->selected_entities)
    return;

  update_control_point_positions (tool, paint_camera);
}

RutClosure *
rig_selection_tool_add_event_callback (RigSelectionTool *tool,
                                       RigSelectionToolEventCallback callback,
                                       void *user_data,
                                       RutClosureDestroyCallback destroy_cb)
{
  return rut_closure_list_add (&tool->selection_event_cb_list,
                               callback,
                               user_data,
                               destroy_cb);
}

void
rig_selection_tool_destroy (RigSelectionTool *tool)
{
  GList *l;

  rut_closure_list_disconnect_all (&tool->selection_event_cb_list);

  cogl_object_unref (tool->default_pipeline);

  for (l = tool->selected_entities; l; l = l->next)
    entity_state_destroy (l->data);
  g_list_free (tool->selected_entities);

  g_slice_free (RigSelectionTool, tool);
}
