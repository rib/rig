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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef __RIG_DOF_EFFECT_H__
#define __RIG_DOF_EFFECT_H__

#include <cogl/cogl.h>

#include <rut.h>

typedef struct _RigDepthOfField RigDepthOfField;

#include "rig-engine.h"

RigDepthOfField *
rig_dof_effect_new (RigEngine *engine);

void
rig_dof_effect_free (RigDepthOfField *dof);

void
rig_dof_effect_set_framebuffer_size (RigDepthOfField *dof,
                                     int width,
                                     int height);

CoglFramebuffer *
rig_dof_effect_get_depth_pass_fb (RigDepthOfField *dof);

CoglFramebuffer *
rig_dof_effect_get_color_pass_fb (RigDepthOfField *dof);

void
rig_dof_effect_draw_rectangle (RigDepthOfField *dof,
                               CoglFramebuffer *fb,
                               float x1,
                               float y1,
                               float x2,
                               float y2);

#endif /* __RIG_DOF_EFFECT_H__ */
