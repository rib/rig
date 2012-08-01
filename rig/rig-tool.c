/*
 * Rig
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

#include "rig-global.h"
#include "rig-types.h"
#include "rig-geometry.h"

#include "components/rig-camera.h"

#include "rig-tool.h"

static RigInputEventStatus
on_rotation_tool_clicked (RigInputRegion *region,
                          RigInputEvent *event,
                          void *user_data)
{
  RigTool *tool = user_data;
  RigMotionEventAction action;
  RigButtonState state;
  RigInputEventStatus status = RIG_INPUT_EVENT_STATUS_UNHANDLED;
  RigEntity *entity;
  float x, y;

  entity = tool->selected_entity;

  switch (rig_input_event_get_type (event))
    {
      case RIG_INPUT_EVENT_TYPE_MOTION:
      {
        action = rig_motion_event_get_action (event);
        state = rig_motion_event_get_button_state (event);
        x = rig_motion_event_get_x (event);
        y = rig_motion_event_get_y (event);

        y = -y + 2 * (tool->screen_pos[1]);

        if (action == RIG_MOTION_EVENT_ACTION_DOWN &&
            state == RIG_BUTTON_STATE_1)
          {
            rig_input_region_set_circle (tool->rotation_circle,
                                         tool->screen_pos[0],
                                         tool->screen_pos[1],
                                         128);


            rig_arcball_init (&tool->arcball,
                              tool->screen_pos[0],
                              tool->screen_pos[1],
                              128);

            rig_entity_get_view_rotations (entity, tool->camera, &tool->saved_rotation);

            cogl_quaternion_init_identity (&tool->arcball.q_drag);

            rig_arcball_mouse_down (&tool->arcball, x, y);

            tool->button_down = TRUE;

            status = RIG_INPUT_EVENT_STATUS_HANDLED;
          }
        else if (action == RIG_MOTION_EVENT_ACTION_MOVE &&
                 state == RIG_BUTTON_STATE_1)
          {
            CoglQuaternion camera_rotation;
            CoglQuaternion new_rotation;
            RigEntity *parent;
            CoglQuaternion parent_inverse;

            if (!tool->button_down)
              break;

            rig_arcball_mouse_motion (&tool->arcball, x, y);

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
            parent = rig_graphable_get_parent (entity);

            rig_entity_get_view_rotations (parent,
                                           tool->camera,
                                           &parent_inverse);
            cogl_quaternion_invert (&parent_inverse);

            cogl_quaternion_multiply (&new_rotation,
                                      &parent_inverse,
                                      &camera_rotation);

            rig_entity_set_rotation (entity, &new_rotation);

            rig_shell_queue_redraw (tool->shell);

            status = RIG_INPUT_EVENT_STATUS_HANDLED;
          }
        else if (action == RIG_MOTION_EVENT_ACTION_UP)
          {
            tool->button_down = FALSE;

            rig_input_region_set_circle (tool->rotation_circle, x, y, 64);
          }

        break;
      }

      case RIG_INPUT_EVENT_TYPE_KEY:
        break;
    }

  return status;
}

RigTool *
rig_tool_new (RigShell *shell)
{
  RigTool *tool;
  RigContext *ctx;

  tool = g_slice_new0 (RigTool);

  tool->shell = shell;

  ctx = rig_shell_get_context (shell);

  /* pipeline to draw the tool */
  tool->default_pipeline = cogl_pipeline_new (rig_cogl_context);

  /* rotation tool */
  tool->rotation_tool = rig_create_rotation_tool_primitive (ctx, 64);

  /* rotation tool handle circle */
  tool->rotation_tool_handle =
    rig_create_circle_outline_primitive (ctx, 64);

  tool->rotation_circle =
    rig_input_region_new_circle (0, 0, 0, on_rotation_tool_clicked, tool);

  return tool;
}

void
rig_tool_set_camera (RigTool *tool,
                     RigEntity *camera)
{
  tool->camera = camera;
}

static void
get_modelview_matrix (RigEntity  *camera,
                      RigEntity  *entity,
                      CoglMatrix *modelview)
{
  RigCamera *camera_component =
    rig_entity_get_component (camera, RIG_COMPONENT_TYPE_CAMERA);
  *modelview = *rig_camera_get_view_transform (camera_component);

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

/* to call every time the selected entity changes or when the one already
 * selected changes transform. As we have now way to be notified if the
 * transform of an entity has change (yet!) this is called every frame
 * before drawing the tool */
void
rig_tool_update (RigTool *tool,
                 RigEntity *selected_entity)
{
  RigComponent *camera;
  CoglMatrix transform;
  const CoglMatrix *projection;
  float scale_thingy[4], screen_space[4], x, y;
  const float *viewport;

  if (selected_entity == NULL)
    {
      tool->selected_entity = NULL;

      /* remove the input region when no entity is selected */
      rig_shell_remove_input_region (tool->shell, tool->rotation_circle);

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

  camera = rig_entity_get_component (tool->camera,
                                     RIG_COMPONENT_TYPE_CAMERA);
  projection = rig_camera_get_projection (RIG_CAMERA (camera));

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
  viewport = rig_camera_get_viewport (RIG_CAMERA (camera));
  x = VIEWPORT_TRANSFORM_X (screen_space[0], viewport[0], viewport[2]);
  y = VIEWPORT_TRANSFORM_Y (screen_space[1], viewport[1], viewport[3]);

  tool->screen_pos[0] = x;
  tool->screen_pos[1] = y;

  if (!tool->button_down)
    rig_input_region_set_circle (tool->rotation_circle, x, y, 64);

  if (tool->selected_entity != selected_entity)
    {
      /* If we go from a "no entity selected" state to a "entity selected"
       * one, we set-up the input region */
      if (tool->selected_entity == NULL)
          rig_shell_add_input_region (tool->shell, tool->rotation_circle);

      tool->selected_entity = selected_entity;
    }

  /* save the camera component for other functions to use */
  tool->camera_component = RIG_CAMERA (camera);
}

static float
rig_tool_get_scale_for_length (RigTool *tool,
                               float    length)
{
  return length * tool->scale;
}

static void
get_rotation (RigEntity  *camera,
              RigEntity  *entity,
              CoglMatrix *rotation)
{
  CoglQuaternion q;

  rig_entity_get_view_rotations (entity, camera, &q);
  cogl_matrix_init_from_quaternion (rotation, &q);
}

void
rig_tool_draw (RigTool *tool,
               CoglFramebuffer *fb)
{
  CoglMatrix rotation;
  float scale, aspect_ratio;
  CoglMatrix saved_projection;

  float fb_width, fb_height;

  get_rotation (tool->camera,
                tool->selected_entity,
                &rotation);

  /* we change the projection matrix to clip at -position[2] to clip the
   * half sphere that is away from the camera */
  fb_width = cogl_framebuffer_get_width (fb);
  fb_height = cogl_framebuffer_get_height (fb);
  aspect_ratio = fb_width / fb_height;

  cogl_framebuffer_get_projection_matrix (fb, &saved_projection);
  cogl_framebuffer_perspective (fb,
                                rig_camera_get_field_of_view (tool->camera_component),
                                aspect_ratio,
                                rig_camera_get_near_plane (tool->camera_component),
                                -tool->position[2]);

  scale = rig_tool_get_scale_for_length (tool, 128 / fb_width);

  /* draw the tool */
  cogl_framebuffer_push_matrix (fb);
  cogl_framebuffer_identity_matrix (fb);
  cogl_framebuffer_translate (fb,
                              tool->position[0],
                              tool->position[1],
                              tool->position[2]);
  cogl_framebuffer_scale (fb, scale, scale, scale);
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
