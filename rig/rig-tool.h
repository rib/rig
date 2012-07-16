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

#ifndef __RIG_TOOL_H__
#define __RIG_TOOL_H__

#include <stdbool.h>

#include <cogl/cogl.h>

#include <rig/rig-shell.h>
#include <rig/rig-entity.h>
#include <rig/rig-arcball.h>

typedef struct _RigTool
{
  RigShell *shell;
  RigEntity *selected_entity;
  CoglPipeline *default_pipeline;
  CoglPrimitive *rotation_tool;
  CoglPrimitive *rotation_tool_handle;
  RigInputRegion *rotation_circle;
  RigArcball arcball;
  CoglQuaternion saved_rotation;
  bool button_down;
  RigEntity *camera;
  RigCamera *camera_component; /* camera component of the camera above */
  float position[3];    /* transformed position of the selected entity */
  float screen_pos[2];
  float scale;
} RigTool;

RigTool *
rig_tool_new (RigShell *shell);

void
rig_tool_set_camera (RigTool *tool,
                     RigEntity *camera);

void
rig_tool_update (RigTool *tool,
                 RigEntity *selected_entity);

void
rig_tool_draw (RigTool *tool,
               CoglFramebuffer *fb);

#endif /* __RIG_TOOL_H__ */
