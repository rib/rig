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
 *
 */

#include <config.h>

#include <glib.h>
#include <math.h>

#include <cogl/cogl.h>

#include "rut-context.h"
#include "rut-geometry.h"
#include "rut-interfaces.h"

CoglAttribute *
rut_create_circle_fan_p2 (RutContext *ctx,
                          int subdivisions,
                          int *n_verts_ret)
{
  int n_verts = subdivisions + 2;
  struct CircleVert {
      float x, y;
  } *verts;
  size_t buffer_size = sizeof (struct CircleVert) * n_verts;
  int i;
  CoglAttributeBuffer *attribute_buffer;
  CoglAttribute *attribute;
  float angle_division = 2.0f * (float)G_PI * (1.0f/(float)subdivisions);

  verts = alloca (buffer_size);

  verts[0].x = 0;
  verts[0].y = 0;
  for (i = 0; i < subdivisions; i++)
    {
      float angle = angle_division * i;
      verts[i + 1].x = sinf (angle);
      verts[i + 1].y = cosf (angle);
    }
  verts[n_verts - 1].x = sinf (0);
  verts[n_verts - 1].y = cosf (0);

  *n_verts_ret = n_verts;

  attribute_buffer =
    cogl_attribute_buffer_new (ctx->cogl_context, buffer_size, verts);

  attribute = cogl_attribute_new (attribute_buffer,
                                  "cogl_position_in",
                                  sizeof (struct CircleVert),
                                  offsetof (struct CircleVert, x),
                                  2,
                                  COGL_ATTRIBUTE_TYPE_FLOAT);
  return attribute;
}

CoglPrimitive *
rut_create_circle_fan_primitive (RutContext *ctx,
                                 int subdivisions)
{
  int n_verts;
  CoglAttribute *attribute =
    rut_create_circle_fan_p2 (ctx, subdivisions, &n_verts);

  return cogl_primitive_new_with_attributes (COGL_VERTICES_MODE_TRIANGLE_FAN,
                                             n_verts,
                                             &attribute, 1);
}

RutMesh *
rut_create_circle_outline_mesh (uint8_t n_vertices)
{
  RutBuffer *buffer = rut_buffer_new (n_vertices * sizeof (CoglVertexP3C4));
  RutMesh *mesh;

  rut_tesselate_circle_with_line_indices ((CoglVertexP3C4 *)buffer->data,
                                          n_vertices,
                                          NULL, /* no indices required */
                                          0,
                                          RUT_AXIS_Z, 255, 255, 255);

  mesh = rut_mesh_new_from_buffer_p3c4 (COGL_VERTICES_MODE_LINE_LOOP,
                                        n_vertices, buffer);

  rut_refable_unref (buffer);

  return mesh;
}

CoglPrimitive *
rut_create_circle_outline_primitive (RutContext *ctx,
                                     uint8_t n_vertices)
{
  RutMesh *mesh = rut_create_circle_outline_mesh (n_vertices);
  CoglPrimitive *primitive = rut_mesh_create_primitive (ctx, mesh);
  rut_refable_unref (mesh);
  return primitive;
}

CoglTexture *
rut_create_circle_texture (RutContext *ctx,
                           int radius_texels,
                           int padding_texels)
{
  CoglPrimitive *circle;
  CoglTexture2D *tex2d;
  CoglOffscreen *offscreen;
  CoglFramebuffer *fb;
  CoglPipeline *white_pipeline;
  int half_size = radius_texels + padding_texels;
  int size = half_size * 2;

  tex2d = cogl_texture_2d_new_with_size (ctx->cogl_context,
                                         size, size,
                                         COGL_PIXEL_FORMAT_RGBA_8888);
  offscreen = cogl_offscreen_new_with_texture (COGL_TEXTURE (tex2d));
  fb = COGL_FRAMEBUFFER (offscreen);

  circle = rut_create_circle_fan_primitive (ctx, 360);

  cogl_framebuffer_clear4f (fb, COGL_BUFFER_BIT_COLOR, 0, 0, 0, 0);

  cogl_framebuffer_identity_matrix (fb);
  cogl_framebuffer_orthographic (fb, 0, 0, size, size, -1, 100);

  cogl_framebuffer_translate (fb, half_size, half_size, 0);
  cogl_framebuffer_scale (fb, radius_texels, radius_texels, 1);

  white_pipeline = cogl_pipeline_new (ctx->cogl_context);
  cogl_pipeline_set_color4f (white_pipeline, 1, 1, 1, 1);

  cogl_primitive_draw (circle, fb, white_pipeline);

  cogl_object_unref (white_pipeline);
  cogl_object_unref (circle);
  cogl_object_unref (offscreen);

  return COGL_TEXTURE (tex2d);
}

/*
 * tesselate_circle:
 * @axis: the axis around which the circle is centered
 */
void
rut_tesselate_circle_with_line_indices (CoglVertexP3C4 *buffer,
                                        uint8_t n_vertices,
                                        uint8_t *indices_data,
                                        int indices_base,
                                        RutAxis axis,
                                        uint8_t r,
                                        uint8_t g,
                                        uint8_t b)
{
  float angle, increment;
  CoglVertexP3C4 *vertex;
  uint8_t i;

  increment = 2 * G_PI / n_vertices;

  for (i = 0, angle = 0.f; i < n_vertices; i++, angle += increment)
    {
      vertex = &buffer[i];

      switch (axis)
        {
        case RUT_AXIS_X:
          vertex->x = 0.f;
          vertex->y = sinf (angle);
          vertex->z = cosf (angle);
          break;
        case RUT_AXIS_Y:
          vertex->x = sinf (angle);
          vertex->y = 0.f;
          vertex->z = cosf (angle);
          break;
        case RUT_AXIS_Z:
          vertex->x = cosf (angle);
          vertex->y = sinf (angle);
          vertex->z = 0.f;
          break;
        }

      vertex->r = r;
      vertex->g = g;
      vertex->b = b;
      vertex->a = 255;
    }

  if (indices_data)
    {
      for (i = indices_base; i < indices_base + n_vertices - 1; i++)
        {
          indices_data[i * 2] = i;
          indices_data[i * 2 + 1] = i + 1;
        }
      indices_data[i * 2] = i;
      indices_data[i * 2 + 1] = indices_base;
    }
}

RutMesh *
rut_create_rotation_tool_mesh (uint8_t n_vertices)
{
  RutMesh *mesh;
  RutBuffer *buffer;
  RutBuffer *indices_buffer;
  uint8_t *indices_data;
  int vertex_buffer_size;

  g_assert (n_vertices < 255 / 3);

  vertex_buffer_size = n_vertices * sizeof (CoglVertexP3C4) * 3;
  buffer = rut_buffer_new (vertex_buffer_size);

  indices_buffer = rut_buffer_new (n_vertices * 2 * 3);
  indices_data = indices_buffer->data;

  rut_tesselate_circle_with_line_indices ((CoglVertexP3C4 *)buffer->data,
                                          n_vertices,
                                          indices_data,
                                          0,
                                          RUT_AXIS_X, 255, 0, 0);

  rut_tesselate_circle_with_line_indices ((CoglVertexP3C4 *)buffer->data +
                                            n_vertices,
                                          n_vertices,
                                          indices_data,
                                          n_vertices,
                                          RUT_AXIS_Y, 0, 255, 0);

  rut_tesselate_circle_with_line_indices ((CoglVertexP3C4 *)buffer->data +
                                            2 * n_vertices,
                                          n_vertices,
                                          indices_data,
                                          n_vertices * 2,
                                          RUT_AXIS_Z, 0, 0, 255);

  mesh = rut_mesh_new_from_buffer_p3c4 (COGL_VERTICES_MODE_LINES,
                                        n_vertices * 3,
                                        buffer);

  rut_refable_unref (buffer);

  rut_mesh_set_indices (mesh,
                        COGL_INDICES_TYPE_UNSIGNED_BYTE,
                        indices_buffer,
                        n_vertices * 2 * 3);

  rut_refable_unref (indices_buffer);

  return mesh;
}

CoglPrimitive *
rut_create_rotation_tool_primitive (RutContext *ctx,
                                    uint8_t n_vertices)
{
  RutMesh *mesh = rut_create_rotation_tool_mesh (n_vertices);
  CoglPrimitive *primitive = rut_mesh_create_primitive (ctx, mesh);
  rut_refable_unref (mesh);

  return primitive;
}

CoglPrimitive *
rut_create_create_grid (RutContext *ctx,
                        float width,
                        float height,
                        float x_space,
                        float y_space)
{
  GArray *lines = g_array_new (FALSE, FALSE, sizeof (CoglVertexP2));
  float x, y;
  int n_lines = 0;

  for (x = 0; x < width; x += x_space)
    {
      CoglVertexP2 p[2] = {
        { .x = x, .y = 0 },
        { .x = x, .y = height }
      };
      g_array_append_vals (lines, p, 2);
      n_lines++;
    }

  for (y = 0; y < height; y += y_space)
    {
      CoglVertexP2 p[2] = {
        { .x = 0, .y = y },
        { .x = width, .y = y }
      };
      g_array_append_vals (lines, p, 2);
      n_lines++;
    }

  return cogl_primitive_new_p2 (ctx->cogl_context,
                                COGL_VERTICES_MODE_LINES,
                                n_lines * 2,
                                (CoglVertexP2 *)lines->data);
}
