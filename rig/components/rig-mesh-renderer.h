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

#ifndef __RIG_MESH_RENDERER_H__
#define __RIG_MESH_RENDERER_H__

#include <stdint.h>

#include <cogl/cogl.h>

#include "rig-entity.h"
#include "mash-data-loader.h"

#define RIG_MESH_RENDERER(p) ((RigMeshRenderer *)(p))
typedef struct _RigMeshRenderer RigMeshRenderer;
extern RigType rig_mesh_renderer_type;

struct _RigMeshRenderer
{
  RigObjectProps _parent;
  RigComponentableProps component;
  CoglPrimitive *primitive;
  MashData *mesh_data;
  uint8_t *vertex_data;
  int      n_vertices;
  size_t   stride;
  CoglPipeline *pipeline_cache;
  int normal_matrix_uniform;
};

void
_rig_mesh_renderer_init_type (void);

RigMeshRenderer *
rig_mesh_renderer_new_from_file (RigContext *ctx,
                                 const char *file);

RigMeshRenderer *
rig_mesh_renderer_new_from_template (RigContext *ctx,
                                     const char *name);

void
rig_mesh_renderer_free (RigMeshRenderer *renderer);

void *
rig_mesh_renderer_get_vertex_data (RigMeshRenderer *renderer,
                                   size_t *stride,
                                   int *n_vertices);

int
rig_mesh_renderer_get_n_vertices (RigMeshRenderer *renderer);

CoglPrimitive *
rig_mesh_renderer_get_primitive (RigObject *object);

#endif /* __RIG_MESH_RENDERER_H__ */
