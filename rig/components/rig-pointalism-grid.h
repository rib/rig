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
