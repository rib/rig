/*
 * Rut
 *
 * Copyright (C) 2013  Intel Corporation
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

#ifndef _RIG_CAMERA_VIEW_H_
#define _RIG_CAMERA_VIEW_H_

#include "rut.h"

/* Forward declare this since there is a circluar header dependency
 * between rig-camera-view.h and rig-engine.h */
typedef struct _RigCameraView RigCameraView;

#include "rig-engine.h"
#include "rig-selection-tool.h"
#include "rig-rotation-tool.h"

typedef struct _EntityTranslateGrabClosure EntityTranslateGrabClosure;
typedef struct _EntitiesTranslateGrabClosure EntitiesTranslateGrabClosure;

typedef struct
{
  RutEntity *origin_offset; /* negative offset */
  RutEntity *dev_scale; /* scale to fit device coords */
  RutEntity *screen_pos; /* position screen in edit view */
} RigCameraViewDeviceTransforms;

struct _RigCameraView
{
  RutObjectProps _parent;

  RigEngine *engine;

  RutContext *context;

  int ref_count;

  RutGraphableProps graphable;
  RutPaintableProps paintable;

  float width, height;

  CoglPipeline *bg_pipeline;

  RutGraph *scene;

  float origin[3];
  //float saved_origin[3];

  float device_scale;

  EntitiesTranslateGrabClosure *entities_translate_grab_closure;

  RutEntity *view_camera_to_origin; /* move to origin */
  RutEntity *view_camera_rotate; /* armature rotate rotate */
  RutEntity *view_camera_armature; /* armature length */
  RutEntity *view_camera_2d_view; /* setup 2d view, origin top-left */
  RigCameraViewDeviceTransforms view_device_transforms;

  RutEntity *play_camera;
  RutCamera *play_camera_component;
  RigCameraViewDeviceTransforms play_device_transforms;
  /* This entity is added as a child of all of the play device
   * transforms. During paint the camera component is temporarily
   * stolen from the play camera entity so that it can be transformed
   * with the device transforms */
  RutEntity *play_dummy_entity;

  RutEntity *view_camera;
  RutCamera *view_camera_component;
  float view_camera_z;
  RutInputRegion *input_region;

  float last_viewport_x;
  float last_viewport_y;
  CoglBool dirty_viewport_size;

#ifdef RIG_EDITOR_ENABLED
  RutGraph *tool_overlay;
  RigSelectionTool *selection_tool;
  RigRotationTool *rotation_tool;
  RigToolID tool_id;
#endif
};

extern RutType rig_view_type;

RigCameraView *
rig_camera_view_new (RigEngine *engine);

void
rig_camera_view_set_scene (RigCameraView *view,
                           RutGraph *scene);

void
rig_camera_view_set_play_camera (RigCameraView *view,
                                 RutEntity *play_camera);

#endif /* _RIG_CAMERA_VIEW_H_ */
