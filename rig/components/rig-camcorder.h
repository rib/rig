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

#ifndef __RIG_CAMCORDER_H__
#define __RIG_CAMCORDER_H__

#include "rig-entity.h"

typedef struct _RigCamcorder RigCamcorder;

#define RIG_CAMCORDER(p) ((RigCamcorder *)(p))

typedef enum
{
  RIG_PROJECTION_PERSPECTIVE,
  RIG_PROJECTION_ORTHOGRAPHIC
} RigProjection;


struct _RigCamcorder
{
  RigComponent component;
  CoglFramebuffer *fb;		  /* framebuffer to draw to */
  CoglMatrix projection;          /* projection */
  float viewport[4];              /* view port of the camera in fb */
  CoglColor background_color;	  /* clear color */
  float fov;                      /* perspective */
  float right, top, left, bottom; /* orthographic */
  float z_near, z_far;
  unsigned int orthographic:1;
  unsigned int projection_dirty;
  unsigned int clear_fb:1;
};

RigComponent *
rig_camcorder_new (void);

void
rig_camcorder_free (RigCamcorder *camcorder);

CoglFramebuffer *
rig_camcorder_get_framebuffer (RigCamcorder *camcorder);

void
rig_camcorder_set_framebuffer (RigCamcorder *camcorder,
                               CoglFramebuffer *fb);

void
rig_camcorder_set_clear (RigCamcorder *camcorder,
                         bool clear);

float *
rig_camcorder_get_viewport (RigCamcorder *camcorder);

void
rig_camcorder_set_viewport (RigCamcorder *camcorder,
                            float viewport[4]);

void
rig_camcorder_set_near_plane (RigCamcorder *camcorder,
                              float z_near);

float
rig_camcorder_get_near_plane (RigCamcorder *camcorder);

void
rig_camcorder_set_far_plane (RigCamcorder *camcorder,
                             float z_far);

float
rig_camcorder_get_far_plane (RigCamcorder *camcorder);

RigProjection
rig_camcorder_get_projection_mode (RigCamcorder *camcorder);

void
rig_camcorder_set_projection_mode (RigCamcorder *camcorder,
                                   RigProjection projection);

void
rig_camcorder_set_field_of_view (RigCamcorder *camcorder,
                                 float fov);

void
rig_camcorder_set_size_of_view (RigCamcorder *camcorder,
                                float right,
                                float top,
                                float left,
                                float bottom);

CoglMatrix *
rig_camcorder_get_projection (RigCamcorder *camcorder);

void
rig_camcorder_set_background_color (RigCamcorder *camcorder,
                                    CoglColor *color);

#endif /* __RIG_CAMCORDER_H__ */
