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

#include <math.h>

#include "rig-global.h"
#include "rig-types.h"
#include "rig-geometry.h"
#include "components/rig-material.h"

#include "rig-mesh-renderer.h"

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
create_primitive_from_vertex_data (RigMeshRenderer *renderer,
                                   Vertex          *data,
                                   int              n_vertices)
{
  CoglAttributeBuffer *attribute_buffer;
  CoglAttribute *attributes[2];
  CoglPrimitive *primitive;

  attribute_buffer = cogl_attribute_buffer_new (rig_cogl_context,
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

  /* update the renderer states */
  renderer->primitive = primitive;
  renderer->vertex_data = (uint8_t *) data;
  renderer->n_vertices = n_vertices;
  renderer->stride = sizeof (Vertex);

  return primitive;
}

static MashData *
create_ply_primitive (RigContext *ctx, const gchar *filename)
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

static void
rig_mesh_get_normal_matrix (const CoglMatrix *matrix,
                            float *normal_matrix)
{
  CoglMatrix inverse_matrix;

  /* Invert the matrix */
  cogl_matrix_get_inverse (matrix, &inverse_matrix);

  /* Transpose it while converting it to 3x3 */
  normal_matrix[0] = inverse_matrix.xx;
  normal_matrix[1] = inverse_matrix.xy;
  normal_matrix[2] = inverse_matrix.xz;

  normal_matrix[3] = inverse_matrix.yx;
  normal_matrix[4] = inverse_matrix.yy;
  normal_matrix[5] = inverse_matrix.yz;

  normal_matrix[6] = inverse_matrix.zx;
  normal_matrix[7] = inverse_matrix.zy;
  normal_matrix[8] = inverse_matrix.zz;
}

CoglPrimitive *
rig_mesh_renderer_get_primitive (RigObject *object)
{
  RigMeshRenderer *renderer = object;

  if (renderer->primitive)
    return renderer->primitive;
  else if (renderer->mesh_data)
    return mash_data_get_primitive (renderer->mesh_data);
  else
    return NULL;
}

static void
rig_mesh_renderer_draw (RigObject *object,
                        CoglFramebuffer *fb)
{
  RigMeshRenderer *renderer = object;
  RigComponentableProps *component =
    rig_object_get_properties (object, RIG_INTERFACE_ID_COMPONENTABLE);
  CoglMatrix modelview_matrix;
  float normal_matrix[9];
  RigMaterial *material =
    rig_entity_get_component (component->entity, RIG_COMPONENT_TYPE_MATERIAL);
  CoglPipeline *pipeline;
  CoglPrimitive *primitive;

  /* FIXME: We could create a default material component in this case */
  if (!material)
    {
      g_warning ("Can't paint mesh without a material component");
      return;
    }

  pipeline = rig_material_get_pipeline (material);

  if (G_UNLIKELY (renderer->pipeline_cache != pipeline))
    {
      renderer->normal_matrix_uniform =
        cogl_pipeline_get_uniform_location (pipeline, "normal_matrix");
      renderer->pipeline_cache = pipeline;
    }

  cogl_framebuffer_get_modelview_matrix (fb, &modelview_matrix);
  rig_mesh_get_normal_matrix (&modelview_matrix, normal_matrix);
  cogl_pipeline_set_uniform_matrix (pipeline,
                                    renderer->normal_matrix_uniform,
                                    3, /* dimensions */
                                    1, /* count */
                                    FALSE, /* don't transpose again */
                                    normal_matrix);

  primitive = rig_mesh_renderer_get_primitive (renderer);

  cogl_framebuffer_draw_primitive (fb,
                                   pipeline,
                                   primitive);
}

RigType rig_mesh_renderer_type;

static RigComponentableVTable _rig_mesh_renderer_componentable_vtable = {
  .draw = rig_mesh_renderer_draw
};

static RigPrimableVTable _rig_mesh_renderer_primable_vtable = {
  .get_primitive = rig_mesh_renderer_get_primitive
};

static RigPickableVTable _rig_mesh_renderer_pickable_vtable = {
  .get_vertex_data = rig_mesh_renderer_get_vertex_data
};

void
_rig_mesh_renderer_init_type (void)
{
  rig_type_init (&rig_mesh_renderer_type);
  rig_type_add_interface (&rig_mesh_renderer_type,
                          RIG_INTERFACE_ID_COMPONENTABLE,
                          offsetof (RigMeshRenderer, component),
                          &_rig_mesh_renderer_componentable_vtable);
  rig_type_add_interface (&rig_mesh_renderer_type,
                          RIG_INTERFACE_ID_PRIMABLE,
                          0, /* no associated properties */
                          &_rig_mesh_renderer_primable_vtable);
  rig_type_add_interface (&rig_mesh_renderer_type,
                          RIG_INTERFACE_ID_PICKABLE,
                          0, /* no associated properties */
                          &_rig_mesh_renderer_pickable_vtable);
}

static RigMeshRenderer *
_rig_mesh_renderer_new (RigContext *ctx)
{
  RigMeshRenderer *renderer;

  renderer = g_slice_new0 (RigMeshRenderer);
  rig_object_init (&renderer->_parent, &rig_mesh_renderer_type);
  renderer->component.type = RIG_COMPONENT_TYPE_GEOMETRY;

  return renderer;
}

RigMeshRenderer *
rig_mesh_renderer_new_from_file (RigContext *ctx,
                                 const char *file)
{
  RigMeshRenderer *renderer;

  renderer = _rig_mesh_renderer_new (ctx);
  renderer->type = RIG_MESH_RENDERER_TYPE_FILE;
  renderer->path = g_strdup (file);
  renderer->mesh_data = create_ply_primitive (ctx, file);

  return renderer;
}

RigMeshRenderer *
rig_mesh_renderer_new_from_template (RigContext *ctx,
                                     const char *name)
{
  RigMeshRenderer *renderer;

  renderer = _rig_mesh_renderer_new (ctx);

  renderer->type = RIG_MESH_RENDERER_TYPE_TEMPLATE;
  renderer->path = g_strdup (name);

  if (g_strcmp0 (name, "plane") == 0)
    {
      create_primitive_from_vertex_data (renderer,
                                         plane_vertices,
                                         G_N_ELEMENTS (plane_vertices));
    }
  else if (g_strcmp0 (name, "cube") == 0)
    {
      create_primitive_from_vertex_data (renderer,
                                         cube_vertices,
                                         G_N_ELEMENTS (cube_vertices));
    }
  else if (g_strcmp0 (name, "circle") == 0)
    {
      renderer->primitive = rig_create_circle_outline_primitive (ctx, 64);
      renderer->vertex_data = NULL;
      renderer->n_vertices = 0;
      renderer->stride = 0;
      //renderer->vertex_data = (uint8_t *) buffer;
      //renderer->n_vertices = n_vertices;
      //renderer->stride = sizeof (CoglVertexP3C4);
    }
  else if (g_strcmp0 (name, "rotation-tool") == 0)
    {
      renderer->primitive = rig_create_rotation_tool_primitive (ctx, 64);
      renderer->vertex_data = NULL;
      renderer->n_vertices = 0;
      renderer->stride = 0;
      //renderer->vertex_data = (uint8_t *) buffer;
      //renderer->n_vertices = n_vertices * 3;
      //renderer->stride = sizeof (CoglVertexP3C4);
    }
  else
    g_assert_not_reached ();

  return renderer;
}

void rig_mesh_renderer_free (RigMeshRenderer *renderer)
{
  if (renderer->primitive)
    cogl_object_unref (renderer->primitive);

  if (renderer->mesh_data)
    g_object_unref (renderer->mesh_data);

  g_slice_free (RigMeshRenderer, renderer);
}

void *
rig_mesh_renderer_get_vertex_data (RigMeshRenderer *renderer,
                                   size_t *stride,
                                   int *n_vertices)
{
  if (stride)
    *stride = renderer->stride;

  if (n_vertices)
    *n_vertices = renderer->n_vertices;

  return renderer->vertex_data;
}

int
rig_mesh_renderer_get_n_vertices (RigMeshRenderer *renderer)
{
  return renderer->n_vertices;
}

RigMeshRendererType
rig_mesh_renderer_get_type (RigMeshRenderer *renderer)
{
  return renderer->type;
}

const char *
rig_mesh_renderer_get_path (RigMeshRenderer *renderer)
{
  return renderer->path;
}

