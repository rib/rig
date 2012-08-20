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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "rig-material.h"
#include "rig-global.h"
#include "rig-asset.h"
#include "rig-color.h"

RigType rig_material_type;

static RigComponentableVTable _rig_material_componentable_vtable = {
    0
};

void
_rig_material_init_type (void)
{
  rig_type_init (&rig_material_type);
  rig_type_add_interface (&rig_material_type,
                           RIG_INTERFACE_ID_COMPONENTABLE,
                           offsetof (RigMaterial, component),
                           &_rig_material_componentable_vtable);
}

RigMaterial *
rig_material_new_full (RigContext *ctx,
                       RigAsset *asset,
                       const RigColor *color,
                       CoglPipeline *pipeline)
{
  RigMaterial *material = g_slice_new0 (RigMaterial);

  rig_object_init (&material->_parent, &rig_material_type);
  material->component.type = RIG_COMPONENT_TYPE_MATERIAL;

  if (color)
    material->color = *color;
  else
    rig_color_init_from_4f (&material->color, 1, 1, 1, 1);

  if (asset)
    material->asset = rig_ref_countable_ref (asset);

  /* TODO: remove - requires updating examples/editor.c */
  if (pipeline)
    material->pipeline = cogl_object_ref (pipeline);

  return material;
}

RigMaterial *
rig_material_new (RigContext *ctx,
                  RigAsset *asset,
                  const RigColor *color)
{
  return rig_material_new_full (ctx, asset, color, NULL);
}

/* TODO: remove - requires updating examples/editor.c */
RigMaterial *
rig_material_new_with_pipeline (RigContext *ctx,
                                CoglPipeline *pipeline)
{
  return rig_material_new_full (ctx, NULL, NULL, pipeline);
}

RigAsset *
rig_material_get_asset (RigMaterial *material)
{
  return material->asset;
}

const RigColor *
rig_material_get_color (RigMaterial *material)
{
  return &material->color;
}

/* TODO: remove - requires updating examples/editor.c */
CoglPipeline *
rig_material_get_pipeline (RigMaterial *material)
{
  return material->pipeline;
}
