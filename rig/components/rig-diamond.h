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
