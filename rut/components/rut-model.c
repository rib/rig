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

#include <math.h>

#include "rut-global.h"
#include "rut-types.h"
#include "rut-geometry.h"
#include "components/rut-material.h"

#include "rut-model.h"

typedef struct
{
  float x, y, z;        /* position       */
  float n_x, n_y, n_z;  /* normal         */
} Vertex;

/*
 *        f +--------+ e
 *         /        /|
 *        /        / |
 *    b  /      a /  |
 *      +--------+   |
 *      |  g     |   + h
 *      |        |  /
 *      |        | /
 *    c |        |/
 *      +--------+ d
 */

#define pos_a        1.0f,  1.0f, 1.0f
#define pos_b       -1.0f,  1.0f, 1.0f
#define pos_c       -1.0f, -1.0f, 1.0f
#define pos_d        1.0f, -1.0f, 1.0f

#define pos_e        1.0f,  1.0f, -1.0f
#define pos_f       -1.0f,  1.0f, -1.0f
#define pos_g       -1.0f, -1.0f, -1.0f
#define pos_h        1.0f, -1.0f, -1.0f

#define norm_front   0.0f,  0.0f,  1.0f
#define norm_right   1.0f,  0.0f,  0.0f
#define norm_back    0.0f,  0.0f, -1.0f
#define norm_left   -1.0f,  0.0f,  0.0f
#define norm_top     0.0f,  1.0f,  0.0f
#define norm_bottom  0.0f, -1.0f,  0.0f

static Vertex cube_vertices[] =
{
  { pos_a, norm_front },
  { pos_b, norm_front },
  { pos_c, norm_front },
  { pos_c, norm_front },
  { pos_d, norm_front },
  { pos_a, norm_front },

  { pos_e, norm_right },
  { pos_a, norm_right },
  { pos_d, norm_right },
  { pos_d, norm_right },
  { pos_h, norm_right },
  { pos_e, norm_right },

  { pos_f, norm_back },
  { pos_e, norm_back },
  { pos_h, norm_back },
  { pos_h, norm_back },
  { pos_g, norm_back },
  { pos_f, norm_back },

  { pos_b, norm_left },
  { pos_f, norm_left },
  { pos_g, norm_left },
  { pos_g, norm_left },
  { pos_c, norm_left },
  { pos_b, norm_left },

  { pos_e, norm_top },
  { pos_f, norm_top },
  { pos_b, norm_top },
  { pos_b, norm_top },
  { pos_a, norm_top },
  { pos_e, norm_top },

  { pos_c, norm_bottom },
  { pos_g, norm_bottom },
  { pos_h, norm_bottom },
  { pos_h, norm_bottom },
  { pos_d, norm_bottom },
  { pos_c, norm_bottom }
};

#undef pos_a
#undef pos_b
#undef pos_c
#undef pos_d
#undef pos_e
#undef pos_f
#undef pos_g
#undef pos_h

#undef norm_front
#undef norm_right
#undef norm_back
#undef norm_left
#undef norm_top
#undef norm_bottom

/*
 *        b +--------+ a
 *         /        /
 *        /        /
 *    c  /      d /
 *      +--------+
 */

#define pos_a    100.0f, 0.0f, -100.0f
#define pos_b   -100.0f, 0.0f, -100.0f
#define pos_c   -100.0f, 0.0f,  100.0f
#define pos_d    100.0f, 0.0f,  100.0f

#define norm     0.0f,  1.0f, 0.0f

static Vertex plane_vertices[] =
{
  { pos_a, norm },
  { pos_b, norm },
  { pos_c, norm },
  { pos_c, norm },
  { pos_d, norm },
  { pos_a, norm },
};

#undef pos_a
#undef pos_b
#undef pos_c
#undef pos_d

#undef norm

static CoglPrimitive *
create_primitive_from_vertex_data (RutModel *model,
                                   Vertex *data,
                                   int n_vertices)
{
  CoglAttributeBuffer *attribute_buffer;
  CoglAttribute *attributes[2];
  CoglPrimitive *primitive;

  attribute_buffer = cogl_attribute_buffer_new (rut_cogl_context,
                                                n_vertices * sizeof (Vertex),
                                                data);
  attributes[0] = cogl_attribute_new (attribute_buffer,
                                      "cogl_position_in",
                                      sizeof (Vertex),
                                      offsetof (Vertex, x),
                                      3,
                                      COGL_ATTRIBUTE_TYPE_FLOAT);
  attributes[1] = cogl_attribute_new (attribute_buffer,
                                      "cogl_normal_in",
                                      sizeof (Vertex),
                                      offsetof (Vertex, n_x),
                                      3,
                                      COGL_ATTRIBUTE_TYPE_FLOAT);
  cogl_object_unref (attribute_buffer);

  primitive = cogl_primitive_new_with_attributes (COGL_VERTICES_MODE_TRIANGLES,
                                                  n_vertices,
                                                  attributes, 2);
  cogl_object_unref (attributes[0]);
  cogl_object_unref (attributes[1]);

  /* update the model states */
  model->primitive = primitive;
  model->vertex_data = (uint8_t *) data;
  model->n_vertices = n_vertices;
  model->stride = sizeof (Vertex);

  return primitive;
}

static MashData *
create_ply_primitive (RutContext *ctx, const gchar *filename)
{
  MashData *data = mash_data_new ();
  GError *error = NULL;
  char *full_path = g_build_filename (ctx->assets_location, filename, NULL);

  mash_data_load (data, MASH_DATA_NONE, full_path, &error);
  if (error)
    {
      g_critical ("could not load model %s: %s", filename, error->message);
      g_free (full_path);
      return NULL;
    }

  g_free (full_path);

  return data;
}

CoglPrimitive *
rut_model_get_primitive (RutObject *object)
{
  RutModel *model = object;

  if (model->primitive)
    return model->primitive;
  else if (model->model_data)
    return mash_data_get_primitive (model->model_data);
  else
    return NULL;
}

RutType rut_model_type;

static RutComponentableVTable _rut_model_componentable_vtable = {
  .draw = NULL
};

static RutPrimableVTable _rut_model_primable_vtable = {
  .get_primitive = rut_model_get_primitive
};

static RutPickableVTable _rut_model_pickable_vtable = {
  .get_vertex_data = rut_model_get_vertex_data
};

void
_rut_model_init_type (void)
{
  rut_type_init (&rut_model_type);
  rut_type_add_interface (&rut_model_type,
                          RUT_INTERFACE_ID_COMPONENTABLE,
                          offsetof (RutModel, component),
                          &_rut_model_componentable_vtable);
  rut_type_add_interface (&rut_model_type,
                          RUT_INTERFACE_ID_PRIMABLE,
                          0, /* no associated properties */
                          &_rut_model_primable_vtable);
  rut_type_add_interface (&rut_model_type,
                          RUT_INTERFACE_ID_PICKABLE,
                          0, /* no associated properties */
                          &_rut_model_pickable_vtable);
}

static RutModel *
_rut_model_new (RutContext *ctx)
{
  RutModel *model;

  model = g_slice_new0 (RutModel);
  rut_object_init (&model->_parent, &rut_model_type);
  model->component.type = RUT_COMPONENT_TYPE_GEOMETRY;

  return model;
}

RutModel *
rut_model_new_from_file (RutContext *ctx,
                         const char *file)
{
  RutModel *model;

  model = _rut_model_new (ctx);
  model->type = RUT_MODEL_TYPE_FILE;
  model->path = g_strdup (file);
  model->model_data = create_ply_primitive (ctx, file);

  return model;
}

RutModel *
rut_model_new_from_template (RutContext *ctx,
                             const char *name)
{
  RutModel *model;

  model = _rut_model_new (ctx);

  model->type = RUT_MODEL_TYPE_TEMPLATE;
  model->path = g_strdup (name);

  if (g_strcmp0 (name, "plane") == 0)
    {
      create_primitive_from_vertex_data (model,
                                         plane_vertices,
                                         G_N_ELEMENTS (plane_vertices));
    }
  else if (g_strcmp0 (name, "cube") == 0)
    {
      create_primitive_from_vertex_data (model,
                                         cube_vertices,
                                         G_N_ELEMENTS (cube_vertices));
    }
  else if (g_strcmp0 (name, "circle") == 0)
    {
      model->primitive = rut_create_circle_outline_primitive (ctx, 64);
      model->vertex_data = NULL;
      model->n_vertices = 0;
      model->stride = 0;
      //model->vertex_data = (uint8_t *) buffer;
      //model->n_vertices = n_vertices;
      //model->stride = sizeof (CoglVertexP3C4);
    }
  else if (g_strcmp0 (name, "rotation-tool") == 0)
    {
      model->primitive = rut_create_rotation_tool_primitive (ctx, 64);
      model->vertex_data = NULL;
      model->n_vertices = 0;
      model->stride = 0;
      //model->vertex_data = (uint8_t *) buffer;
      //model->n_vertices = n_vertices * 3;
      //model->stride = sizeof (CoglVertexP3C4);
    }
  else
    g_assert_not_reached ();

  return model;
}

void rut_model_free (RutModel *model)
{
  if (model->primitive)
    cogl_object_unref (model->primitive);

  if (model->model_data)
    g_object_unref (model->model_data);

  g_slice_free (RutModel, model);
}

void *
rut_model_get_vertex_data (RutModel *model,
                           size_t *stride,
                           int *n_vertices)
{
  if (stride)
    *stride = model->stride;

  if (n_vertices)
    *n_vertices = model->n_vertices;

  return model->vertex_data;
}

int
rut_model_get_n_vertices (RutModel *model)
{
  return model->n_vertices;
}

RutModelType
rut_model_get_type (RutModel *model)
{
  return model->type;
}

const char *
rut_model_get_path (RutModel *model)
{
  return model->path;
}

