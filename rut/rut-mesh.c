/*
 * Rut
 *
 * Copyright (C) 2012,2013  Intel Corporation
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

#include <config.h>

#include "rut-mesh.h"
#include "rut-interfaces.h"

static void
_rut_buffer_free (void *object)
{
  RutBuffer *buffer = object;

  g_free (buffer->data);
  g_slice_free (RutBuffer, buffer);
}

static RutRefCountableVTable _rut_buffer_ref_countable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rut_buffer_free
};

RutType rut_buffer_type;

void
_rut_buffer_init_type (void)
{
  rut_type_init (&rut_buffer_type, "RigBuffer");
  rut_type_add_interface (&rut_buffer_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RutBuffer, ref_count),
                          &_rut_buffer_ref_countable_vtable);
}

RutBuffer *
rut_buffer_new (size_t buffer_size)
{
  RutBuffer *buffer = g_slice_new (RutBuffer);

  rut_object_init (&buffer->_parent, &rut_buffer_type);

  buffer->ref_count = 1;

  buffer->size = buffer_size;
  buffer->data = g_malloc (buffer_size);

  return buffer;
}

static void
_rut_attribute_free (RutAttribute *attribute)
{
  rut_refable_unref (attribute->buffer);
  g_slice_free (RutAttribute, attribute);
}

static RutRefCountableVTable _rut_attribute_ref_countable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rut_attribute_free
};

RutType rut_attribute_type;

void
_rut_attribute_init_type (void)
{
  rut_type_init (&rut_attribute_type, "RigAttribute");
  rut_type_add_interface (&rut_attribute_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RutAttribute, ref_count),
                          &_rut_attribute_ref_countable_vtable);
}

RutAttribute *
rut_attribute_new (RutBuffer *buffer,
                   const char *name,
                   size_t stride,
                   size_t offset,
                   int n_components,
                   RutAttributeType type)
{
  RutAttribute *attribute = g_slice_new0 (RutAttribute);

  rut_object_init (&attribute->_parent, &rut_attribute_type);

  attribute->ref_count = 1;
  attribute->buffer = rut_refable_ref (buffer);
  attribute->name = g_strdup (name);
  attribute->stride = stride;
  attribute->offset = offset;
  attribute->n_components = n_components;
  attribute->type = type;

  return attribute;
}

static void
_rut_mesh_free (RutMesh *mesh)
{
  int i;

  for (i = 0; i < mesh->n_attributes; i++)
    rut_refable_unref (mesh->attributes[i]);

  g_slice_free1 (mesh->n_attributes * sizeof (void *), mesh->attributes);
  g_slice_free (RutMesh, mesh);
}

static RutRefCountableVTable _rut_mesh_ref_countable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rut_mesh_free
};

RutType rut_mesh_type;

void
_rut_mesh_init_type (void)
{
  rut_type_init (&rut_mesh_type, "RigMesh");
  rut_type_add_interface (&rut_mesh_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RutMesh, ref_count),
                          &_rut_mesh_ref_countable_vtable);
}

RutMesh *
rut_mesh_new (CoglVerticesMode mode,
              int n_vertices,
              RutAttribute **attributes,
              int n_attributes)
{
  RutMesh *mesh = g_slice_new0 (RutMesh);
  RutAttribute **attributes_real =
    g_slice_copy (sizeof (void *) * n_attributes, attributes);
  int i;

  rut_object_init (&mesh->_parent, &rut_mesh_type);

  mesh->ref_count = 1;
  memcpy (attributes_real, attributes, sizeof (void *) * n_attributes);
  mesh->attributes = attributes_real;
  mesh->n_attributes = n_attributes;
  mesh->mode = mode;
  mesh->n_vertices = n_vertices;

  for (i = 0; i < n_attributes; i++)
    rut_refable_ref (attributes[i]);

  return mesh;
}

RutMesh *
rut_mesh_new_from_buffer_p3 (CoglVerticesMode mode,
                             int n_vertices,
                             RutBuffer *buffer)
{
  RutMesh *mesh = g_slice_new0 (RutMesh);
  int n_attributes = 1;
  RutAttribute **attributes = g_slice_alloc (sizeof (void *) * n_attributes);

  rut_object_init (&mesh->_parent, &rut_mesh_type);
  mesh->ref_count = 1;

  attributes[0] = rut_attribute_new (buffer,
                                     "cogl_position_in",
                                     sizeof (CoglVertexP3),
                                     offsetof (CoglVertexP3, x),
                                     3,
                                     RUT_ATTRIBUTE_TYPE_FLOAT);

  mesh->attributes = attributes;
  mesh->n_attributes = n_attributes;
  mesh->mode = mode;
  mesh->n_vertices = n_vertices;

  return mesh;
}

typedef struct _VertexP3N3
{
  float x, y, z, nx, ny, nz;
} VertexP3N3;

RutMesh *
rut_mesh_new_from_buffer_p3n3 (CoglVerticesMode mode,
                               int n_vertices,
                               RutBuffer *buffer)
{
  RutMesh *mesh = g_slice_new0 (RutMesh);
  int n_attributes = 2;
  RutAttribute **attributes = g_slice_alloc (sizeof (void *) * n_attributes);

  rut_object_init (&mesh->_parent, &rut_mesh_type);
  mesh->ref_count = 1;

  attributes[0] = rut_attribute_new (buffer,
                                     "cogl_position_in",
                                     sizeof (VertexP3N3),
                                     0,
                                     3,
                                     RUT_ATTRIBUTE_TYPE_FLOAT);
  attributes[1] = rut_attribute_new (buffer,
                                     "cogl_normal_in",
                                     sizeof (VertexP3N3),
                                     offsetof (VertexP3N3, nx),
                                     3,
                                     RUT_ATTRIBUTE_TYPE_FLOAT);

  mesh->attributes = attributes;
  mesh->n_attributes = n_attributes;
  mesh->mode = mode;
  mesh->n_vertices = n_vertices;

  return mesh;
}

RutMesh *
rut_mesh_new_from_buffer_p3c4 (CoglVerticesMode mode,
                               int n_vertices,
                               RutBuffer *buffer)
{
  RutMesh *mesh = g_slice_new0 (RutMesh);
  int n_attributes = 2;
  RutAttribute **attributes = g_slice_alloc (sizeof (void *) * n_attributes);

  rut_object_init (&mesh->_parent, &rut_mesh_type);
  mesh->ref_count = 1;

  attributes[0] = rut_attribute_new (buffer,
                                     "cogl_position_in",
                                     sizeof (CoglVertexP3C4),
                                     offsetof (CoglVertexP3C4, x),
                                     3,
                                     RUT_ATTRIBUTE_TYPE_FLOAT);
  attributes[1] = rut_attribute_new (buffer,
                                     "cogl_color_in",
                                     sizeof (CoglVertexP3C4),
                                     offsetof (CoglVertexP3C4, r),
                                     4,
                                     RUT_ATTRIBUTE_TYPE_UNSIGNED_BYTE);
  attributes[1]->normalized = TRUE;

  mesh->attributes = attributes;
  mesh->n_attributes = n_attributes;
  mesh->mode = mode;
  mesh->n_vertices = n_vertices;

  return mesh;
}

void
rut_mesh_set_indices (RutMesh *mesh,
                      CoglIndicesType type,
                      RutBuffer *buffer,
                      int n_indices)
{
  mesh->indices_buffer = rut_refable_ref (buffer);
  mesh->indices_type = type;
  mesh->n_indices = n_indices;
}

static void
foreach_vertex (RutMesh *mesh,
                RutMeshVertexCallback callback,
                void *user_data,
                CoglBool ignore_indices,
                uint8_t **bases,
                int *strides,
                int n_attributes)
{
  if (mesh->indices_buffer && !ignore_indices)
    {
      void *indices_data = mesh->indices_buffer->data;
      CoglIndicesType indices_type = mesh->indices_type;
      void **data;
      int i;
      int v;

      data = g_alloca (sizeof (void *) * n_attributes);

      for (i = 0; i < mesh->n_indices; i++)
        {
          int j;

          switch (indices_type)
            {
            case COGL_INDICES_TYPE_UNSIGNED_BYTE:
              v = ((uint8_t *)indices_data)[i];
              break;
            case COGL_INDICES_TYPE_UNSIGNED_SHORT:
              v = ((uint16_t *)indices_data)[i];
              break;
            case COGL_INDICES_TYPE_UNSIGNED_INT:
              v = ((uint32_t *)indices_data)[i];
              break;
            }

          for (j = 0; j < n_attributes; j++)
            data[j] = bases[j] + v * strides[j];

          callback (data, v, user_data);
        }
    }
  else
    {
      int i;

      for (i = 0; i < mesh->n_vertices; i++)
        {
          int j;

          callback (bases, i, user_data);

          for (j = 0; j < n_attributes; j++)
            bases[j] += strides[j];
        }
    }
}

static CoglBool
collect_attribute_state (RutMesh *mesh,
                         uint8_t **bases,
                         int *strides,
                         const char *first_attribute,
                         va_list ap)
{
  const char *attribute_name = first_attribute;
  int i;

  for (i = 0; attribute_name; i++)
    {
      int j;

      for (j = 0; j < mesh->n_attributes; j++)
        {
          if (strcmp (mesh->attributes[j]->name, attribute_name) == 0)
            {
              bases[i] = mesh->attributes[j]->buffer->data + mesh->attributes[j]->offset;
              strides[i] = mesh->attributes[j]->stride;
              break;
            }
        }

      if (j == mesh->n_attributes)
        return FALSE;

      attribute_name = va_arg (ap, const char *);
    }

  return TRUE;
}

void
rut_mesh_foreach_vertex (RutMesh *mesh,
                         RutMeshVertexCallback callback,
                         void *user_data,
                         const char *first_attribute,
                         ...)
{
  va_list ap;
  int n_attributes = 0;
  uint8_t **bases;
  int *strides;

  va_start (ap, first_attribute);
  do {
    n_attributes++;
  } while (va_arg (ap, const char *));
  va_end (ap);

  bases = g_alloca (sizeof (void *) * n_attributes);
  strides = g_alloca (sizeof (int) * n_attributes);

  va_start (ap, first_attribute);
  collect_attribute_state (mesh, bases, strides, first_attribute, ap);
  va_end (ap);

  foreach_vertex (mesh, callback, user_data, FALSE,
                  bases, strides, n_attributes);
}

void
rut_mesh_foreach_index (RutMesh *mesh,
                        RutMeshVertexCallback callback,
                        void *user_data,
                        const char *first_attribute,
                        ...)
{
  va_list ap;
  int n_attributes = 0;
  uint8_t **bases;
  int *strides;

  va_start (ap, first_attribute);
  do {
    n_attributes++;
  } while (va_arg (ap, const char *));
  va_end (ap);

  bases = g_alloca (sizeof (void *) * n_attributes);
  strides = g_alloca (sizeof (int) * n_attributes);

  va_start (ap, first_attribute);
  collect_attribute_state (mesh, bases, strides, first_attribute, ap);
  va_end (ap);

  foreach_vertex (mesh, callback, user_data, TRUE,
                  bases, strides, n_attributes);
}

typedef struct _IndexState
{
  int n_attributes;
  uint8_t **bases;
  int *strides;
  int stride;
  CoglIndicesType indices_type;
  void *indices;
} IndexState;

static inline int
move_to_i (int index,
           IndexState *state,
           void **data)
{
  uint32_t v;
  int i;

  switch (state->indices_type)
    {
    case COGL_INDICES_TYPE_UNSIGNED_BYTE:
      v = ((uint8_t *)state->indices)[index];
      break;
    case COGL_INDICES_TYPE_UNSIGNED_SHORT:
      v = ((uint16_t *)state->indices)[index];
      break;
    case COGL_INDICES_TYPE_UNSIGNED_INT:
      v = ((uint32_t *)state->indices)[index];
      break;
    }

  for (i = 0; i < state->n_attributes; i++)
    data[i] = state->bases[i] + v * state->strides[i];

  return v;
}

static inline int
move_to (int index,
         int n_attributes,
         uint8_t **bases,
         int *strides,
         void **data)
{
  int i;
  for (i = 0; i < n_attributes; i++)
    data[i] = bases[i] + index * strides[i];
  return index;
}

void
rut_mesh_foreach_triangle (RutMesh *mesh,
                           RutMeshTriangleCallback callback,
                           void *user_data,
                           const char *first_attribute,
                           ...)
{
  va_list ap;
  int n_vertices;
  int n_attributes = 0;
  uint8_t **bases;
  int *strides;
  void **data0;
  void **data1;
  void **data2;
  void **tri_v[3];
  int tri_i[3];

  switch (mesh->mode)
    {
    case COGL_VERTICES_MODE_LINES:
    case COGL_VERTICES_MODE_LINE_STRIP:
    case COGL_VERTICES_MODE_LINE_LOOP:
      return;
    default:
      break;
    }

  n_vertices = mesh->indices_buffer ? mesh->n_indices : mesh->n_vertices;
  if (n_vertices < 3)
    return;

  va_start (ap, first_attribute);
  do {
    n_attributes++;
  } while (va_arg (ap, const char *));
  va_end (ap);

  bases = g_alloca (sizeof (void *) * n_attributes);
  strides = g_alloca (sizeof (int) * n_attributes);
  data0 = g_alloca (sizeof (void *) * n_attributes);
  data1 = g_alloca (sizeof (void *) * n_attributes);
  data2 = g_alloca (sizeof (void *) * n_attributes);

  tri_v[0] = data0;
  tri_v[1] = data1;
  tri_v[2] = data2;

  va_start (ap, first_attribute);
  collect_attribute_state (mesh, bases, strides, first_attribute, ap);
  va_end (ap);

#define SWAP_TRIANGLE_VERTICES(V0, V1) \
  do { \
    void **tmp_v; \
    int tmp_i; \
    tmp_v = tri_v[V0]; \
    tri_v[V0] = tri_v[V1]; \
    tri_v[V1] = tmp_v; \
    \
    tmp_i = tri_i[V0]; \
    tri_i[V0] = tri_i[V1]; \
    tri_i[V1] = tmp_i; \
  } while (1)

  /* Make sure we don't overrun the vertices if we don't have a
   * multiple of three vertices in triangle list mode */
  if (mesh->mode == COGL_VERTICES_MODE_TRIANGLES)
    n_vertices -= 2;

  if (mesh->indices_buffer)
    {
      IndexState state;
      CoglVerticesMode mode = mesh->mode;
      int i = 0;

      state.n_attributes = n_attributes;
      state.bases = bases;
      state.strides = strides;
      state.indices_type = mesh->indices_type;
      state.indices = mesh->indices_buffer->data;

      tri_i[0] = move_to_i (i++, &state, tri_v[0]);
      tri_i[1] = move_to_i (i++, &state, tri_v[1]);
      tri_i[2] = move_to_i (i++, &state, tri_v[2]);

      while (TRUE)
        {
          if (!callback (tri_v[0], tri_v[1], tri_v[2],
                         tri_i[0], tri_i[1], tri_i[2], user_data))
            return;

          if (i >= n_vertices)
            break;

          switch (mode)
            {
            case COGL_VERTICES_MODE_TRIANGLES:
              tri_i[0] = move_to_i (i++, &state, tri_v[0]);
              tri_i[1] = move_to_i (i++, &state, tri_v[1]);
              tri_i[2] = move_to_i (i++, &state, tri_v[2]);
              break;
            case COGL_VERTICES_MODE_TRIANGLE_FAN:
              SWAP_TRIANGLE_VERTICES (1, 2);
              tri_i[2] = move_to_i (i++, &state, tri_v[2]);
              break;
            case COGL_VERTICES_MODE_TRIANGLE_STRIP:
              SWAP_TRIANGLE_VERTICES (0, 1);
              SWAP_TRIANGLE_VERTICES (1, 2);
              tri_i[2] = move_to_i (i++, &state, tri_v[2]);
              break;
            default:
              g_warn_if_reached ();
            }
        }
    }
  else
    {
      CoglVerticesMode mode = mesh->mode;
      int i = 0;

      tri_i[0] = move_to (i++, n_attributes, bases, strides, tri_v[0]);
      tri_i[1] = move_to (i++, n_attributes, bases, strides, tri_v[1]);
      tri_i[2] = move_to (i++, n_attributes, bases, strides, tri_v[2]);

      while (TRUE)
        {
          if (!callback (tri_v[0], tri_v[1], tri_v[2],
                         tri_i[0], tri_i[1], tri_i[2], user_data))
            return;

          if (i >= n_vertices)
            break;

          switch (mode)
            {
            case COGL_VERTICES_MODE_TRIANGLES:
              tri_i[0] = move_to (i++, n_attributes, bases, strides, tri_v[0]);
              tri_i[1] = move_to (i++, n_attributes, bases, strides, tri_v[1]);
              tri_i[2] = move_to (i++, n_attributes, bases, strides, tri_v[2]);
              break;
            case COGL_VERTICES_MODE_TRIANGLE_FAN:
              SWAP_TRIANGLE_VERTICES (1, 2);
              tri_i[2] = move_to (i++, n_attributes, bases, strides, tri_v[2]);
              break;
            case COGL_VERTICES_MODE_TRIANGLE_STRIP:
              SWAP_TRIANGLE_VERTICES (0, 1);
              SWAP_TRIANGLE_VERTICES (1, 2);
              tri_i[2] = move_to (i++, n_attributes, bases, strides, tri_v[2]);
              break;
            default:
              g_warn_if_reached ();
            }
        }
    }
#undef SWAP_TRIANGLE_VERTICES
}

static CoglAttributeType
get_cogl_attribute_type (RutAttributeType type)
{
  switch (type)
    {
    case RUT_ATTRIBUTE_TYPE_BYTE:
      return COGL_ATTRIBUTE_TYPE_BYTE;
    case RUT_ATTRIBUTE_TYPE_UNSIGNED_BYTE:
      return COGL_ATTRIBUTE_TYPE_UNSIGNED_BYTE;
    case RUT_ATTRIBUTE_TYPE_SHORT:
      return COGL_ATTRIBUTE_TYPE_SHORT;
    case RUT_ATTRIBUTE_TYPE_UNSIGNED_SHORT:
      return COGL_ATTRIBUTE_TYPE_UNSIGNED_SHORT;
    case RUT_ATTRIBUTE_TYPE_FLOAT:
      return COGL_ATTRIBUTE_TYPE_FLOAT;
    }

  g_warn_if_reached ();
  return 0;
}

CoglPrimitive *
rut_mesh_create_primitive (RutContext *ctx,
                           RutMesh *mesh)
{
  RutBuffer **buffers;
  int n_buffers = 0;
  CoglAttributeBuffer **attribute_buffers;
  CoglAttributeBuffer **attribute_buffers_map;
  CoglAttribute **attributes;
  CoglPrimitive *primitive;
  int i;

  buffers = g_alloca (sizeof (void *) * mesh->n_attributes);
  attribute_buffers = g_alloca (sizeof (void *) * mesh->n_attributes);
  attribute_buffers_map = g_alloca (sizeof (void *) * mesh->n_attributes);

  /* NB:
   * attributes may refer to shared buffers so we need to first
   * figure out how many unique buffers the mesh refers too...
   */

  for (i = 0; i < mesh->n_attributes; i++)
    {
      int j;

      for (j = 0; i < n_buffers; j++)
        if (buffers[j] == mesh->attributes[i]->buffer)
          break;

      if (j < n_buffers)
        attribute_buffers_map[i] = attribute_buffers[j];
      else
        {
          attribute_buffers[n_buffers] =
            cogl_attribute_buffer_new (ctx->cogl_context,
                                       mesh->attributes[i]->buffer->size,
                                       mesh->attributes[i]->buffer->data);

          attribute_buffers_map[i] = attribute_buffers[n_buffers];
          buffers[n_buffers++] = mesh->attributes[i]->buffer;
        }
    }

  attributes = g_alloca (sizeof (void *) * mesh->n_attributes);
  for (i = 0; i < mesh->n_attributes; i++)
    {
      CoglAttributeType type =
        get_cogl_attribute_type (mesh->attributes[i]->type);

      attributes[i] = cogl_attribute_new (attribute_buffers_map[i],
                                          mesh->attributes[i]->name,
                                          mesh->attributes[i]->stride,
                                          mesh->attributes[i]->offset,
                                          mesh->attributes[i]->n_components,
                                          type);
    }

  for (i = 0; i < n_buffers; i++)
    cogl_object_unref (attribute_buffers[i]);

  primitive = cogl_primitive_new_with_attributes (mesh->mode,
                                                  mesh->n_vertices,
                                                  attributes,
                                                  mesh->n_attributes);

  for (i = 0; i < mesh->n_attributes; i++)
    cogl_object_unref (attributes[i]);

  if (mesh->indices_buffer)
    {
      CoglIndices *indices = cogl_indices_new (ctx->cogl_context,
                                               mesh->indices_type,
                                               mesh->indices_buffer->data,
                                               mesh->n_indices);
      cogl_primitive_set_indices (primitive, indices, mesh->n_indices);
    }

  return primitive;
}

RutAttribute *
rut_mesh_find_attribute (RutMesh *mesh,
                         const char *attribute_name)
{
  int i;

  for (i = 0; i < mesh->n_attributes; i++)
    {
      RutAttribute *attribute = mesh->attributes[i];
      if (strcmp (attribute->name, attribute_name) == 0)
        return attribute;
    }
  return NULL;
}
