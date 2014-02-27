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

#ifndef __RIG_CAMERA_H__
#define __RIG_CAMERA_H__

#include <cogl/cogl.h>

#include <rut.h>

#include "rig-types.h"
#include "rig-entity.h"

/* NB: consider changes to _rig_camera_copy if adding
 * properties, or making existing properties
 * introspectable */
enum {
  RIG_CAMERA_PROP_MODE,
  RIG_CAMERA_PROP_VIEWPORT_X,
  RIG_CAMERA_PROP_VIEWPORT_Y,
  RIG_CAMERA_PROP_VIEWPORT_WIDTH,
  RIG_CAMERA_PROP_VIEWPORT_HEIGHT,
  RIG_CAMERA_PROP_FOV,
  RIG_CAMERA_PROP_NEAR,
  RIG_CAMERA_PROP_FAR,
  RIG_CAMERA_PROP_ZOOM,
  RIG_CAMERA_PROP_BG_COLOR,
  RIG_CAMERA_PROP_FOCAL_DISTANCE,
  RIG_CAMERA_PROP_DEPTH_OF_FIELD,
  RIG_CAMERA_N_PROPS
};


typedef struct _RigCamera RigCamera;
extern RutType rig_camera_type;

RigCamera *
rig_camera_new (RigEngine *engine,
                float width,
                float height,
                CoglFramebuffer *framebuffer); /* may be NULL */

#endif /* __RIG_CAMERA_H__ */
