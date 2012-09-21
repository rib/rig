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

#ifndef __RUT_MATERIAL_H__
#define __RUT_MATERIAL_H__

#include "rut-entity.h"
#include "rut-asset.h"

typedef struct _RutMaterial RutMaterial;
#define RUT_MATERIAL(p) ((RutMaterial *)(p))
extern RutType rut_material_type;

struct _RutMaterial
{
  RutObjectProps _parent;
  RutComponentableProps component;
  RutAsset *asset;
  RutColor color;
};

void
_rut_material_init_type (void);

RutMaterial *
rut_material_new (RutContext *ctx,
                  RutAsset *asset,
                  const RutColor *color);

RutAsset *
rut_material_get_asset (RutMaterial *material);

const RutColor *
rut_material_get_color (RutMaterial *material);

#endif /* __RUT_MATERIAL_H__ */
