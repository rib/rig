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

#ifndef __RIG_ROTATION_TOOL_H__
#define __RIG_ROTATION_TOOL_H__

#include <stdbool.h>

#include <cogl/cogl.h>

#include <rut.h>

#include "rig-camera-view.h"

typedef struct _RigRotationTool
{
  RutContext *ctx;

  RigCameraView *view;

  RigEntity *camera;
  RutObject *camera_component; /* camera component of the camera above */

  bool active;
  RutClosure *objects_selection_closure;

  RigEntity *selected_entity;

  cg_pipeline_t *default_pipeline;
  cg_primitive_t *rotation_tool;
  cg_primitive_t *rotation_tool_handle;

  RutInputRegion *rotation_circle;
  RutArcball arcball;
  cg_quaternion_t start_rotation;
  cg_quaternion_t start_view_rotations;
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
                                  const cg_quaternion_t *start_rotation,
                                  const cg_quaternion_t *new_rotation,
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
                        cg_framebuffer_t *fb);

void
rig_rotation_tool_destroy (RigRotationTool *tool);

#endif /* __RIG_ROTATION_TOOL_H__ */
