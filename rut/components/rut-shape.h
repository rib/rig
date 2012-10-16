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

#ifndef __RUT_SHAPE_H__
#define __RUT_SHAPE_H__

#include "rut-entity.h"

typedef struct _RutShapeModel RutShapeModel;
#define RUT_SHAPE_SLICE(X) ((RutShapeModel *)X)
extern RutType _rut_shape_model_type;

typedef struct _RutShapeModel
{
  RutObjectProps _parent;
  int ref_count;

  float size;

  /* TODO: Allow this to be an asset */
  CoglTexture *shape_texture;

  RutMesh *mesh;

  /* TODO: optionally copy the shape texture into a cpu cached buffer
   * and pick by sampling into that instead of using geometry. */
  RutMesh *pick_mesh;

  CoglPrimitive *primitive;

} RutShapeModel;

void
_rut_shape_model_init_type (void);

CoglPipeline *
rut_shape_model_get_pipeline_template (RutShapeModel *model);

typedef struct _RutShape RutShape;
#define RUT_SHAPE(p) ((RutShape *)(p))
extern RutType rut_shape_type;

struct _RutShape
{
  RutObjectProps _parent;

  int ref_count;

  RutComponentableProps component;

  RutContext *ctx;

  RutShapeModel *model;

  int size;
};

void
_rut_shape_init_type (void);

RutShape *
rut_shape_new (RutContext *ctx,
               float size,
               int tex_width,
               int tex_height);

RutShapeModel *
rut_shape_get_model (RutShape *shape);

float
rut_shape_get_size (RutShape *shape);

CoglPrimitive *
rut_shape_get_primitive (RutObject *object);

/* TODO: Perhaps add a RUT_INTERFACE_ID_GEOMETRY_COMPONENTABLE
 * interface with a ->get_shape_texture() methos so we can
 * generalize rut_diamond_apply_mask() and
 * rut_shape_get_shape_texture()
 */
CoglTexture *
rut_shape_get_shape_texture (RutShape *shape);

RutMesh *
rut_shape_get_pick_mesh (RutShape *shape);

#endif /* __RUT_SHAPE_H__ */
