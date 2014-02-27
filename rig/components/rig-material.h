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

#ifndef __RIG_MATERIAL_H__
#define __RIG_MATERIAL_H__

#include "rig-entity.h"
#include "rig-asset.h"

typedef struct _RigMaterial RigMaterial;
extern RutType rig_material_type;

enum {
  RIG_MATERIAL_PROP_VISIBLE,
  RIG_MATERIAL_PROP_CAST_SHADOW,
  RIG_MATERIAL_PROP_RECEIVE_SHADOW,
  RIG_MATERIAL_PROP_COLOR_SOURCE,
  RIG_MATERIAL_PROP_NORMAL_MAP,
  RIG_MATERIAL_PROP_ALPHA_MASK,
  RIG_MATERIAL_PROP_AMBIENT,
  RIG_MATERIAL_PROP_DIFFUSE,
  RIG_MATERIAL_PROP_SPECULAR,
  RIG_MATERIAL_PROP_SHININESS,
  RIG_MATERIAL_PROP_ALPHA_MASK_THRESHOLD,
  RIG_MATERIAL_N_PROPS
};

struct _RigMaterial
{
  RutObjectBase _base;


  RutComponentableProps component;
  RigAsset *color_source_asset;
  RigAsset *normal_map_asset;
  RigAsset *alpha_mask_asset;

  CoglColor ambient;
  CoglColor diffuse;
  CoglColor specular;
  float shininess;

  float alpha_mask_threshold;

  int uniforms_age;
  int uniforms_flush_age;

  RutIntrospectableProps introspectable;
  RutProperty properties[RIG_MATERIAL_N_PROPS];

  unsigned int visible:1;
  unsigned int cast_shadow:1;
  unsigned int receive_shadow:1;
};

void
_rig_material_init_type (void);

RigMaterial *
rig_material_new (RutContext *ctx,
                  RigAsset *asset);

void
rig_material_set_color_source_asset (RutObject *object,
                                     RigAsset *asset);

RigAsset *
rig_material_get_color_source_asset (RutObject *object);

void
rig_material_set_normal_map_asset (RutObject *material,
                                   RigAsset *normal_map_asset);

RigAsset *
rig_material_get_normal_map_asset (RutObject *material);

void
rig_material_set_alpha_mask_asset (RutObject *material,
                                   RigAsset *alpha_mask_asset);

RigAsset *
rig_material_get_alpha_mask_asset (RutObject *material);

void
rig_material_set_ambient (RutObject *material,
                          const CoglColor *color);

const CoglColor *
rig_material_get_ambient (RutObject *material);

void
rig_material_set_diffuse (RutObject *material,
                          const CoglColor *color);

const CoglColor *
rig_material_get_diffuse (RutObject *material);

void
rig_material_set_specular (RutObject *material,
                           const CoglColor *color);

const CoglColor *
rig_material_get_specular (RutObject *material);

void
rig_material_set_shininess (RutObject *material,
                            float shininess);

float
rig_material_get_shininess (RutObject *material);

float
rig_material_get_alpha_mask_threshold (RutObject *material);

void
rig_material_set_alpha_mask_threshold (RutObject *material,
                                       float alpha_mask_threshold);

void
rig_material_flush_uniforms (RigMaterial *material,
                             CoglPipeline *pipeline);

void
rig_material_flush_uniforms_ignore_age (RigMaterial *material);

bool
rig_material_get_visible (RutObject *material);

void
rig_material_set_visible (RutObject *material, bool visible);

bool
rig_material_get_cast_shadow (RutObject *entity);

void
rig_material_set_cast_shadow (RutObject *material,
                              bool cast_shadow);

bool
rig_material_get_receive_shadow (RutObject *material);

void
rig_material_set_receive_shadow (RutObject *material,
                                 bool receive_shadow);


#endif /* __RIG_MATERIAL_H__ */
