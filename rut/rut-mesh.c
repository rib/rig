/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012,2013  Intel Corporation
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

#include <config.h>

#include "rut-mesh.h"
#include "rut-interfaces.h"

static void
_rut_buffer_free (void *object)
{
  RutBuffer *buffer = object;

  c_free (buffer->data);
  rut_object_free (RutBuffer, buffer);
}

RutType rut_buffer_type;

void
_rut_buffer_init_type (void)
{
  rut_type_init (&rut_buffer_type, "RutBuffer", _rut_buffer_free);
}

RutBuffer *
rut_buffer_new (size_t buffer_size)
{
  RutBuffer *buffer =
    rut_object_alloc0 (RutBuffer, &rut_buffer_type, _rut_buffer_init_type);



  buffer->size = buffer_size;
  buffer->data = c_malloc (buffer_size);

  return buffer;
}

static void
_rut_attribute_free (RutObject *object)
{
  RutAttribute *attribute = object;
  rut_object_unref (attribute->buffer);
  rut_object_free (RutAttribute, attribute);
}

RutType rut_attribute_type;

void
_rut_attribute_init_type (void)
{
  rut_type_init (&rut_attribute_type, "RutAttribute", _rut_attribute_free);
}

RutAttribute *
rut_attribute_new (RutBuffer *buffer,
                   const char *name,
                   size_t stride,
                   size_t offset,
                   int n_components,
                   RutAttributeType type)
{
  RutAttribute *attribute =
    rut_object_alloc0 (RutAttribute, &rut_attribute_type, _rut_attribute_init_type);


  attribute->buffer = rut_object_ref (buffer);
  attribute->name = c_strdup (name);
  attribute->stride = stride;
  attribute->offset = offset;
  attribute->n_components = n_components;
  attribute->type = type;

  return attribute;
}

void
rut_attribute_set_normalized (RutAttribute *attribute,
                              bool normalized)
{
  attribute->normalized = normalized;
}

static void
_rut_mesh_free (RutObject *object)
{
  RutMesh *mesh = object;
  int i;

  for (i = 0; i < mesh->n_attributes; i++)
    rut_object_unref (mesh->attributes[i]);

  c_slice_free1 (mesh->n_attributes * sizeof (void *), mesh->attributes);
  rut_object_free (RutMesh, mesh);
}

RutType rut_mesh_type;

void
_rut_mesh_init_type (void)
{
  rut_type_init (&rut_mesh_type, "RutMesh", _rut_mesh_free);
}

RutMesh *
rut_mesh_new (cg_vertices_mode_t mode,
              int n_vertices,
              RutAttribute **attributes,
              int n_attributes)
{
  RutMesh *mesh =
    rut_object_alloc0 (RutMesh, &rut_mesh_type, _rut_mesh_init_type);


  mesh->mode = mode;
  mesh->n_vertices = n_vertices;

  rut_mesh_set_attributes (mesh, attributes, n_attributes);

  return mesh;
}

RutMesh *
rut_mesh_new_from_buffer_p3 (cg_vertices_mode_t mode,
                             int n_vertices,
                             RutBuffer *buffer)
{
  RutMesh *mesh =
    rut_object_alloc0 (RutMesh, &rut_mesh_type, _rut_mesh_init_type);
  int n_attributes = 1;
  RutAttribute **attributes = c_slice_alloc (sizeof (void *) * n_attributes);


  attributes[0] = rut_attribute_new (buffer,
                                     "cg_position_in",
                                     sizeof (cg_vertex_p3_t),
                                     offsetof (cg_vertex_p3_t, x),
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
rut_mesh_new_from_buffer_p3n3 (cg_vertices_mode_t mode,
                               int n_vertices,
                               RutBuffer *buffer)
{
  RutMesh *mesh =
    rut_object_alloc0 (RutMesh, &rut_mesh_type, _rut_mesh_init_type);
  int n_attributes = 2;
  RutAttribute **attributes = c_slice_alloc (sizeof (void *) * n_attributes);


  attributes[0] = rut_attribute_new (buffer,
                                     "cg_position_in",
                                     sizeof (VertexP3N3),
                                     0,
                                     3,
                                     RUT_ATTRIBUTE_TYPE_FLOAT);
  attributes[1] = rut_attribute_new (buffer,
                                     "cg_normal_in",
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
rut_mesh_new_from_buffer_p3c4 (cg_vertices_mode_t mode,
                               int n_vertices,
                               RutBuffer *buffer)
{
  RutMesh *mesh =
    rut_object_alloc0 (RutMesh, &rut_mesh_type, _rut_mesh_init_type);
  int n_attributes = 2;
  RutAttribute **attributes = c_slice_alloc (sizeof (void *) * n_attributes);


  attributes[0] = rut_attribute_new (buffer,
                                     "cg_position_in",
                                     sizeof (cg_vertex_p3c4_t),
                                     offsetof (cg_vertex_p3c4_t, x),
                                     3,
                                     RUT_ATTRIBUTE_TYPE_FLOAT);
  attributes[1] = rut_attribute_new (buffer,
                                     "cg_color_in",
                                     sizeof (cg_vertex_p3c4_t),
                                     offsetof (cg_vertex_p3c4_t, r),
                                     4,
                                     RUT_ATTRIBUTE_TYPE_UNSIGNED_BYTE);
  attributes[1]->normalized = true;

  mesh->attributes = attributes;
  mesh->n_attributes = n_attributes;
  mesh->mode = mode;
  mesh->n_vertices = n_vertices;

  return mesh;
}

void
rut_mesh_set_indices (RutMesh *mesh,
                      cg_indices_type_t type,
                      RutBuffer *buffer,
                      int n_indices)
{
  mesh->indices_buffer = rut_object_ref (buffer);
  mesh->indices_type = type;
  mesh->n_indices = n_indices;
}

void
rut_mesh_set_attributes (RutMesh *mesh,
                         RutAttribute **attributes,
                         int n_attributes)
{
  RutAttribute **attributes_real =
    c_slice_copy (sizeof (void *) * n_attributes, attributes);
  int i;

  /* NB: some of the given attributes may be the same as
   * some of the current attributes so we ref before
   * unrefing to avoid destroying any of them. */
  for (i = 0; i < n_attributes; i++)
    rut_object_ref (attributes[i]);

  for (i = 0; i < mesh->n_attributes; i++)
    rut_object_unref (mesh->attributes[i]);

  mesh->attributes = attributes_real;
  mesh->n_attributes = n_attributes;
}

static void
foreach_vertex (RutMesh *mesh,
                RutMeshVertexCallback callback,
                void *user_data,
                bool ignore_indices,
                uint8_t **bases,
                int *strides,
                int n_attributes)
{
  if (mesh->indices_buffer && !ignore_indices)
    {
      void *indices_data = mesh->indices_buffer->data;
      cg_indices_type_t indices_type = mesh->indices_type;
      void **data;
      int i;
      int v;

      data = g_alloca (sizeof (void *) * n_attributes);

      for (i = 0; i < mesh->n_indices; i++)
        {
          int j;

          switch (indices_type)
            {
            case CG_INDICES_TYPE_UNSIGNED_BYTE:
              v = ((uint8_t *)indices_data)[i];
              break;
            case CG_INDICES_TYPE_UNSIGNED_SHORT:
              v = ((uint16_t *)indices_data)[i];
              break;
            case CG_INDICES_TYPE_UNSIGNED_INT:
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

          callback ( (void **)bases, i, user_data);

          for (j = 0; j < n_attributes; j++)
            bases[j] += strides[j];
        }
    }
}

static bool
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
        return false;

      attribute_name = va_arg (ap, const char *);
    }

  return true;
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
  bool ready;

  va_start (ap, first_attribute);
  do {
    n_attributes++;
  } while (va_arg (ap, const char *));
  va_end (ap);

  bases = g_alloca (sizeof (void *) * n_attributes);
  strides = g_alloca (sizeof (int) * n_attributes);

  va_start (ap, first_attribute);
  ready = collect_attribute_state (mesh, bases, strides, first_attribute, ap);
  va_end (ap);

  c_return_if_fail (ready);

  foreach_vertex (mesh, callback, user_data, false,
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
  bool ready;

  va_start (ap, first_attribute);
  do {
    n_attributes++;
  } while (va_arg (ap, const char *));
  va_end (ap);

  bases = g_alloca (sizeof (void *) * n_attributes);
  strides = g_alloca (sizeof (int) * n_attributes);

  va_start (ap, first_attribute);
  ready = collect_attribute_state (mesh, bases, strides, first_attribute, ap);
  va_end (ap);

  c_return_if_fail (ready);

  foreach_vertex (mesh, callback, user_data, true,
                  bases, strides, n_attributes);
}

typedef struct _IndexState
{
  int n_attributes;
  uint8_t **bases;
  int *strides;
  int stride;
  cg_indices_type_t indices_type;
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
    case CG_INDICES_TYPE_UNSIGNED_BYTE:
      v = ((uint8_t *)state->indices)[index];
      break;
    case CG_INDICES_TYPE_UNSIGNED_SHORT:
      v = ((uint16_t *)state->indices)[index];
      break;
    case CG_INDICES_TYPE_UNSIGNED_INT:
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
  bool ready;

  switch (mesh->mode)
    {
    case CG_VERTICES_MODE_LINES:
    case CG_VERTICES_MODE_LINE_STRIP:
    case CG_VERTICES_MODE_LINE_LOOP:
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
  ready = collect_attribute_state (mesh, bases, strides, first_attribute, ap);
  va_end (ap);

  c_return_if_fail (ready);

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
  if (mesh->mode == CG_VERTICES_MODE_TRIANGLES)
    n_vertices -= 2;

  if (mesh->indices_buffer)
    {
      IndexState state;
      cg_vertices_mode_t mode = mesh->mode;
      int i = 0;

      state.n_attributes = n_attributes;
      state.bases = bases;
      state.strides = strides;
      state.indices_type = mesh->indices_type;
      state.indices = mesh->indices_buffer->data;

      tri_i[0] = move_to_i (i++, &state, tri_v[0]);
      tri_i[1] = move_to_i (i++, &state, tri_v[1]);
      tri_i[2] = move_to_i (i++, &state, tri_v[2]);

      while (true)
        {
          if (!callback (tri_v[0], tri_v[1], tri_v[2],
                         tri_i[0], tri_i[1], tri_i[2], user_data))
            return;

          if (i >= n_vertices)
            break;

          switch (mode)
            {
            case CG_VERTICES_MODE_TRIANGLES:
              tri_i[0] = move_to_i (i++, &state, tri_v[0]);
              tri_i[1] = move_to_i (i++, &state, tri_v[1]);
              tri_i[2] = move_to_i (i++, &state, tri_v[2]);
              break;
            case CG_VERTICES_MODE_TRIANGLE_FAN:
              SWAP_TRIANGLE_VERTICES (1, 2);
              tri_i[2] = move_to_i (i++, &state, tri_v[2]);
              break;
            case CG_VERTICES_MODE_TRIANGLE_STRIP:
              SWAP_TRIANGLE_VERTICES (0, 1);
              SWAP_TRIANGLE_VERTICES (1, 2);
              tri_i[2] = move_to_i (i++, &state, tri_v[2]);
              break;
            default:
              c_warn_if_reached ();
            }
        }
    }
  else
    {
      cg_vertices_mode_t mode = mesh->mode;
      int i = 0;

      tri_i[0] = move_to (i++, n_attributes, bases, strides, tri_v[0]);
      tri_i[1] = move_to (i++, n_attributes, bases, strides, tri_v[1]);
      tri_i[2] = move_to (i++, n_attributes, bases, strides, tri_v[2]);

      while (true)
        {
          if (!callback (tri_v[0], tri_v[1], tri_v[2],
                         tri_i[0], tri_i[1], tri_i[2], user_data))
            return;

          if (i >= n_vertices)
            break;

          switch (mode)
            {
            case CG_VERTICES_MODE_TRIANGLES:
              tri_i[0] = move_to (i++, n_attributes, bases, strides, tri_v[0]);
              tri_i[1] = move_to (i++, n_attributes, bases, strides, tri_v[1]);
              tri_i[2] = move_to (i++, n_attributes, bases, strides, tri_v[2]);
              break;
            case CG_VERTICES_MODE_TRIANGLE_FAN:
              SWAP_TRIANGLE_VERTICES (1, 2);
              tri_i[2] = move_to (i++, n_attributes, bases, strides, tri_v[2]);
              break;
            case CG_VERTICES_MODE_TRIANGLE_STRIP:
              SWAP_TRIANGLE_VERTICES (0, 1);
              SWAP_TRIANGLE_VERTICES (1, 2);
              tri_i[2] = move_to (i++, n_attributes, bases, strides, tri_v[2]);
              break;
            default:
              c_warn_if_reached ();
            }
        }
    }
#undef SWAP_TRIANGLE_VERTICES
}

static cg_attribute_type_t
get_cg_attribute_type (RutAttributeType type)
{
  switch (type)
    {
    case RUT_ATTRIBUTE_TYPE_BYTE:
      return CG_ATTRIBUTE_TYPE_BYTE;
    case RUT_ATTRIBUTE_TYPE_UNSIGNED_BYTE:
      return CG_ATTRIBUTE_TYPE_UNSIGNED_BYTE;
    case RUT_ATTRIBUTE_TYPE_SHORT:
      return CG_ATTRIBUTE_TYPE_SHORT;
    case RUT_ATTRIBUTE_TYPE_UNSIGNED_SHORT:
      return CG_ATTRIBUTE_TYPE_UNSIGNED_SHORT;
    case RUT_ATTRIBUTE_TYPE_FLOAT:
      return CG_ATTRIBUTE_TYPE_FLOAT;
    }

  c_warn_if_reached ();
  return 0;
}

cg_primitive_t *
rut_mesh_create_primitive (RutContext *ctx,
                           RutMesh *mesh)
{
  RutBuffer **buffers;
  int n_buffers = 0;
  cg_attribute_buffer_t **attribute_buffers;
  cg_attribute_buffer_t **attribute_buffers_map;
  cg_attribute_t **attributes;
  cg_primitive_t *primitive;
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
            cg_attribute_buffer_new (ctx->cg_context,
                                       mesh->attributes[i]->buffer->size,
                                       mesh->attributes[i]->buffer->data);

          attribute_buffers_map[i] = attribute_buffers[n_buffers];
          buffers[n_buffers++] = mesh->attributes[i]->buffer;
        }
    }

  attributes = g_alloca (sizeof (void *) * mesh->n_attributes);
  for (i = 0; i < mesh->n_attributes; i++)
    {
      cg_attribute_type_t type =
        get_cg_attribute_type (mesh->attributes[i]->type);

      attributes[i] = cg_attribute_new (attribute_buffers_map[i],
                                          mesh->attributes[i]->name,
                                          mesh->attributes[i]->stride,
                                          mesh->attributes[i]->offset,
                                          mesh->attributes[i]->n_components,
                                          type);

      if (mesh->attributes[i]->normalized)
        cg_attribute_set_normalized (attributes[i], true);
    }

  for (i = 0; i < n_buffers; i++)
    cg_object_unref (attribute_buffers[i]);

  primitive = cg_primitive_new_with_attributes (mesh->mode,
                                                  mesh->n_vertices,
                                                  attributes,
                                                  mesh->n_attributes);

  for (i = 0; i < mesh->n_attributes; i++)
    cg_object_unref (attributes[i]);

  if (mesh->indices_buffer)
    {
      cg_indices_t *indices = cg_indices_new (ctx->cg_context,
                                               mesh->indices_type,
                                               mesh->indices_buffer->data,
                                               mesh->n_indices);
      cg_primitive_set_indices (primitive, indices, mesh->n_indices);
      cg_object_unref (indices);
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

RutMesh *
rut_mesh_copy (RutMesh *mesh)
{
  RutBuffer **buffers;
  int n_buffers = 0;
  RutBuffer **attribute_buffers;
  RutBuffer **attribute_buffers_map;
  RutAttribute **attributes;
  RutMesh *copy;
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
            rut_buffer_new (mesh->attributes[i]->buffer->size);
          memcpy (attribute_buffers[n_buffers]->data,
                  mesh->attributes[i]->buffer->data,
                  mesh->attributes[i]->buffer->size);

          attribute_buffers_map[i] = attribute_buffers[n_buffers];
          buffers[n_buffers++] = mesh->attributes[i]->buffer;
        }
    }

  attributes = g_alloca (sizeof (void *) * mesh->n_attributes);
  for (i = 0; i < mesh->n_attributes; i++)
    {
      attributes[i] = rut_attribute_new (attribute_buffers_map[i],
                                         mesh->attributes[i]->name,
                                         mesh->attributes[i]->stride,
                                         mesh->attributes[i]->offset,
                                         mesh->attributes[i]->n_components,
                                         mesh->attributes[i]->type);
    }

  copy = rut_mesh_new (mesh->mode,
                       mesh->n_vertices,
                       attributes,
                       mesh->n_attributes);

  for (i = 0; i < mesh->n_attributes; i++)
    rut_object_unref (attributes[i]);

  if (mesh->indices_buffer)
    {
      RutBuffer *indices_buffer = rut_buffer_new (mesh->indices_buffer->size);
      memcpy (indices_buffer->data,
              mesh->indices_buffer->data,
              mesh->indices_buffer->size);
      rut_mesh_set_indices (copy,
                            mesh->indices_type,
                            indices_buffer,
                            mesh->n_indices);
      rut_object_unref (indices_buffer);
    }

  return copy;
}
