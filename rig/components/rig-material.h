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

#ifndef __RIG_MATERIAL_H__
#define __RIG_MATERIAL_H__

#include "rig-entity.h"

typedef struct _RigMaterial RigMaterial;
#define RIG_MATERIAL(p) ((RigMaterial *)(p))
extern RigType rig_material_type;

struct _RigMaterial
{
  RigObjectProps _parent;
  RigComponentableProps component;
  CoglPipeline *pipeline; /* pipeline where to update the material uniforms */
};

void
_rig_material_init_type (void);

RigMaterial *
rig_material_new (RigContext *ctx);

RigMaterial *
rig_material_new_with_pipeline (RigContext *ctx,
                                CoglPipeline *pipeline);

CoglPipeline *
rig_material_get_pipeline (RigMaterial *material);

#endif /* __RIG_MATERIAL_H__ */
