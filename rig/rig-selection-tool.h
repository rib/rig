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

#ifndef __RIG_SELECTION_TOOL_H__
#define __RIG_SELECTION_TOOL_H__

#include <stdbool.h>

#include <cogl/cogl.h>

#include <rut.h>

#include "rig-camera-view.h"

typedef struct _RigSelectionTool
{
  RutContext *ctx;

  RigCameraView *view;

  RutEntity *camera;
  RutCamera *camera_component; /* camera component of the camera above */

  RutGraph *tool_overlay;

  bool active;
  RutClosure *objects_selection_closure;

  GList *selected_entities;

  CoglPipeline *default_pipeline;

  RutList selection_event_cb_list;
} RigSelectionTool;

typedef enum
{
  RIG_SELECTION_TOOL_DRAG,
  RIG_SELECTION_TOOL_RELEASE,
  RIG_SELECTION_TOOL_CANCEL
} RigSelectionToolEventType;

typedef void
(* RigSelectionToolEventCallback) (RigSelectionTool *tool,
                                   RigSelectionToolEventType type,
                                   const CoglQuaternion *start_selection,
                                   const CoglQuaternion *new_selection,
                                   void *user_data);

RigSelectionTool *
rig_selection_tool_new (RigCameraView *view,
                        RutObject *overlay);

void
rig_selection_tool_set_active (RigSelectionTool *tool,
                               bool active);

RutClosure *
rig_selection_tool_add_event_callback (RigSelectionTool *tool,
                                       RigSelectionToolEventCallback callback,
                                       void *user_data,
                                       RutClosureDestroyCallback destroy_cb);

void
rig_selection_tool_update (RigSelectionTool *tool,
                           RutCamera *paint_camera);

void
rig_selection_tool_destroy (RigSelectionTool *tool);

#endif /* __RIG_SELECTION_TOOL_H__ */
