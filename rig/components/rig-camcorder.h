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
  RIG_CAMCORDER_FLAG_NONE             = 0,
  RIG_CAMCORDER_FLAG_ORTHOGRAPHIC     = 1 << 0,
  RIG_CAMCORDER_FLAG_VIEW_DIRTY       = 1 << 1,
  RIG_CAMCORDER_FLAG_PROJECTION_DIRTY = 1 << 2,
} RigCamcorderFlag;

typedef enum
{
  RIG_PROJECTION_PERSPECTIVE,
  RIG_PROJECTION_ORTHOGRAPHIC
} RigProjection;


#define CAMCORDER_HAS_FLAG(camcorder,flag)  ((camcorder)->flags & RIG_CAMCORDER_FLAG_##flag)
#define CAMCORDER_SET_FLAG(camcorder,flag)  ((camcorder)->flags |= RIG_CAMCORDER_FLAG_##flag)
#define CAMCORDER_CLEAR_FLAG(camcorder,flag)((camcorder)->flags &= ~(RIG_CAMCORDER_FLAG_##flag))

struct _RigCamcorder
{
  RigComponent component;
  uint32_t flags;
  CoglFramebuffer *fb;		/* framebuffer to draw to */
  float viewport[4];            /* view port of the camera in fb */
  CoglColor background_color;	/* clear color */
  float fov;                    /* perspective */
  float size;                   /* orthographic */
  float z_near, z_far;
};

RigComponent *    rig_camcorder_new                   (void);
void              rig_camcorder_free		      (RigCamcorder *camcorder);
CoglFramebuffer * rig_camcorder_get_framebuffer       (RigCamcorder *camcorder);
void              rig_camcorder_set_framebuffer       (RigCamcorder    *camcorder,
                                                       CoglFramebuffer *fb);
float *           rig_camcorder_get_viewport          (RigCamcorder *camcorder);
void              rig_camcorder_set_viewport          (RigCamcorder *camcorder,
                                                       float         viewport[4]);
void	          rig_camcorder_set_near_plane        (RigCamcorder *camcorder,
                                                       float         z_near);
void	          rig_camcorder_set_far_plane         (RigCamcorder *camcorder,
                                                       float         z_far);
RigProjection     rig_camcorder_get_projection        (RigCamcorder *camcorder);
void	          rig_camcorder_set_projection        (RigCamcorder *camcorder,
                                                       RigProjection projection);
void	          rig_camcorder_set_field_of_view     (RigCamcorder *camcorder,
                                                       float         fov);
void              rig_camcorder_set_size_of_view      (RigCamcorder *camcorder,
                                                       float         sov);
void	          rig_camcorder_set_background_color  (RigCamcorder *camcorder,
                                                       CoglColor    *color);

#endif /* __RIG_CAMCORDER_H__ */
