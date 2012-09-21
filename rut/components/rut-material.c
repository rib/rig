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

#include "rut-material.h"
#include "rut-global.h"
#include "rut-asset.h"
#include "rut-color.h"

RutType rut_material_type;

static RutComponentableVTable _rut_material_componentable_vtable = {
    0
};

void
_rut_material_init_type (void)
{
  rut_type_init (&rut_material_type);
  rut_type_add_interface (&rut_material_type,
                           RUT_INTERFACE_ID_COMPONENTABLE,
                           offsetof (RutMaterial, component),
                           &_rut_material_componentable_vtable);
}

RutMaterial *
rut_material_new_full (RutContext *ctx,
                       RutAsset *asset,
                       const RutColor *color,
                       CoglPipeline *pipeline)
{
  RutMaterial *material = g_slice_new0 (RutMaterial);

  rut_object_init (&material->_parent, &rut_material_type);
  material->component.type = RUT_COMPONENT_TYPE_MATERIAL;

  if (color)
    material->color = *color;
  else
    rut_color_init_from_4f (&material->color, 1, 1, 1, 1);

  if (asset)
    material->asset = rut_refable_ref (asset);

  return material;
}

RutMaterial *
rut_material_new (RutContext *ctx,
                  RutAsset *asset,
                  const RutColor *color)
{
  return rut_material_new_full (ctx, asset, color, NULL);
}

RutAsset *
rut_material_get_asset (RutMaterial *material)
{
  return material->asset;
}

const RutColor *
rut_material_get_color (RutMaterial *material)
{
  return &material->color;
}

