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

#include "rig-global.h"
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

#define norm     1.0f,  1.0f, 1.0f

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
create_ply_primitive (const gchar *filename)
{
  MashData *data = mash_data_new ();
  GError *error = NULL;

  mash_data_load (data, MASH_DATA_NONE, filename, &error);
  if (error)
    {
      g_critical ("could not load model %s: %s", filename, error->message);
      return NULL;
    }

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

static void
rig_mesh_renderer_draw (RigComponent    *component,
                        CoglFramebuffer *fb)
{
  RigMeshRenderer *renderer = RIG_MESH_RENDERER (component);
  CoglMatrix modelview_matrix;
  float normal_matrix[9];

  cogl_framebuffer_get_modelview_matrix (fb, &modelview_matrix);
  rig_mesh_get_normal_matrix (&modelview_matrix, normal_matrix);
  cogl_pipeline_set_uniform_matrix (renderer->pipeline,
                                    renderer->normal_matrix_uniform,
                                    3, /* dimensions */
                                    1, /* count */
                                    FALSE, /* don't transpose again */
                                    normal_matrix);

  if (renderer->primitive)
    {
      cogl_framebuffer_draw_primitive (fb,
                                       renderer->pipeline,
                                       renderer->primitive);
    }
  else if (renderer->mesh_data)
    {
      CoglPrimitive *primitive;

      primitive = mash_data_get_primitive (renderer->mesh_data);
      cogl_framebuffer_draw_primitive (fb,
                                       renderer->pipeline,
                                       primitive);
    }
}

static RigMeshRenderer *
rig_mesh_renderer_new (void)
{
  RigMeshRenderer *renderer;

  renderer = g_slice_new0 (RigMeshRenderer);
  renderer->component.type = RIG_COMPONENT_TYPE_MESH_RENDERER;
  renderer->component.draw = rig_mesh_renderer_draw;

  return renderer;
}

RigComponent *
rig_mesh_renderer_new_from_file (const char   *file,
                                 CoglPipeline *pipeline)
{
  RigMeshRenderer *renderer;

  renderer = rig_mesh_renderer_new ();
  renderer->mesh_data = create_ply_primitive (file);

  rig_mesh_renderer_set_pipeline (renderer, pipeline);

  return RIG_COMPONENT (renderer);
}

RigComponent *
rig_mesh_renderer_new_from_template (const char   *name,
                                     CoglPipeline *pipeline)
{
  RigMeshRenderer *renderer;

  renderer = rig_mesh_renderer_new ();

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
  else
    g_assert_not_reached ();

  rig_mesh_renderer_set_pipeline (renderer, pipeline);

  return RIG_COMPONENT (renderer);
}

void rig_mesh_renderer_free (RigMeshRenderer *renderer)
{
  if (renderer->pipeline)
    cogl_object_unref (renderer->pipeline);

  if (renderer->primitive)
    cogl_object_unref (renderer->primitive);

  if (renderer->mesh_data)
    g_object_unref (renderer->mesh_data);

  g_slice_free (RigMeshRenderer, renderer);
}


void
rig_mesh_renderer_set_pipeline (RigMeshRenderer *renderer,
                                CoglPipeline    *pipeline)
{
  if (renderer->pipeline)
    {
      cogl_object_unref (renderer->pipeline);
      renderer->pipeline = NULL;
    }

  if (pipeline)
    {
      renderer->pipeline = cogl_object_ref (pipeline);
      renderer->normal_matrix_uniform =
        cogl_pipeline_get_uniform_location (renderer->pipeline,
                                            "normal_matrix");
    }
}

uint8_t *
rig_mesh_renderer_get_vertex_data (RigMeshRenderer *renderer,
                                   size_t          *stride)
{
  if (stride)
    *stride = renderer->stride;

  return renderer->vertex_data;
}

int
rig_mesh_renderer_get_n_vertices (RigMeshRenderer *renderer)
{
  return renderer->n_vertices;
}
