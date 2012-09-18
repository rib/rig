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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef _RIG_MESH_H_
#define _RIG_MESH_H_

#include <stdint.h>

#include <cogl/cogl.h>

#include "rig-entity.h"
#include "mash-data-loader.h"

#define RIG_MESH(p) ((RigMesh *)(p))
typedef struct _RigMesh RigMesh;
extern RigType rig_mesh_type;

typedef enum _RigMeshType
{
  RIG_MESH_TYPE_TEMPLATE,
  RIG_MESH_TYPE_FILE,
} RigMeshType;

struct _RigMesh
{
  RigObjectProps _parent;
  RigComponentableProps component;

  RigMeshType type;
  char *path;

  MashData *mesh_data;
  uint8_t *vertex_data;
  int n_vertices;
  size_t stride;

  CoglPrimitive *primitive;

  CoglPipeline *pipeline_cache;
  int normal_matrix_uniform;
};

void
_rig_mesh_init_type (void);

RigMesh *
rig_mesh_new_from_file (RigContext *ctx,
                        const char *file);

RigMesh *
rig_mesh_new_from_template (RigContext *ctx,
                            const char *name);

void
rig_mesh_free (RigMesh *renderer);

void *
rig_mesh_get_vertex_data (RigMesh *renderer,
                          size_t *stride,
                          int *n_vertices);

int
rig_mesh_get_n_vertices (RigMesh *renderer);

CoglPrimitive *
rig_mesh_get_primitive (RigObject *object);

RigMeshType
rig_mesh_get_type (RigMesh *renderer);

const char *
rig_mesh_get_path (RigMesh *renderer);

#endif /* _RIG_MESH_H_ */
