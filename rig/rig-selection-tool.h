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

#ifndef __RIG_SELECTION_TOOL_H__
#define __RIG_SELECTION_TOOL_H__

#include <stdbool.h>

#include <cogl/cogl.h>

#include <rut.h>

#include "rig-camera-view.h"
#include "rig-entity.h"

typedef struct _RigSelectionTool
{
  RutContext *ctx;

  RigCameraView *view;

  RigEntity *camera;
  RutObject *camera_component; /* camera component of the camera above */

  RutGraph *tool_overlay;

  bool active;
  RutClosure *objects_selection_closure;

  c_list_t *selected_entities;

  cg_pipeline_t *default_pipeline;

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
                                   const cg_quaternion_t *start_selection,
                                   const cg_quaternion_t *new_selection,
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
                           RutObject *paint_camera);

void
rig_selection_tool_destroy (RigSelectionTool *tool);

#endif /* __RIG_SELECTION_TOOL_H__ */
