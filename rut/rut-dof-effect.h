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

#ifndef __RUT_DOF_EFFECT_H__
#define __RUT_DOF_EFFECT_H__

#include <cogl/cogl.h>

#include "rut-context.h"

typedef struct _RutDepthOfField RutDepthOfField;

RutDepthOfField *
rut_dof_effect_new (RutContext *ctx);

void
rut_dof_effect_free (RutDepthOfField *dof);

void
rut_dof_effect_set_framebuffer_size (RutDepthOfField *dof,
                                     int width,
                                     int height);

CoglFramebuffer *
rut_dof_effect_get_depth_pass_fb (RutDepthOfField *dof);

CoglFramebuffer *
rut_dof_effect_get_color_pass_fb (RutDepthOfField *dof);

void
rut_dof_effect_draw_rectangle (RutDepthOfField *dof,
                               CoglFramebuffer *fb,
                               float x1,
                               float y1,
                               float x2,
                               float y2);

#endif /* __RUT_DOF_EFFECT_H__ */
