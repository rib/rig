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
#include "rig-asset.h"

typedef struct _RigMaterial RigMaterial;
#define RIG_MATERIAL(p) ((RigMaterial *)(p))
extern RigType rig_material_type;

struct _RigMaterial
{
  RigObjectProps _parent;
  RigComponentableProps component;
  RigAsset *asset;
  RigColor color;
};

void
_rig_material_init_type (void);

RigMaterial *
rig_material_new (RigContext *ctx,
                  RigAsset *asset,
                  const RigColor *color);

RigAsset *
rig_material_get_asset (RigMaterial *material);

const RigColor *
rig_material_get_color (RigMaterial *material);

#endif /* __RIG_MATERIAL_H__ */
