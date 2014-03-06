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

#ifndef __RIG_POINTALISM_GRID_H__
#define __RIG_POINTALISM_GRID_H__

#include "rig-entity.h"

typedef struct _RigPointalismGridSlice RigPointalismGridSlice;
#define RIG_POINTALISM_GRID_SLICE(X) ((RigPointalismGridSlice *)X)
extern RutType _rig_pointalism_grid_slice_type;

enum {
  RIG_POINTALISM_GRID_PROP_SCALE,
  RIG_POINTALISM_GRID_PROP_Z,
  RIG_POINTALISM_GRID_PROP_LIGHTER,
  RIG_POINTALISM_GRID_PROP_CELL_SIZE,
  RIG_POINTALISM_GRID_N_PROPS
};

struct _RigPointalismGridSlice
{
  RutObjectBase _base;
  RutMesh *mesh;
};

void
_rig_pointalism_grid_slice_init_type (void);

typedef struct _RigPointalismGrid RigPointalismGrid;
#define RIG_POINTALISM_GRID(p) ((RigPointalismGrid *)(p))
extern RutType rig_pointalism_grid_type;

struct _RigPointalismGrid
{
  RutObjectBase _base;


  RutComponentableProps component;

  RutContext *ctx;

  RutList updated_cb_list;

  RigPointalismGridSlice *slice;

  RutMesh *pick_mesh;

  float pointalism_scale;
  float pointalism_z;
  bool pointalism_lighter;

  RutIntrospectableProps introspectable;
  RutProperty properties[RIG_POINTALISM_GRID_N_PROPS];
  float cell_size;
  int tex_width;
  int tex_height;
};

void
_rig_pointalism_grid_init_type (void);

RigPointalismGrid *
rig_pointalism_grid_new (RutContext *ctx,
                         float size,
                         int tex_width,
                         int tex_height);

CoglPrimitive *
rig_pointalism_grid_get_primitive (RutObject *object);

RutMesh *
rig_pointalism_grid_get_pick_mesh (RutObject *self);

float
rig_pointalism_grid_get_scale (RutObject *obj);

void
rig_pointalism_grid_set_scale (RutObject *obj,
                               float scale);

float
rig_pointalism_grid_get_z (RutObject *obj);

void
rig_pointalism_grid_set_z (RutObject *obj,
                           float z);

bool
rig_pointalism_grid_get_lighter (RutObject *obj);

void
rig_pointalism_grid_set_lighter (RutObject *obj,
                                 bool lighter);

float
rig_pointalism_grid_get_cell_size (RutObject *obj);

void
rig_pointalism_grid_set_cell_size (RutObject *obj,
                                   float rows);

typedef void (* RigPointalismGridUpdateCallback) (RigPointalismGrid *grid,
                                                  void *user_data);
RutClosure *
rig_pointalism_grid_add_update_callback (RigPointalismGrid *grid,
                                         RigPointalismGridUpdateCallback callback,
                                         void *user_data,
                                         RutClosureDestroyCallback destroy_cb);

#endif /* __RIG_POINTALISM_GRID_H__ */
