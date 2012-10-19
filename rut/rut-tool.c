/*
 * Rut
 *
 * Copyright (C) 2012  Intel Corporation
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "rut-global.h"
#include "rut-types.h"
#include "rut-geometry.h"

#include "components/rut-camera.h"

#include "rut-tool.h"

static RutInputEventStatus
rotation_tool_grab_cb (RutInputEvent *event,
                       void *user_data)
{
  RutInputEventStatus status = RUT_INPUT_EVENT_STATUS_UNHANDLED;
  RutTool *tool = user_data;

  g_warn_if_fail (tool->button_down);

  if (rut_input_event_get_type (event) != RUT_INPUT_EVENT_TYPE_MOTION)
    return RUT_INPUT_EVENT_STATUS_UNHANDLED;

  switch (rut_motion_event_get_action (event))
    {
    case RUT_MOTION_EVENT_ACTION_MOVE:
      {
        CoglQuaternion camera_rotation;
        CoglQuaternion new_rotation;
        RutEntity *parent;
        CoglQuaternion parent_inverse;
        RutEntity *entity = tool->selected_entity;
        float x = rut_motion_event_get_x (event);
        float y = rut_motion_event_get_y (event);

        rut_arcball_mouse_motion (&tool->arcball, x, y);

        cogl_quaternion_multiply (&camera_rotation,
                                  &tool->arcball.q_drag,
                                  &tool->saved_rotation);

        /* XXX: We have calculated the combined rotation in camera
         * space, we now need to separate out the rotation of the
         * entity itself.
         *
         * We rotate by the inverse of the parent's view transform
         * so we are left with just the entity's rotation.
         */
        parent = rut_graphable_get_parent (entity);

        rut_entity_get_view_rotations (parent,
                                       tool->camera,
                                       &parent_inverse);
        cogl_quaternion_invert (&parent_inverse);

        cogl_quaternion_multiply (&new_rotation,
                                  &parent_inverse,
                                  &camera_rotation);

        rut_entity_set_rotation (entity, &new_rotation);

        rut_shell_queue_redraw (tool->shell);

        status = RUT_INPUT_EVENT_STATUS_HANDLED;
      }
      break;

    case RUT_MOTION_EVENT_ACTION_UP:
      if ((rut_motion_event_get_button_state (event) & RUT_BUTTON_STATE_1) == 0)
        {
          tool->button_down = FALSE;

          rut_shell_ungrab_input (tool->shell,
                                  rotation_tool_grab_cb,
                                  tool);
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
  RutTool *tool = user_data;

  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION &&
      rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_DOWN &&
      rut_motion_event_get_button_state (event) == RUT_BUTTON_STATE_1)
    {
      RutEntity *entity = tool->selected_entity;
      float x = rut_motion_event_get_x (event);
      float y = rut_motion_event_get_y (event);

      rut_shell_grab_input (tool->shell,
                            rut_input_event_get_camera (event),
                            rotation_tool_grab_cb,
                            tool);

      rut_arcball_init (&tool->arcball,
                        tool->screen_pos[0],
                        tool->screen_pos[1],
                        128);

      rut_entity_get_view_rotations (entity,
                                     tool->camera,
                                     &tool->saved_rotation);

      cogl_quaternion_init_identity (&tool->arcball.q_drag);

      rut_arcball_mouse_down (&tool->arcball, x, y);

      tool->button_down = TRUE;

      status = RUT_INPUT_EVENT_STATUS_HANDLED;
    }

  return status;
}

RutTool *
rut_tool_new (RutShell *shell)
{
  RutTool *tool;
  RutContext *ctx;

  tool = g_slice_new0 (RutTool);

  tool->shell = shell;

  ctx = rut_shell_get_context (shell);

  /* pipeline to draw the tool */
  tool->default_pipeline = cogl_pipeline_new (rut_cogl_context);

  /* rotation tool */
  tool->rotation_tool = rut_create_rotation_tool_primitive (ctx, 64);

  /* rotation tool handle circle */
  tool->rotation_tool_handle =
    rut_create_circle_outline_primitive (ctx, 64);

  tool->rotation_circle =
    rut_input_region_new_circle (0, 0, 0, on_rotation_tool_clicked, tool);
  rut_input_region_set_hud_mode (tool->rotation_circle, TRUE);

  return tool;
}

void
rut_tool_set_camera (RutTool *tool,
                     RutEntity *camera)
{
  tool->camera = camera;
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

/* Scale from OpenGL normalized device coordinates (ranging from -1 to 1)
 * to Cogl window/framebuffer coordinates (ranging from 0 to buffer-size) with
 * (0,0) being top left. */
#define VIEWPORT_TRANSFORM_X(x, vp_origin_x, vp_width) \
    (  ( ((x) + 1.0) * ((vp_width) / 2.0) ) + (vp_origin_x)  )
/* Note: for Y we first flip all coordinates around the X axis while in
 * normalized device coodinates */
#define VIEWPORT_TRANSFORM_Y(y, vp_origin_y, vp_height) \
    (  ( ((-(y)) + 1.0) * ((vp_height) / 2.0) ) + (vp_origin_y)  )

/* to call every time the selected entity changes or when the one already
 * selected changes transform. As we have now way to be notified if the
 * transform of an entity has change (yet!) this is called every frame
 * before drawing the tool */
void
rut_tool_update (RutTool *tool,
                 RutEntity *selected_entity)
{
  RutComponent *camera;
  CoglMatrix transform;
  const CoglMatrix *projection;
  float scale_thingy[4], screen_space[4], x, y;
  const float *viewport;

  camera = rut_entity_get_component (tool->camera,
                                     RUT_COMPONENT_TYPE_CAMERA);

  if (selected_entity == NULL)
    {
      tool->selected_entity = NULL;

      /* remove the input region when no entity is selected */
      rut_camera_remove_input_region (RUT_CAMERA (camera),
                                      tool->rotation_circle);

      return;
    }

  /* transform the selected entity up to the projection */
  get_modelview_matrix (tool->camera,
                        selected_entity,
                        &transform);

  tool->position[0] = tool->position[1] = tool->position[2] = 0.f;

  cogl_matrix_transform_points (&transform,
                                3, /* num components for input */
                                sizeof (float) * 3, /* input stride */
                                tool->position,
                                sizeof (float) * 3, /* output stride */
                                tool->position,
                                1 /* n_points */);

  projection = rut_camera_get_projection (RUT_CAMERA (camera));

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
  viewport = rut_camera_get_viewport (RUT_CAMERA (camera));
  x = VIEWPORT_TRANSFORM_X (screen_space[0], viewport[0], viewport[2]);
  y = VIEWPORT_TRANSFORM_Y (screen_space[1], viewport[1], viewport[3]);

  tool->screen_pos[0] = x;
  tool->screen_pos[1] = y;

  rut_input_region_set_circle (tool->rotation_circle, x, y, 64);

  if (tool->selected_entity != selected_entity)
    {
      /* If we go from a "no entity selected" state to a "entity selected"
       * one, we set-up the input region */
      if (tool->selected_entity == NULL)
        rut_camera_add_input_region (RUT_CAMERA (camera),
                                     tool->rotation_circle);

      tool->selected_entity = selected_entity;
    }

  /* save the camera component for other functions to use */
  tool->camera_component = RUT_CAMERA (camera);
}

static float
rut_tool_get_scale_for_length (RutTool *tool,
                               float    length)
{
  return length * tool->scale;
}

static void
get_rotation (RutEntity  *camera,
              RutEntity  *entity,
              CoglMatrix *rotation)
{
  CoglQuaternion q;

  rut_entity_get_view_rotations (entity, camera, &q);
  cogl_matrix_init_from_quaternion (rotation, &q);
}

void
rut_tool_draw (RutTool *tool,
               CoglFramebuffer *fb)
{
  CoglMatrix rotation;
  float scale, aspect_ratio;
  CoglMatrix saved_projection;

  float vp_width, vp_height;

  get_rotation (tool->camera,
                tool->selected_entity,
                &rotation);

  /* we change the projection matrix to clip at -position[2] to clip the
   * half sphere that is away from the camera */
  vp_width = cogl_framebuffer_get_viewport_width (fb);
  vp_height = cogl_framebuffer_get_viewport_height (fb);
  aspect_ratio = vp_width / vp_height;

  cogl_framebuffer_get_projection_matrix (fb, &saved_projection);
  cogl_framebuffer_perspective (fb,
                                rut_camera_get_field_of_view (tool->camera_component),
                                aspect_ratio,
                                rut_camera_get_near_plane (tool->camera_component),
                                -tool->position[2]);

  scale = rut_tool_get_scale_for_length (tool, 128 / vp_width);

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
  cogl_framebuffer_draw_primitive (fb,
                                   tool->default_pipeline,
                                   tool->rotation_tool);
  cogl_framebuffer_pop_matrix (fb);
  cogl_framebuffer_draw_primitive (fb,
                                   tool->default_pipeline,
                                   tool->rotation_tool_handle);
  cogl_framebuffer_scale (fb, 1.1, 1.1, 1.1);
  cogl_framebuffer_draw_primitive (fb,
                                   tool->default_pipeline,
                                   tool->rotation_tool_handle);
  cogl_framebuffer_pop_matrix (fb);

  cogl_framebuffer_set_projection_matrix (fb, &saved_projection);
}

void
rut_tool_free (RutTool *tool)
{
  cogl_object_unref (tool->default_pipeline);
  cogl_object_unref (tool->rotation_tool);
  cogl_object_unref (tool->rotation_tool_handle);
  rut_refable_unref (tool->rotation_circle);

  if (tool->button_down)
    rut_shell_ungrab_input (tool->shell,
                            rotation_tool_grab_cb,
                            tool);

  g_slice_free (RutTool, tool);
}
