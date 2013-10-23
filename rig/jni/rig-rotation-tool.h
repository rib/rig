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

#include <rut.h>

#include "rig-camera-view.h"

typedef struct _RigRotationTool
{
  RutContext *ctx;

  RigCameraView *view;

  RutEntity *camera;
  RutCamera *camera_component; /* camera component of the camera above */

  bool active;
  RutClosure *objects_selection_closure;

  RutEntity *selected_entity;

  CoglPipeline *default_pipeline;
  CoglPrimitive *rotation_tool;
  CoglPrimitive *rotation_tool_handle;

  RutInputRegion *rotation_circle;
  RutArcball arcball;
  CoglQuaternion start_rotation;
  CoglQuaternion start_view_rotations;
  bool button_down;
  float position[3];    /* transformed position of the selected entity */
  float screen_pos[2];
  float scale;

  RutList rotation_event_cb_list;
} RigRotationTool;

typedef enum
{
  RIG_ROTATION_TOOL_DRAG,
  RIG_ROTATION_TOOL_RELEASE,
  RIG_ROTATION_TOOL_CANCEL
} RigRotationToolEventType;

typedef void
(* RigRotationToolEventCallback) (RigRotationTool *tool,
                                  RigRotationToolEventType type,
                                  const CoglQuaternion *start_rotation,
                                  const CoglQuaternion *new_rotation,
                                  void *user_data);

RigRotationTool *
rig_rotation_tool_new (RigCameraView *view);

void
rig_rotation_tool_set_active (RigRotationTool *tool,
                              bool active);

RutClosure *
rig_rotation_tool_add_event_callback (RigRotationTool *tool,
                                      RigRotationToolEventCallback callback,
                                      void *user_data,
                                      RutClosureDestroyCallback destroy_cb);

void
rig_rotation_tool_draw (RigRotationTool *tool,
                        CoglFramebuffer *fb);

void
rig_rotation_tool_destroy (RigRotationTool *tool);

#endif /* __RUT_TOOL_H__ */
