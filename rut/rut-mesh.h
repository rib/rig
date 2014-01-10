/*
 * Rut
 *
 * Copyright (C) 2012 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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

#include <glib.h>

#include <cogl/cogl.h>

typedef struct _RutBuffer RutBuffer;
typedef struct _RutAttribute RutAttribute;
typedef struct _RutMesh RutMesh;

#include "rut-context.h"
#include "rut-list.h"

typedef struct _RutEditAttribute
{
  int name_id;
  float value;
} RutEditAttribute;

typedef struct _RutEditVertex
{
  RutList attributes;
} RutEditVertex;

/* This kind of mesh is optimized for random editing */
typedef struct _RutEditMesh
{
  RutList vertices;
} RutEditMesh;

typedef enum {
  RUT_ATTRIBUTE_TYPE_BYTE,
  RUT_ATTRIBUTE_TYPE_UNSIGNED_BYTE,
  RUT_ATTRIBUTE_TYPE_SHORT,
  RUT_ATTRIBUTE_TYPE_UNSIGNED_SHORT,
  RUT_ATTRIBUTE_TYPE_FLOAT
} RutAttributeType;

extern RutType rut_buffer_type;

struct _RutBuffer
{
  RutObjectBase _base;


  uint8_t *data;
  size_t size;
};

struct _RutAttribute
{
  RutObjectBase _base;

  RutBuffer *buffer;
  const char *name;
  CoglBool normalized;
  size_t stride;
  size_t offset;
  int n_components;
  RutAttributeType type;
};

extern RutType rut_mesh_type;

/* This kind of mesh is optimized for size and use by a GPU */
struct _RutMesh
{
  RutObjectBase _base;


  CoglVerticesMode mode;
  RutAttribute **attributes;
  int n_attributes;

  /* NB: this always represents the amount of actual data so
   * you need to consider if indices != NULL then n_indices
   * says how many vertices should be drawn, including duplicates.
   */
  int n_vertices;

  /* optional */
  CoglIndicesType indices_type;
  int n_indices;
  RutBuffer *indices_buffer;
};

void
_rut_buffer_init_type (void);

RutBuffer *
rut_buffer_new (size_t buffer_size);

void
_rut_attribute_init_type (void);

RutAttribute *
rut_attribute_new (RutBuffer *buffer,
                   const char *name,
                   size_t stride,
                   size_t offset,
                   int n_components,
                   RutAttributeType type);

void
rut_attribute_set_normalized (RutAttribute *attribute,
                              bool normalized);

void
_rut_mesh_init_type (void);

RutMesh *
rut_mesh_new (CoglVerticesMode mode,
              int n_vertices,
              RutAttribute **attributes,
              int n_attributes);

RutMesh *
rut_mesh_new_from_buffer_p3 (CoglVerticesMode mode,
                             int n_vertices,
                             RutBuffer *buffer);

RutMesh *
rut_mesh_new_from_buffer_p3n3 (CoglVerticesMode mode,
                               int n_vertices,
                               RutBuffer *buffer);

RutMesh *
rut_mesh_new_from_buffer_p3c4 (CoglVerticesMode mode,
                               int n_vertices,
                               RutBuffer *buffer);

void
rut_mesh_set_attributes (RutMesh *mesh,
                         RutAttribute **attributes,
                         int n_attributes);

void
rut_mesh_set_indices (RutMesh *mesh,
                      CoglIndicesType type,
                      RutBuffer *buffer,
                      int n_indices);

/* Performs a deep copy of all the buffers */
RutMesh *
rut_mesh_copy (RutMesh *mesh);

CoglPrimitive *
rut_mesh_create_primitive (RutContext *ctx,
                           RutMesh *mesh);

RutAttribute *
rut_mesh_find_attribute (RutMesh *mesh,
                         const char *attribute_name);

typedef CoglBool (*RutMeshVertexCallback) (void **attribute_data,
                                           int vertex_index,
                                           void *user_data);

void
rut_mesh_foreach_vertex (RutMesh *mesh,
                         RutMeshVertexCallback callback,
                         void *user_data,
                         const char *first_attribute_name,
                         ...); G_GNUC_NULL_TERMINATED

void
rut_mesh_foreach_index (RutMesh *mesh,
                        RutMeshVertexCallback callback,
                        void *user_data,
                        const char *first_attribute_name,
                        ...) G_GNUC_NULL_TERMINATED;

typedef CoglBool (*RutMeshTriangleCallback) (void **attribute_data_v0,
                                             void **attribute_data_v1,
                                             void **attribute_data_v2,
                                             int v0_index,
                                             int v1_index,
                                             int v2_index,
                                             void *user_data);

void
rut_mesh_foreach_triangle (RutMesh *mesh,
                           RutMeshTriangleCallback callback,
                           void *user_data,
                           const char *first_attribute,
                           ...) G_GNUC_NULL_TERMINATED;

#endif /* _RUT_MESH_H_ */
