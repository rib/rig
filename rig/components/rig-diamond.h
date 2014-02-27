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

#ifndef __RIG_DIAMOND_H__
#define __RIG_DIAMOND_H__

#include "rig-entity.h"

typedef struct _RigDiamondSlice RigDiamondSlice;
#define RIG_DIAMOND_SLICE(X) ((RigDiamondSlice *)X)
extern RutType _rig_diamond_slice_type;

struct _RigDiamondSlice
{
  RutObjectBase _base;

  CoglMatrix rotate_matrix;

  float size;

  RutMesh *mesh;

};

void
_rig_diamond_slice_init_type (void);

typedef struct _RigDiamond RigDiamond;
#define RIG_DIAMOND(p) ((RigDiamond *)(p))
extern RutType rig_diamond_type;

struct _RigDiamond
{
  RutObjectBase _base;


  RutComponentableProps component;

  RutContext *ctx;

  RigDiamondSlice *slice;

  RutMesh *pick_mesh;

  int size;
};

void
_rig_diamond_init_type (void);

RigDiamond *
rig_diamond_new (RutContext *ctx,
                 float size,
                 int tex_width,
                 int tex_height);

float
rig_diamond_get_size (RigDiamond *diamond);

CoglPrimitive *
rig_diamond_get_primitive (RutObject *object);

void
rig_diamond_apply_mask (RigDiamond *diamond,
                        CoglPipeline *pipeline);

RutMesh *
rig_diamond_get_pick_mesh (RutObject *self);

#endif /* __RIG_DIAMOND_H__ */
