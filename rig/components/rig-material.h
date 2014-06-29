/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012  Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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

  cg_color_t ambient;
  cg_color_t diffuse;
  cg_color_t specular;
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
                          const cg_color_t *color);

const cg_color_t *
rig_material_get_ambient (RutObject *material);

void
rig_material_set_diffuse (RutObject *material,
                          const cg_color_t *color);

const cg_color_t *
rig_material_get_diffuse (RutObject *material);

void
rig_material_set_specular (RutObject *material,
                           const cg_color_t *color);

const cg_color_t *
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
                             cg_pipeline_t *pipeline);

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
