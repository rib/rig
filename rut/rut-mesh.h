/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012 Intel Corporation.
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

#ifndef _RUT_MESH_H_
#define _RUT_MESH_H_

#include <clib.h>

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
  bool normalized;
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

typedef bool (*RutMeshVertexCallback) (void **attribute_data,
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

typedef bool (*RutMeshTriangleCallback) (void **attribute_data_v0,
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
