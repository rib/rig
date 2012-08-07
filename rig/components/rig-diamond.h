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

#ifndef __RIG_DIAMOND_H__
#define __RIG_DIAMOND_H__

#include "rig-entity.h"

typedef struct _RigDiamondSlice RigDiamondSlice;
#define RIG_DIAMOND_SLICE(X) ((RigDiamondSlice *)X)
extern RigType _rig_diamond_slice_type;

typedef struct _RigDiamondSlice
{
  RigObjectProps _parent;
  int ref_count;

  CoglMatrix rotate_matrix;

  CoglTexture *texture;

  float size;

  CoglPipeline *pipeline;
  CoglPrimitive *primitive;

} RigDiamondSlice;

void
_rig_diamond_slice_init_type (void);

CoglPipeline *
rig_diamond_slice_get_pipeline_template (RigDiamondSlice *slice);

typedef struct _RigDiamond RigDiamond;
#define RIG_DIAMOND(p) ((RigDiamond *)(p))
extern RigType rig_diamond_type;

struct _RigDiamond
{
  RigObjectProps _parent;
  RigComponentableProps component;

  RigContext *ctx;

  RigDiamondSlice *slice;

  CoglVertexP3 pick_vertices[6];

  int size;
};

void
_rig_diamond_init_type (void);

RigDiamond *
rig_diamond_new (RigContext *ctx,
                 float size,
                 int tex_width,
                 int tex_height);

RigDiamondSlice *
rig_diamond_get_slice (RigDiamond *diamond);

float
rig_diamond_get_size (RigDiamond *diamond);

CoglPrimitive *
rig_diamond_get_primitive (RigObject *object);

void
rig_diamond_apply_mask (RigDiamond *diamond,
                        CoglPipeline *pipeline);

void *
rig_diamond_get_vertex_data (RigDiamond *diamond,
                             size_t *stride,
                             int *n_vertices);

#endif /* __RIG_DIAMOND_H__ */
