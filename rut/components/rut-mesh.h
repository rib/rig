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

#ifndef _RUT_MESH_H_
#define _RUT_MESH_H_

#include <stdint.h>

#include <cogl/cogl.h>

#include "rut-entity.h"
#include "mash-data-loader.h"

#define RUT_MESH(p) ((RutMesh *)(p))
typedef struct _RutMesh RutMesh;
extern RutType rut_mesh_type;

typedef enum _RutMeshType
{
  RUT_MESH_TYPE_TEMPLATE,
  RUT_MESH_TYPE_FILE,
} RutMeshType;

struct _RutMesh
{
  RutObjectProps _parent;
  RutComponentableProps component;

  RutMeshType type;
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
_rut_mesh_init_type (void);

RutMesh *
rut_mesh_new_from_file (RutContext *ctx,
                        const char *file);

RutMesh *
rut_mesh_new_from_template (RutContext *ctx,
                            const char *name);

void
rut_mesh_free (RutMesh *renderer);

void *
rut_mesh_get_vertex_data (RutMesh *renderer,
                          size_t *stride,
                          int *n_vertices);

int
rut_mesh_get_n_vertices (RutMesh *renderer);

CoglPrimitive *
rut_mesh_get_primitive (RutObject *object);

RutMeshType
rut_mesh_get_type (RutMesh *renderer);

const char *
rut_mesh_get_path (RutMesh *renderer);

#endif /* _RUT_MESH_H_ */
