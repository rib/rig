/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012  Intel Corporation
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

#include <rut.h>

#include "rig-camera-view.h"
#include "rig-rotation-tool.h"

static RutInputEventStatus
rotation_tool_grab_cb (RutInputEvent *event,
                       void *user_data)
{
  RutInputEventStatus status = RUT_INPUT_EVENT_STATUS_UNHANDLED;
  RigRotationTool *tool = user_data;
  RutMotionEventAction action;

  g_warn_if_fail (tool->button_down);

  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_KEY &&
      rut_key_event_get_keysym (event) == RUT_KEY_Escape)
    {
      tool->button_down = FALSE;

      rut_shell_ungrab_input (tool->view->context->shell,
                              rotation_tool_grab_cb,
                              tool);

      rut_closure_list_invoke (&tool->rotation_event_cb_list,
                               RigRotationToolEventCallback,
                               tool,
                               RIG_ROTATION_TOOL_CANCEL,
                               &tool->start_rotation,
                               &tool->start_rotation);

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
        CoglQuaternion camera_rotation;
        CoglQuaternion new_rotation;
        RigEntity *parent;
        CoglQuaternion parent_inverse;
        RigEntity *entity = tool->selected_entity;
        float x = rut_motion_event_get_x (event);
        float y = rut_motion_event_get_y (event);
        RigRotationToolEventType event_type = RIG_ROTATION_TOOL_DRAG;

        rut_arcball_mouse_motion (&tool->arcball, x, y);

        cogl_quaternion_multiply (&camera_rotation,
                                  &tool->arcball.q_drag,
                                  &tool->start_view_rotations);

        /* XXX: We have calculated the combined rotation in camera
         * space, we now need to separate out the rotation of the
         * entity itself.
         *
         * We rotate by the inverse of the parent's view transform
         * so we are left with just the entity's rotation.
         */
        parent = rut_graphable_get_parent (entity);

        rig_entity_get_view_rotations (parent,
                                       tool->camera,
                                       &parent_inverse);
        cogl_quaternion_invert (&parent_inverse);

        cogl_quaternion_multiply (&new_rotation,
                                  &parent_inverse,
                                  &camera_rotation);

        if (action == RUT_MOTION_EVENT_ACTION_UP)
          {
            if ((rut_motion_event_get_button_state (event) &
                 RUT_BUTTON_STATE_1) == 0)
              {
                status = RUT_INPUT_EVENT_STATUS_HANDLED;
                event_type = RIG_ROTATION_TOOL_RELEASE;

                tool->button_down = FALSE;

                rut_shell_ungrab_input (tool->ctx->shell,
                                        rotation_tool_grab_cb,
                                        tool);
              }
          }
        else
          status = RUT_INPUT_EVENT_STATUS_HANDLED;

        rut_closure_list_invoke (&tool->rotation_event_cb_list,
                                 RigRotationToolEventCallback,
                                 tool,
                                 event_type,
                                 &tool->start_rotation,
                                 &new_rotation);
      }
      break;

    default:
      break;
    }

  return status;
}

static RutInputEventStatus
on_rotation_tool_clicked (RutInputRegion *region,
                          RutInputEvent *event,
                          void *user_data)
{
  RutInputEventStatus status = RUT_INPUT_EVENT_STATUS_UNHANDLED;
  RigRotationTool *tool = user_data;

  g_return_if_fail (tool->selected_entity != NULL);

  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION &&
      rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_DOWN &&
      rut_motion_event_get_button_state (event) == RUT_BUTTON_STATE_1)
    {
      RigEntity *entity = tool->selected_entity;
      float x = rut_motion_event_get_x (event);
      float y = rut_motion_event_get_y (event);

      rut_shell_grab_input (tool->ctx->shell,
                            rut_input_event_get_camera (event),
                            rotation_tool_grab_cb,
                            tool);

      rut_arcball_init (&tool->arcball,
                        tool->screen_pos[0],
                        tool->screen_pos[1],
                        128);

      rig_entity_get_view_rotations (entity,
                                     tool->camera,
                                     &tool->start_view_rotations);

      tool->start_rotation = *rig_entity_get_rotation (entity);

      cogl_quaternion_init_identity (&tool->arcball.q_drag);

      rut_arcball_mouse_down (&tool->arcball, x, y);

      tool->button_down = TRUE;

      status = RUT_INPUT_EVENT_STATUS_HANDLED;
    }

  return status;
}

static void
update_selection_state (RigRotationTool *tool)
{
  RigObjectsSelection *selection = tool->view->engine->objects_selection;
  RutObject *camera = tool->camera_component;

  if (tool->active &&
      g_list_length (selection->objects) == 1 &&
      rut_object_get_type (selection->objects->data) == &rig_entity_type)
    {
      if (!tool->selected_entity)
        rut_camera_add_input_region (camera, tool->rotation_circle);

      tool->selected_entity = selection->objects->data;
    }
  else
    {
      if (tool->selected_entity)
        rut_camera_remove_input_region (camera, tool->rotation_circle);

      tool->selected_entity = NULL;
    }
}

static void
objects_selection_event_cb (RigObjectsSelection *selection,
                            RigObjectsSelectionEvent event,
                            RutObject *object,
                            void *user_data)
{
  RigRotationTool *tool = user_data;

  if (event != RIG_OBJECTS_SELECTION_ADD_EVENT &&
      event != RIG_OBJECTS_SELECTION_REMOVE_EVENT)
    return;

  update_selection_state (tool);
}

static void
tool_event_cb (RigRotationTool *tool,
               RigRotationToolEventType type,
               const CoglQuaternion *start_rotation,
               const CoglQuaternion *new_rotation,
               void *user_data)
{
  RigEngine *engine = tool->view->engine;
  RigEntity *entity;

  g_return_if_fail (engine->objects_selection->objects);

  /* XXX: For now we don't do anything clever to handle rotating a
   * set of entityies, since it's ambiguous what origin should be used
   * in this case. In the future the rotation capabilities need to be
   * more capable though and we may intoduce the idea of a 3D cursor
   * for example to define the origin for the set. */
  entity = engine->objects_selection->objects->data;

  switch (type)
    {
    case RIG_ROTATION_TOOL_DRAG:
      rig_entity_set_rotation (entity, new_rotation);
      rut_shell_queue_redraw (engine->shell);
      break;

    case RIG_ROTATION_TOOL_RELEASE:
      {
        RutProperty *rotation_prop =
          rut_introspectable_lookup_property (entity, "rotation");
        RutBoxed value;

        /* Revert the rotation before logging the new rotation into
         * the journal... */
        rig_entity_set_rotation (entity, start_rotation);

        value.type = RUT_PROPERTY_TYPE_QUATERNION;
        value.d.quaternion_val = *new_rotation;

        rig_controller_view_edit_property (engine->controller_view,
                                           FALSE, /* mergable */
                                           rotation_prop, &value);
      }
      break;

    case RIG_ROTATION_TOOL_CANCEL:
      rig_entity_set_rotation (entity, start_rotation);
      rut_shell_queue_redraw (engine->shell);
      break;
    }
}

RigRotationTool *
rig_rotation_tool_new (RigCameraView *view)
{
  RigRotationTool *tool = g_slice_new0 (RigRotationTool);
  RutContext *ctx = view->context;

  tool->view = view;
  tool->ctx = ctx;

  tool->camera = view->view_camera;
  tool->camera_component =
    rig_entity_get_component (tool->camera, RUT_COMPONENT_TYPE_CAMERA);

  rut_list_init (&tool->rotation_event_cb_list);

  /* pipeline to draw the tool */
  tool->default_pipeline = cogl_pipeline_new (ctx->cogl_context);

  /* rotation tool */
  tool->rotation_tool = rut_create_rotation_tool_primitive (ctx, 64);

  /* rotation tool handle circle */
  tool->rotation_tool_handle =
    rut_create_circle_outline_primitive (ctx, 64);

  tool->rotation_circle =
    rut_input_region_new_circle (0, 0, 0, on_rotation_tool_clicked, tool);
  rut_input_region_set_hud_mode (tool->rotation_circle, TRUE);

  rig_rotation_tool_add_event_callback (tool,
                                        tool_event_cb,
                                        NULL, /* user data */
                                        NULL /* destroy_cb */);

  return tool;
}

void
rig_rotation_tool_set_active (RigRotationTool *tool,
                              bool active)
{
  if (tool->active == active)
    return;

  tool->active = active;

  if (active)
    {
      RigObjectsSelection *selection = tool->view->engine->objects_selection;

      tool->objects_selection_closure =
        rig_objects_selection_add_event_callback (selection,
                                                  objects_selection_event_cb,
                                                  tool,
                                                  NULL); /* destroy notify */
    }
  else
    {
      rut_closure_disconnect (tool->objects_selection_closure);
      tool->objects_selection_closure = NULL;
    }

  update_selection_state (tool);
}

static void
get_modelview_matrix (RigEntity  *camera,
                      RigEntity  *entity,
                      CoglMatrix *modelview)
{
  RutObject *camera_component =
    rig_entity_get_component (camera, RUT_COMPONENT_TYPE_CAMERA);
  *modelview = *rut_camera_get_view_transform (camera_component);

  cogl_matrix_multiply (modelview,
                        modelview,
                        rig_entity_get_transform (entity));
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
update_position (RigRotationTool *tool)
{
  RutObject *camera = tool->camera_component;
  CoglMatrix transform;
  const CoglMatrix *projection;
  float scale_thingy[4], screen_space[4], x, y;
  const float *viewport;

  /* transform the selected entity up to the projection */
  get_modelview_matrix (tool->camera,
                        tool->selected_entity,
                        &transform);

  tool->position[0] = tool->position[1] = tool->position[2] = 0.f;

  cogl_matrix_transform_points (&transform,
                                3, /* num components for input */
                                sizeof (float) * 3, /* input stride */
                                tool->position,
                                sizeof (float) * 3, /* output stride */
                                tool->position,
                                1 /* n_points */);

  projection = rut_camera_get_projection (camera);

  scale_thingy[0] = 1.f;
  scale_thingy[1] = 0.f;
  scale_thingy[2] = tool->position[2];

  cogl_matrix_project_points (projection,
                              3, /* num components for input */
                              sizeof (float) * 3, /* input stride */
                              scale_thingy,
                              sizeof (float) * 4, /* output stride */
                              scale_thingy,
                              1 /* n_points */);
  scale_thingy[0] /= scale_thingy[3];

  tool->scale = 1. / scale_thingy[0];

  /* update the input region, need project the transformed point and do
   * the viewport transform */
  screen_space[0] = tool->position[0];
  screen_space[1] = tool->position[1];
  screen_space[2] = tool->position[2];
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
  viewport = rut_camera_get_viewport (camera);
  x = VIEWPORT_TRANSFORM_X (screen_space[0], viewport[0], viewport[2]);
  y = VIEWPORT_TRANSFORM_Y (screen_space[1], viewport[1], viewport[3]);

  tool->screen_pos[0] = x;
  tool->screen_pos[1] = y;

  rut_input_region_set_circle (tool->rotation_circle, x, y, 64);
}

static float
get_scale_for_length (RigRotationTool *tool, float length)
{
  return length * tool->scale;
}

static void
get_rotation (RigEntity *camera,
              RigEntity *entity,
              CoglMatrix *rotation)
{
  CoglQuaternion q;

  rig_entity_get_view_rotations (entity, camera, &q);
  cogl_matrix_init_from_quaternion (rotation, &q);
}

void
rig_rotation_tool_draw (RigRotationTool *tool,
                        CoglFramebuffer *fb)
{
  CoglMatrix rotation;
  float scale, aspect_ratio;
  CoglMatrix saved_projection;
  CoglMatrix projection;
  float fov;
  float near;
  float zoom;
  float vp_width, vp_height;

  g_return_if_fail (tool->active);

  if (!tool->selected_entity)
    return;

  update_position (tool);

  get_rotation (tool->camera,
                tool->selected_entity,
                &rotation);

  /* we change the projection matrix to clip at -position[2] to clip the
   * half sphere that is away from the camera */
  vp_width = cogl_framebuffer_get_viewport_width (fb);
  vp_height = cogl_framebuffer_get_viewport_height (fb);
  aspect_ratio = vp_width / vp_height;

  cogl_framebuffer_get_projection_matrix (fb, &saved_projection);

  cogl_matrix_init_identity (&projection);
  fov = rut_camera_get_field_of_view (tool->camera_component);
  near = rut_camera_get_near_plane (tool->camera_component);
  zoom = rut_camera_get_zoom (tool->camera_component);
  rut_util_matrix_scaled_perspective (&projection,
                                      fov,
                                      aspect_ratio,
                                      near,
                                      -tool->position[2], /* far */
                                      zoom);
  cogl_framebuffer_set_projection_matrix (fb, &projection);

  scale = get_scale_for_length (tool, 128 / vp_width);

  /* draw the tool */
  cogl_framebuffer_push_matrix (fb);
  cogl_framebuffer_identity_matrix (fb);
  cogl_framebuffer_translate (fb,
                              tool->position[0],
                              tool->position[1],
                              tool->position[2]);

  /* XXX: We flip the y axis here since the get_rotation() call
   * doesn't take into account that editor/main.c does a view
   * transform with the camera outside of the entity system
   * which flips the y axis.
   *
   * Note: this means the examples won't look right for now.
   */
  cogl_framebuffer_scale (fb, scale, -scale, scale);
  cogl_framebuffer_push_matrix (fb);
  cogl_framebuffer_transform (fb, &rotation);
  cogl_primitive_draw (tool->rotation_tool,
                       fb,
                       tool->default_pipeline);
  cogl_framebuffer_pop_matrix (fb);
  cogl_primitive_draw (tool->rotation_tool_handle,
                       fb,
                       tool->default_pipeline);
  cogl_framebuffer_scale (fb, 1.1, 1.1, 1.1);
  cogl_primitive_draw (tool->rotation_tool_handle,
                       fb,
                       tool->default_pipeline);
  cogl_framebuffer_pop_matrix (fb);

  cogl_framebuffer_set_projection_matrix (fb, &saved_projection);
}

RutClosure *
rig_rotation_tool_add_event_callback (RigRotationTool *tool,
                                      RigRotationToolEventCallback callback,
                                      void *user_data,
                                      RutClosureDestroyCallback destroy_cb)
{
  return rut_closure_list_add (&tool->rotation_event_cb_list,
                               callback,
                               user_data,
                               destroy_cb);
}

void
rig_rotation_tool_destroy (RigRotationTool *tool)
{
  rut_closure_list_disconnect_all (&tool->rotation_event_cb_list);

  cogl_object_unref (tool->default_pipeline);
  cogl_object_unref (tool->rotation_tool);
  cogl_object_unref (tool->rotation_tool_handle);
  rut_object_unref (tool->rotation_circle);

  if (tool->button_down)
    rut_shell_ungrab_input (tool->ctx->shell,
                            rotation_tool_grab_cb,
                            tool);

  g_slice_free (RigRotationTool, tool);
}
