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
#include "rut-mesh.h"
#include "rut-mesh-ply.h"

#include "components/rut-material.h"
#include "components/rut-model.h"

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

CoglPrimitive *
rut_model_get_primitive (RutObject *object)
{
  RutModel *model = object;

  if (!model->primitive)
    {
      if (model->mesh)
        {
          model->primitive =
            rut_mesh_create_primitive (model->ctx, model->mesh);
        }
    }

  return model->primitive;
}

RutType rut_model_type;

static RutComponentableVTable _rut_model_componentable_vtable = {
  .draw = NULL
};

static RutPrimableVTable _rut_model_primable_vtable = {
  .get_primitive = rut_model_get_primitive
};

static RutPickableVTable _rut_model_pickable_vtable = {
  .get_mesh = rut_model_get_mesh
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
  model->ctx = ctx;

  return model;
}

/* These should be sorted in descending order of size to
 * avoid gaps due to attributes being naturally aligned. */
static RutPLYAttribute ply_attributes[] =
{
  {
    .name = "cogl_position_in",
    .properties = {
      { "x" },
      { "y" },
      { "z" },
    },
    .n_properties = 3,
    .min_components = 1,
  },
  {
    .name = "cogl_normal_in",
    .properties = {
      { "nx" },
      { "ny" },
      { "nz" },
    },
    .n_properties = 3,
    .min_components = 3,
    .pad_n_components = 3,
    .pad_type = RUT_ATTRIBUTE_TYPE_FLOAT,
  },
  {
    .name = "cogl_tex_coord0_in",
    .properties = {
      { "s" },
      { "t" },
      { "r" },
    },
    .n_properties = 3,
    .min_components = 2,
  },
  {
    .name = "tangent",
    .properties = {
      { "tanx" },
      { "tany" },
      { "tanz" }
    },
    .n_properties = 3,
    .min_components = 3,
    .pad_n_components = 3,
    .pad_type = RUT_ATTRIBUTE_TYPE_FLOAT,
  },
  {
    .name = "cogl_color_in",
    .properties = {
      { "red" },
      { "green" },
      { "blue" },
      { "alpha" }
    },
    .n_properties = 4,
    .normalized = TRUE,
    .min_components = 3,
  }
};


RutModel *
rut_model_new_from_file (RutContext *ctx,
                         const char *file)
{
  RutModel *model;
  GError *error = NULL;
  RutPLYAttributeStatus padding_status[G_N_ELEMENTS (ply_attributes)];

  model = _rut_model_new (ctx);
  model->type = RUT_MODEL_TYPE_FILE;
  model->mesh = rut_mesh_new_from_ply (ctx,
                                       file,
                                       ply_attributes,
                                       G_N_ELEMENTS (ply_attributes),
                                       padding_status,
                                       &error);
  if (!model->mesh)
    {
      g_critical ("could not load model %s: %s", file, error->message);
      rut_model_free (model);
      model = NULL;
    }

  return model;
}

RutModel *
rut_model_new_from_asset (RutContext *ctx,
                          const char *file)
{
  char *full_path = g_build_filename (ctx->assets_location, file, NULL);
  RutModel *model = rut_model_new_from_file (ctx, full_path);

  model->path = g_strdup (file);

  g_free (full_path);
  return model;
}

static RutMesh *
create_mesh_from_vertex_data (Vertex *data,
                              int n_vertices)
{
  RutBuffer *buffer = rut_buffer_new (sizeof (Vertex) * n_vertices);
  RutMesh *mesh;

  memcpy (buffer->data, data, buffer->size);

  mesh = rut_mesh_new_from_buffer_p3n3 (COGL_VERTICES_MODE_TRIANGLES,
                                        n_vertices,
                                        buffer);

  rut_refable_unref (buffer);

  return mesh;
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
      model->mesh =
        create_mesh_from_vertex_data (plane_vertices,
                                      G_N_ELEMENTS (plane_vertices));
    }
  else if (g_strcmp0 (name, "cube") == 0)
    {
      model->mesh =
        create_mesh_from_vertex_data (cube_vertices,
                                      G_N_ELEMENTS (cube_vertices));
    }
  else if (g_strcmp0 (name, "circle") == 0)
    {
      model->mesh = rut_create_circle_outline_mesh (64);
    }
  else if (g_strcmp0 (name, "rotation-tool") == 0)
    {
      model->mesh = rut_create_rotation_tool_mesh (64);
    }
  else
    g_assert_not_reached ();

  return model;
}

void rut_model_free (RutModel *model)
{
  if (model->primitive)
    cogl_object_unref (model->primitive);

  if (model->mesh)
    rut_refable_unref (model->mesh);

  g_slice_free (RutModel, model);
}

RutMesh *
rut_model_get_mesh (RutModel *model)
{
  return model->mesh;
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

