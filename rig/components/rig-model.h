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

#ifndef _RIG_MODEL_H_
#define _RIG_MODEL_H_

#include <stdint.h>

#include <cogl/cogl.h>

#include <rut.h>

#include "rig-entity.h"
#include "rig-asset.h"

typedef struct _RigModel RigModel;
typedef struct _RigModelPrivate RigModelPrivate;
extern RutType rig_model_type;

typedef enum _RigModelType
{
  RIG_MODEL_TYPE_TEMPLATE,
  RIG_MODEL_TYPE_FILE,
} RigModelType;

struct _RigModel
{
  RutObjectBase _base;

  RutContext *ctx;

  RutComponentableProps component;

  RigModelType type;

  RigAsset *asset;

  RutMesh *mesh;

  float min_x;
  float max_x;
  float min_y;
  float max_y;
  float min_z;
  float max_z;

  CoglPrimitive *primitive;

  bool builtin_normals;
  bool builtin_tex_coords;

  /* TODO: I think maybe we should make RigHair and RigModel mutually
   * exclusive and move all of this stuff to RigHair instead. */
  bool is_hair_model;
  RigModelPrivate *priv;
  RutMesh *patched_mesh;
  RutMesh *fin_mesh;
  CoglPrimitive *fin_primitive;
  float default_hair_length;
};

void
_rig_model_init_type (void);

/* NB: the mesh given here is copied before deriving missing
 * attributes and not used directly.
 *
 * In the case where we are loading a model from a serialized
 * UI then the serialized data should be used directly and
 * there should be no need to derive missing attributes at
 * runtime.
 */
RigModel *
rig_model_new_from_asset_mesh (RutContext *ctx,
                               RutMesh *mesh,
                               bool needs_normals,
                               bool needs_tex_coords);

RigModel *
rig_model_new_from_asset (RutContext *ctx,
                          RigAsset *asset);

RigModel *
rig_model_new_for_hair (RigModel *base);

RutMesh *
rig_model_get_mesh (RutObject *self);

RigAsset *
rig_model_get_asset (RigModel *model);

CoglPrimitive *
rig_model_get_primitive (RutObject *object);

CoglPrimitive *
rig_model_get_fin_primitive (RutObject *object);

float
rig_model_get_default_hair_length (RutObject *object);

#endif /* _RIG_MODEL_H_ */
