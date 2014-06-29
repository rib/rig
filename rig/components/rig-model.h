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

  cg_primitive_t *primitive;

  bool builtin_normals;
  bool builtin_tex_coords;

  /* TODO: I think maybe we should make RigHair and RigModel mutually
   * exclusive and move all of this stuff to RigHair instead. */
  bool is_hair_model;
  RigModelPrivate *priv;
  RutMesh *patched_mesh;
  RutMesh *fin_mesh;
  cg_primitive_t *fin_primitive;
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

cg_primitive_t *
rig_model_get_primitive (RutObject *object);

cg_primitive_t *
rig_model_get_fin_primitive (RutObject *object);

float
rig_model_get_default_hair_length (RutObject *object);

#endif /* _RIG_MODEL_H_ */
