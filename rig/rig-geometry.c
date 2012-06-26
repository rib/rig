/*
 * Rig
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

#include <glib.h>
#include <math.h>

#include <cogl/cogl.h>

#include "rig.h"

CoglAttribute *
rig_create_circle (CoglContext *ctx, int subdivisions)
{
  int n_verts = subdivisions + 1;
  struct CircleVert {
      float x, y;
  } *verts;
  size_t buffer_size = sizeof (struct CircleVert) * n_verts;
  int i;
  CoglAttributeBuffer *attribute_buffer;
  CoglAttribute *attribute;

  verts = alloca (buffer_size);

  verts[0].x = 0;
  verts[0].y = 0;
  for (i = 1; i < n_verts; i++)
    {
      float angle =
        2.0f * (float)G_PI * (1.0f/(float)subdivisions) * (float)i;
      verts[i].x = sinf (angle);
      verts[i].y = cosf (angle);
    }
  attribute_buffer = cogl_attribute_buffer_new (ctx, buffer_size, verts);

  attribute = cogl_attribute_new (attribute_buffer,
                                  "cogl_position_in",
                                  sizeof (struct CircleVert),
                                  offsetof (struct CircleVert, x),
                                  2,
                                  COGL_ATTRIBUTE_TYPE_FLOAT);
  return attribute;
}

CoglTexture *
rig_create_circle_texture (RigContext *ctx,
                           int radius_texels,
                           int padding_texels)
{
  CoglAttribute *circle;
  CoglTexture2D *tex2d;
  CoglOffscreen *offscreen;
  CoglFramebuffer *fb;
  CoglPipeline *white_pipeline;
  int half_size = radius_texels + padding_texels;
  int size = half_size * 2;

  tex2d = cogl_texture_2d_new_with_size (ctx->cogl_context,
                                         size, size,
                                         COGL_PIXEL_FORMAT_RGBA_8888,
                                         NULL);
  offscreen = cogl_offscreen_new_to_texture (COGL_TEXTURE (tex2d));
  fb = COGL_FRAMEBUFFER (offscreen);

  circle = rig_create_circle (ctx->cogl_context, 360);

  cogl_framebuffer_clear4f (fb, COGL_BUFFER_BIT_COLOR, 0, 0, 0, 0);

  cogl_framebuffer_identity_matrix (fb);
  cogl_framebuffer_orthographic (fb, 0, 0, size, size, -1, 100);

  cogl_framebuffer_translate (fb, half_size, half_size, 0);
  cogl_framebuffer_scale (fb, radius_texels, radius_texels, 1);

  white_pipeline = cogl_pipeline_new (ctx->cogl_context);
  cogl_pipeline_set_color4f (white_pipeline, 1, 1, 1, 1);

  cogl_framebuffer_draw_attributes (fb,
                                    white_pipeline,
                                    COGL_VERTICES_MODE_TRIANGLE_FAN,
                                    0, /* first vertex */
                                    361, /* n_vertices */
                                    &circle,
                                    1); /* n_attributes */

  cogl_object_unref (white_pipeline);
  cogl_object_unref (circle);
  cogl_object_unref (offscreen);

  return COGL_TEXTURE (tex2d);
}
