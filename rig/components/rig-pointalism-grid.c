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

#include <config.h>

#include <math.h>
#include <stdlib.h>

#include <rut.h>

#include "rig-pointalism-grid.h"

#define MESA_CONST_ATTRIB_BUG_WORKAROUND


static RutPropertySpec _rig_pointalism_grid_prop_specs[] = {
  {
    .name = "pointalism-scale",
    .nick = "Pointalism Scale Factor",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .getter.float_type = rig_pointalism_grid_get_scale,
    .setter.float_type = rig_pointalism_grid_set_scale,
    .flags = RUT_PROPERTY_FLAG_READWRITE |
      RUT_PROPERTY_FLAG_VALIDATE,
    .validation = { .float_range = { 0, 100 }},
    .animatable = TRUE
  },
  {
    .name = "pointalism-z",
    .nick = "Pointalism Z Factor",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .getter.float_type = rig_pointalism_grid_get_z,
    .setter.float_type = rig_pointalism_grid_set_z,
    .flags = RUT_PROPERTY_FLAG_READWRITE |
      RUT_PROPERTY_FLAG_VALIDATE,
    .validation = { .float_range = { 0, 100 }},
    .animatable = TRUE
  },
  {
    .name = "pointalism-lighter",
    .nick = "Pointalism Lighter",
    .type = RUT_PROPERTY_TYPE_BOOLEAN,
    .getter.boolean_type = rig_pointalism_grid_get_lighter,
    .setter.boolean_type = rig_pointalism_grid_set_lighter,
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .animatable = TRUE
  },
  {
    .name = "pointalism-cell-size",
    .nick = "Cell Size",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .getter.float_type = rig_pointalism_grid_get_cell_size,
    .setter.float_type = rig_pointalism_grid_set_cell_size,
    .flags = RUT_PROPERTY_FLAG_READWRITE | RUT_PROPERTY_FLAG_VALIDATE,
    .validation = { .float_range = { 1, 100 }},
    .animatable = TRUE
  },
  { NULL }
};

static void
_rig_pointalism_grid_slice_free (void *object)
{
  RigPointalismGridSlice *pointalism_grid_slice = object;

#ifdef RIG_ENABLE_DEBUG
  {
    RutComponentableProps *component =
      rut_object_get_properties (object, RUT_TRAIT_ID_COMPONENTABLE);
    g_return_if_fail (component->entity == NULL);
  }
#endif

  rut_object_unref (pointalism_grid_slice->mesh);

  rut_object_free (RigPointalismGridSlice, object);
}

RutType rig_pointalism_grid_slice_type;

void
_rig_pointalism_grid_slice_init_type (void)
{

  RutType *type = &rig_pointalism_grid_slice_type;

#define TYPE RigPointalismGridSlice

  rut_type_init (type, G_STRINGIFY (TYPE), _rig_pointalism_grid_slice_free);

#undef TYPE
}

typedef struct _GridVertex
{
  float x0, y0;
  float x1, y1;
  float s0, t0;
  float s1, s2, t1, t2;
  float s3, t3;
#ifdef MESA_CONST_ATTRIB_BUG_WORKAROUND
  float nx, ny, nz;
  float tx, ty, tz;
#endif
} GridVertex;

static RutMesh *
mesh_new_grid (CoglVerticesMode mode,
               int n_vertices,
               int n_indices,
               GridVertex *vertices,
               unsigned int *indices)
{
  RutMesh *mesh;
  RutAttribute *attributes[10];
  RutBuffer *vertex_buffer;
  RutBuffer *index_buffer;

  vertex_buffer = rut_buffer_new (sizeof (GridVertex) * n_vertices);
  index_buffer = rut_buffer_new (sizeof (unsigned int) * n_indices);

  memcpy (vertex_buffer->data, vertices, sizeof (GridVertex) * n_vertices);
  memcpy (index_buffer->data, indices, sizeof (unsigned int) * n_indices);

  attributes[0] = rut_attribute_new (vertex_buffer,
                                     "cogl_position_in",
                                     sizeof (GridVertex),
                                     offsetof (GridVertex, x0),
                                     2,
                                     RUT_ATTRIBUTE_TYPE_FLOAT);

  attributes[1] = rut_attribute_new (vertex_buffer,
                                     "cogl_tex_coord0_in",
                                     sizeof (GridVertex),
                                     offsetof (GridVertex, s0),
                                     2,
                                     RUT_ATTRIBUTE_TYPE_FLOAT);

  attributes[2] = rut_attribute_new (vertex_buffer,
                                     "cogl_tex_coord1_in",
                                     sizeof (GridVertex),
                                     offsetof (GridVertex, s3),
                                     2,
                                     RUT_ATTRIBUTE_TYPE_FLOAT);

  attributes[3] = rut_attribute_new (vertex_buffer,
                                     "cogl_tex_coord4_in",
                                     sizeof (GridVertex),
                                     offsetof (GridVertex, s3),
                                     2,
                                     RUT_ATTRIBUTE_TYPE_FLOAT);

  attributes[4] = rut_attribute_new (vertex_buffer,
                                     "cogl_tex_coord7_in",
                                     sizeof (GridVertex),
                                     offsetof (GridVertex, s3),
                                     2,
                                     RUT_ATTRIBUTE_TYPE_FLOAT);

  attributes[5] = rut_attribute_new (vertex_buffer,
                                     "cogl_tex_coord11_in",
                                     sizeof (GridVertex),
                                     offsetof (GridVertex, s0),
                                     2,
                                     RUT_ATTRIBUTE_TYPE_FLOAT);

  attributes[6] = rut_attribute_new (vertex_buffer,
                                     "cogl_normal_in",
                                     sizeof (GridVertex),
                                     offsetof (GridVertex, nx),
                                     3,
                                     RUT_ATTRIBUTE_TYPE_FLOAT);

  attributes[7] = rut_attribute_new (vertex_buffer,
                                     "tangent_in",
                                     sizeof (GridVertex),
                                     offsetof (GridVertex, tx),
                                     3,
                                     RUT_ATTRIBUTE_TYPE_FLOAT);

  attributes[8] = rut_attribute_new (vertex_buffer,
                                     "cell_xy",
                                     sizeof (GridVertex),
                                     offsetof (GridVertex, x1),
                                     2,
                                     RUT_ATTRIBUTE_TYPE_FLOAT);

  attributes[9] = rut_attribute_new (vertex_buffer,
                                     "cell_st",
                                     sizeof (GridVertex),
                                     offsetof (GridVertex, s1),
                                     4,
                                     RUT_ATTRIBUTE_TYPE_FLOAT);

  mesh = rut_mesh_new (mode, n_vertices, attributes, 10);
  rut_mesh_set_indices (mesh,
                        COGL_INDICES_TYPE_UNSIGNED_INT,
                        index_buffer,
                        n_indices);

  g_free (vertices);
  g_free (indices);

  return mesh;
}

void
pointalism_generate_grid (RigPointalismGridSlice *slice,
                          int tex_width,
                          int tex_height,
                          float size)
{
  int columns = abs (tex_width / size);
  int rows = abs (tex_height / size);
  int i, j, k, l;
  float cell_s = 1.0 / columns;
  float cell_t = 1.0 / rows;
  float start_x = -1.0 * ((size * columns) / 2.0);
  float start_y = -1.0 * ((size * rows) / 2.0);
  GridVertex *vertices = g_new (GridVertex, (columns * rows) * 4);
  unsigned int *indices = g_new (unsigned int, (columns * rows) * 6);

  k = 0;
  l = 0;

  for (i = 0; i < rows; i++)
  {
    for (j = 0; j < columns; j++)
    {
      float x = start_x + (size / 2);
      float y = start_y + (size / 2);

      vertices[k].x0 = -1 * (size / 2);
      vertices[k].y0 = -1 * (size / 2);
      vertices[k].s0 = 0;
      vertices[k].t0 = 0;
      vertices[k].x1 = x;
      vertices[k].y1 = y;
      vertices[k].s1 = j * cell_s;
      vertices[k].t1 = i * cell_t;
      vertices[k].s2 = (j + 1) * cell_s;
      vertices[k].t2 = (i + 1) * cell_t;
      vertices[k].s3 = j * cell_s;
      vertices[k].t3 = i * cell_t;
      vertices[k].nx = 0;
      vertices[k].ny = 0;
      vertices[k].nz = 1;
      vertices[k].tx = 1;
      vertices[k].ty = 0;
      vertices[k].tz = 0;
      k++;

      vertices[k].x0 = size / 2;
      vertices[k].y0 = -1 * (size / 2);
      vertices[k].s0 = 1;
      vertices[k].t0 = 0;
      vertices[k].x1 = x;
      vertices[k].y1 = y;
      vertices[k].s1 = j * cell_s;
      vertices[k].t1 = i * cell_t;
      vertices[k].s2 = (j + 1) * cell_s;
      vertices[k].t2 = (i + 1) * cell_t;
      vertices[k].s3 = (j + 1) * cell_s;
      vertices[k].t3 = i * cell_t;
      vertices[k].nx = 0;
      vertices[k].ny = 0;
      vertices[k].nz = 1;
      vertices[k].tx = 1;
      vertices[k].ty = 0;
      vertices[k].tz = 0;
      k++;

      vertices[k].x0 = size / 2;
      vertices[k].y0 = size / 2;
      vertices[k].s0 = 1;
      vertices[k].t0 = 1;
      vertices[k].x1 = x;
      vertices[k].y1 = y;
      vertices[k].s1 = j * cell_s;
      vertices[k].t1 = i * cell_t;
      vertices[k].s2 = (j + 1) * cell_s;
      vertices[k].t2 = (i + 1) * cell_t;
      vertices[k].s3 = (j + 1) * cell_s;
      vertices[k].t3 = (i + 1) * cell_t;
      vertices[k].nx = 0;
      vertices[k].ny = 0;
      vertices[k].nz = 1;
      vertices[k].tx = 1;
      vertices[k].ty = 0;
      vertices[k].tz = 0;
      k++;

      vertices[k].x0 = -1 * (size / 2);
      vertices[k].y0 = size / 2;
      vertices[k].s0 = 0;
      vertices[k].t0 = 1;
      vertices[k].x1 = x;
      vertices[k].y1 = y;
      vertices[k].s1 = j * cell_s;
      vertices[k].t1 = i * cell_t;
      vertices[k].s2 = (j + 1) * cell_s;
      vertices[k].t2 = (i + 1) * cell_t;
      vertices[k].s3 = j * cell_s;
      vertices[k].t3 = (i + 1) * cell_t;
      vertices[k].nx = 0;
      vertices[k].ny = 0;
      vertices[k].nz = 1;
      vertices[k].tx = 1;
      vertices[k].ty = 0;
      vertices[k].tz = 0;
      k++;

      indices[l] = k;
      indices[l + 1] = k + 1;
      indices[l + 2] = k + 2;
      indices[l + 3] = k + 2;
      indices[l + 4] = k + 3;
      indices[l + 5] = k;

      l += 6;
      start_x += size;
    }
    start_x = -1.0 * ((size * columns) / 2.0);
    start_y += size;
  }

  if (slice->mesh)
    rut_object_unref (slice->mesh);

  slice->mesh = mesh_new_grid (COGL_VERTICES_MODE_TRIANGLES,
                               (columns * rows) * 4,
                               (columns * rows) * 6,
                               vertices, indices);
}

static RigPointalismGridSlice *
pointalism_grid_slice_new (int tex_width,
                           int tex_height,
                           float size)
{
  RigPointalismGridSlice *grid_slice =
    rut_object_alloc0 (RigPointalismGridSlice, &rig_pointalism_grid_slice_type, _rig_pointalism_grid_slice_init_type);


  grid_slice->mesh = NULL;

  pointalism_generate_grid (grid_slice, tex_width, tex_height, size);

  return grid_slice;
}

static void
_rig_pointalism_grid_free (void *object)
{
  RigPointalismGrid *grid = object;

  rut_closure_list_disconnect_all (&grid->updated_cb_list);

  rut_object_unref (grid->slice);
  rut_object_unref (grid->pick_mesh);

  rut_introspectable_destroy (grid);

  rut_object_free (RigPointalismGrid, grid);
}

static RutObject *
_rig_pointalism_grid_copy (RutObject *object)
{
  RigPointalismGrid *grid = object;
  RigPointalismGrid *copy = rig_pointalism_grid_new (grid->ctx,
                                                     grid->cell_size,
                                                     grid->tex_width,
                                                     grid->tex_height);

#warning "FIXME: make sure the pointalism grid mesh is generated lazily"
  rut_introspectable_copy_properties (&grid->ctx->property_ctx,
                                      grid,
                                      copy);

  return copy;
}

RutType rig_pointalism_grid_type;

void
_rig_pointalism_grid_init_type (void)
{

  static RutComponentableVTable componentable_vtable = {
    .copy = _rig_pointalism_grid_copy
  };

  static RutPrimableVTable primable_vtable = {
    .get_primitive = rig_pointalism_grid_get_primitive
  };

  static RutMeshableVTable meshable_vtable = {
    .get_mesh = rig_pointalism_grid_get_pick_mesh
  };


  RutType *type = &rig_pointalism_grid_type;

#define TYPE RigPointalismGrid

  rut_type_init (type, G_STRINGIFY (TYPE), _rig_pointalism_grid_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_COMPONENTABLE,
                      offsetof (TYPE, component),
                      &componentable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_PRIMABLE,
                      0, /* no associated properties */
                      &primable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_MESHABLE,
                      0, /* no associated properties */
                      &meshable_vtable);


  rut_type_add_trait (type,
                      RUT_TRAIT_ID_INTROSPECTABLE,
                      offsetof (TYPE, introspectable),
                      NULL);

#undef TYPE
}

RigPointalismGrid *
rig_pointalism_grid_new (RutContext *ctx,
                         float size,
                         int tex_width,
                         int tex_height)
{
  RigPointalismGrid *grid =
    rut_object_alloc0 (RigPointalismGrid, &rig_pointalism_grid_type, _rig_pointalism_grid_init_type);
  RutBuffer *buffer = rut_buffer_new (sizeof (CoglVertexP3) * 6);
  RutMesh *pick_mesh = rut_mesh_new_from_buffer_p3 (COGL_VERTICES_MODE_TRIANGLES,
                                                    6,
                                                    buffer);
  CoglVertexP3 *pick_vertices = (CoglVertexP3 *)buffer->data;
  float half_tex_width;
  float half_tex_height;



  grid->component.type = RUT_COMPONENT_TYPE_GEOMETRY;

  grid->ctx = rut_object_ref (ctx);

  rut_list_init (&grid->updated_cb_list);

  grid->slice = pointalism_grid_slice_new (tex_width, tex_height,
                                           size);

  half_tex_width = tex_width / 2.0f;
  half_tex_height = tex_height / 2.0f;

  pick_vertices[0].x = -half_tex_width;
  pick_vertices[0].y = -half_tex_height;
  pick_vertices[1].x = -half_tex_width;
  pick_vertices[1].y = half_tex_height;
  pick_vertices[2].x = half_tex_width;
  pick_vertices[2].y = half_tex_height;
  pick_vertices[3] = pick_vertices[0];
  pick_vertices[4] = pick_vertices[2];
  pick_vertices[5].x = half_tex_width;
  pick_vertices[5].y = -half_tex_height;

  grid->pick_mesh = pick_mesh;
  grid->pointalism_scale = 1;
  grid->pointalism_z = 1;
  grid->pointalism_lighter = TRUE;
  grid->cell_size = size;
  grid->tex_width = tex_width;
  grid->tex_height = tex_height;

  rut_introspectable_init (grid, _rig_pointalism_grid_prop_specs,
                           grid->properties);

  return grid;
}

CoglPrimitive *
rig_pointalism_grid_get_primitive (RutObject *object)
{
  RigPointalismGrid *grid = object;
  CoglPrimitive *primitive = rut_mesh_create_primitive (grid->ctx,
                                                        grid->slice->mesh);
  return primitive;
}

RutMesh *
rig_pointalism_grid_get_pick_mesh (RutObject *self)
{
  RigPointalismGrid *grid = self;
  return grid->pick_mesh;
}

float
rig_pointalism_grid_get_scale (RutObject *obj)
{
  RigPointalismGrid *grid = RIG_POINTALISM_GRID (obj);

  return grid->pointalism_scale;
}

void
rig_pointalism_grid_set_scale (RutObject *obj,
                               float scale)
{
  RigPointalismGrid *grid = RIG_POINTALISM_GRID (obj);
  RigEntity *entity;
  RutContext *ctx;

  if (scale == grid->pointalism_scale)
    return;

  grid->pointalism_scale = scale;

  entity = grid->component.entity;
  ctx = rig_entity_get_context (entity);
  rut_property_dirty (&ctx->property_ctx,
                     &grid->properties[RIG_POINTALISM_GRID_PROP_SCALE]);
}

float
rig_pointalism_grid_get_z (RutObject *obj)
{
  RigPointalismGrid *grid = RIG_POINTALISM_GRID (obj);

  return grid->pointalism_z;
}



void
rig_pointalism_grid_set_z (RutObject *obj,
                           float z)
{
  RigPointalismGrid *grid = RIG_POINTALISM_GRID (obj);
  RigEntity *entity;
  RutContext *ctx;

  if (z == grid->pointalism_z)
    return;

  grid->pointalism_z = z;

  entity = grid->component.entity;
  ctx = rig_entity_get_context (entity);
  rut_property_dirty (&ctx->property_ctx,
                     &grid->properties[RIG_POINTALISM_GRID_PROP_Z]);
}

bool
rig_pointalism_grid_get_lighter (RutObject *obj)
{
  RigPointalismGrid *grid = RIG_POINTALISM_GRID (obj);

  return grid->pointalism_lighter;
}

void
rig_pointalism_grid_set_lighter (RutObject *obj,
                                 bool lighter)
{
  RigPointalismGrid *grid = RIG_POINTALISM_GRID (obj);
  RigEntity *entity;
  RutContext *ctx;

  if (lighter == grid->pointalism_lighter)
    return;

  grid->pointalism_lighter = lighter;

  entity = grid->component.entity;
  ctx = rig_entity_get_context (entity);
  rut_property_dirty (&ctx->property_ctx,
                 &grid->properties[RIG_POINTALISM_GRID_PROP_LIGHTER]);
}

float
rig_pointalism_grid_get_cell_size (RutObject *obj)
{
  RigPointalismGrid *grid = RIG_POINTALISM_GRID (obj);

  return grid->cell_size;
}

void
rig_pointalism_grid_set_cell_size (RutObject *obj,
                                   float cell_size)
{
  RigPointalismGrid *grid = RIG_POINTALISM_GRID (obj);
  RigEntity *entity;
  RutContext *ctx;

  if (cell_size == grid->cell_size)
    return;

  grid->cell_size = cell_size;

  entity = grid->component.entity;
  ctx = rig_entity_get_context (entity);

  pointalism_generate_grid (grid->slice, grid->tex_width,
                            grid->tex_height, grid->cell_size);

  rut_property_dirty (&ctx->property_ctx,
                      &grid->properties[RIG_POINTALISM_GRID_PROP_CELL_SIZE]);

  rut_closure_list_invoke (&grid->updated_cb_list,
                           RigPointalismGridUpdateCallback,
                           grid);
}

RutClosure *
rig_pointalism_grid_add_update_callback (RigPointalismGrid *grid,
                                         RigPointalismGridUpdateCallback callback,
                                         void *user_data,
                                         RutClosureDestroyCallback destroy_cb)
{
  g_return_val_if_fail (callback != NULL, NULL);
  return rut_closure_list_add (&grid->updated_cb_list,
                               callback,
                               user_data,
                               destroy_cb);
}
