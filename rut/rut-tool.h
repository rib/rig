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

#ifndef __RUT_TOOL_H__
#define __RUT_TOOL_H__

#include <stdbool.h>

#include <cogl/cogl.h>

#include <rut-shell.h>
#include <rut-entity.h>
#include <rut-arcball.h>
#include <rut-closure.h>

typedef struct _RutTool
{
  RutShell *shell;
  RutEntity *selected_entity;
  CoglPipeline *default_pipeline;
  CoglPrimitive *rotation_tool;
  CoglPrimitive *rotation_tool_handle;
  RutInputRegion *rotation_circle;
  RutArcball arcball;
  CoglQuaternion start_rotation;
  CoglQuaternion start_view_rotations;
  bool button_down;
  RutEntity *camera;
  RutCamera *camera_component; /* camera component of the camera above */
  float position[3];    /* transformed position of the selected entity */
  float screen_pos[2];
  float scale;
  RutList rotation_event_cb_list;
} RutTool;

typedef enum
{
  RUT_TOOL_ROTATION_DRAG,
  RUT_TOOL_ROTATION_RELEASE,
  RUT_TOOL_ROTATION_CANCEL
} RutToolRotationEventType;

typedef void
(* RutToolRotationEventCallback) (RutTool *tool,
                                  RutToolRotationEventType type,
                                  const CoglQuaternion *start_rotation,
                                  const CoglQuaternion *new_rotation,
                                  void *user_data);

RutTool *
rut_tool_new (RutShell *shell);

void
rut_tool_set_camera (RutTool *tool,
                     RutEntity *camera);

void
rut_tool_update (RutTool *tool,
                 RutEntity *selected_entity);

void
rut_tool_draw (RutTool *tool,
               CoglFramebuffer *fb);

RutClosure *
rut_tool_add_rotation_event_callback (RutTool *tool,
                                      RutToolRotationEventCallback callback,
                                      void *user_data,
                                      RutClosureDestroyCallback destroy_cb);

void
rut_tool_free (RutTool *tool);

#endif /* __RUT_TOOL_H__ */
