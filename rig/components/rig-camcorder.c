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
#include "rig-camcorder.h"

typedef struct
{
  float x, y, z, w;
  float r, g, b, a;
} RigVertex4C4;

static void
rig_camcorder_update (RigComponent *component,
                      int64_t       time)
{
  RigCamcorder *camcorder = RIG_CAMCORDER (component);

  if (CAMCORDER_HAS_FLAG (camcorder, PROJECTION_DIRTY))
    {
      CAMCORDER_CLEAR_FLAG (camcorder, PROJECTION_DIRTY);

      if (CAMCORDER_HAS_FLAG (camcorder, ORTHOGRAPHIC))
        {
          cogl_framebuffer_orthographic (camcorder->fb,
                                         camcorder->size,
                                         camcorder->size,
                                         -camcorder->size,
                                         -camcorder->size,
                                         camcorder->z_near,
                                         camcorder->z_far);
        }
      else /* perspective */
        {
          float aspect_ratio;

          aspect_ratio = (float) cogl_framebuffer_get_width (camcorder->fb) /
                         cogl_framebuffer_get_height (camcorder->fb);

          cogl_framebuffer_perspective (camcorder->fb,
                                        camcorder->fov,
                                        aspect_ratio,
                                        camcorder->z_near,
                                        camcorder->z_far);
        }
    }
}

static void
draw_frustum (RigCamcorder    *camcorder,
              CoglFramebuffer *fb,
              RigVertex4C4     vertices[8])
{
  CoglAttributeBuffer *attribute_buffer;
  CoglAttribute *attributes[2];
  CoglPrimitive *primitive;
  CoglPipeline *pipeline;
  CoglIndices *indices;
  unsigned char indices_data[24] = {
      0,1, 1,2, 2,3, 3,0,
      4,5, 5,6, 6,7, 7,4,
      0,4, 1,5, 2,6, 3,7
  };
  CoglDepthState depth_state;

  attribute_buffer = cogl_attribute_buffer_new (rig_cogl_context,
                                                8 * sizeof (RigVertex4C4),
                                                vertices);

  attributes[0] = cogl_attribute_new (attribute_buffer,
                                      "cogl_position_in",
                                      sizeof (RigVertex4C4),
                                      offsetof (RigVertex4C4, x),
                                      3,
                                      COGL_ATTRIBUTE_TYPE_FLOAT);

  attributes[1] = cogl_attribute_new (attribute_buffer,
                                      "cogl_color_in",
                                      sizeof (RigVertex4C4),
                                      offsetof (RigVertex4C4, r),
                                      4,
                                      COGL_ATTRIBUTE_TYPE_FLOAT);


  indices = cogl_indices_new (rig_cogl_context,
                              COGL_INDICES_TYPE_UNSIGNED_BYTE,
                              indices_data,
                              G_N_ELEMENTS (indices_data));

  primitive = cogl_primitive_new_with_attributes (COGL_VERTICES_MODE_LINES,
                                                  8, attributes, 2);

  cogl_primitive_set_indices (primitive, indices, G_N_ELEMENTS(indices_data));

  cogl_object_unref (attribute_buffer);
  cogl_object_unref (attributes[0]);
  cogl_object_unref (attributes[1]);
  cogl_object_unref (indices);

  pipeline = cogl_pipeline_new (rig_cogl_context);

  /* enable depth testing */
  cogl_depth_state_init (&depth_state);
  cogl_depth_state_set_test_enabled (&depth_state, TRUE);
  cogl_pipeline_set_depth_state (pipeline, &depth_state, NULL);

  cogl_framebuffer_draw_primitive (fb, pipeline, primitive);

  cogl_object_unref (primitive);
  cogl_object_unref (pipeline);
}

static void
rig_camcorder_draw (RigComponent    *component,
                    CoglFramebuffer *fb)
{
  RigCamcorder *camcorder = RIG_CAMCORDER (component);
  float r, g, b;

  if (camcorder->fb == fb)
    {
      /* we are responsible for that fb, let's clear it */
      r = cogl_color_get_red_float (&camcorder->background_color);
      g = cogl_color_get_green_float (&camcorder->background_color);
      b = cogl_color_get_blue_float (&camcorder->background_color);

      cogl_framebuffer_clear4f (camcorder->fb,
                                COGL_BUFFER_BIT_COLOR |
                                COGL_BUFFER_BIT_DEPTH | COGL_BUFFER_BIT_STENCIL,
                                r, g, b, 1);
    }
  else
    {
      /* When the fb we have to draw to is not the one this camcorder is
       * responsible for, we can draw its frustum to visualize it */
      RigVertex4C4 vertices[8] = {
          /* near plane in projection space */
          {-1, -1, -1, 1, /* position */ .8, .8, .8, .8 /* color */ },
          { 1, -1, -1, 1,                .8, .8, .8, .8             },
          { 1,  1, -1, 1,                .8, .8, .8, .8             },
          {-1,  1, -1, 1,                .8, .8, .8, .8             },
          /* far plane in projection space */
          {-1, -1, 1, 1,  /* position */ .3, .3, .3, .3 /* color */ },
          { 1, -1, 1, 1,                 .3, .3, .3, .3             },
          { 1,  1, 1, 1,                 .3, .3, .3, .3             },
          {-1,  1, 1, 1,                 .3, .3, .3, .3             }
      };
      CoglMatrix projection, projection_inv;
      int i;

      cogl_framebuffer_get_projection_matrix (camcorder->fb, &projection);
      cogl_matrix_get_inverse (&projection, &projection_inv);

      for (i = 0; i < 8; i++)
        {
          cogl_matrix_transform_point (&projection_inv,
                                       &vertices[i].x,
                                       &vertices[i].y,
                                       &vertices[i].z,
                                       &vertices[i].w);
          vertices[i].x /= vertices[i].w;
          vertices[i].y /= vertices[i].w;
          vertices[i].z /= vertices[i].w;
          vertices[i].w /= 1.0f;
        }

      draw_frustum (camcorder, fb, vertices);
    }
}

RigComponent *
rig_camcorder_new (void)
{
  RigCamcorder *camcorder;

  camcorder = g_slice_new0 (RigCamcorder);
  camcorder->component.update = rig_camcorder_update;
  camcorder->component.draw = rig_camcorder_draw;

  CAMCORDER_SET_FLAG (camcorder, PROJECTION_DIRTY);

  return RIG_COMPONENT (camcorder);
}

void
rig_camcorder_free (RigCamcorder *camcorder)
{
  g_slice_free (RigCamcorder, camcorder);
}

void
rig_camcorder_set_near_plane (RigCamcorder *camcorder,
                              float         z_near)
{
  camcorder->z_near = z_near;
  CAMCORDER_SET_FLAG (camcorder, PROJECTION_DIRTY);
}

void
rig_camcorder_set_far_plane (RigCamcorder *camcorder,
                             float         z_far)
{
  camcorder->z_far = z_far;
  CAMCORDER_SET_FLAG (camcorder, PROJECTION_DIRTY);
}

CoglFramebuffer *
rig_camcorder_get_framebuffer (RigCamcorder *camcorder)
{
  return camcorder->fb;
}

void
rig_camcorder_set_framebuffer (RigCamcorder    *camcorder,
                               CoglFramebuffer *fb)
{
  if (camcorder->fb)
    {
      cogl_object_unref (camcorder->fb);
      camcorder->fb = NULL;
    }

  if (fb)
    camcorder->fb = cogl_object_ref (fb);
}

RigProjection
rig_camcorder_get_projection (RigCamcorder *camcorder)
{
  if (CAMCORDER_HAS_FLAG (camcorder, ORTHOGRAPHIC))
    return RIG_PROJECTION_ORTHOGRAPHIC;
  else
    return RIG_PROJECTION_PERSPECTIVE;
}

void
rig_camcorder_set_projection (RigCamcorder  *camcorder,
                              RigProjection  projection)
{
  if (projection == RIG_PROJECTION_ORTHOGRAPHIC)
    CAMCORDER_SET_FLAG (camcorder, ORTHOGRAPHIC);
  else
    CAMCORDER_CLEAR_FLAG (camcorder, ORTHOGRAPHIC);
}

void
rig_camcorder_set_field_of_view (RigCamcorder *camcorder,
                                 float         fov)
{
  camcorder->fov = fov;
  CAMCORDER_SET_FLAG (camcorder, PROJECTION_DIRTY);
}

void
rig_camcorder_set_size_of_view (RigCamcorder *camcorder,
                                float         sov)
{
  camcorder->size = sov;
  CAMCORDER_SET_FLAG (camcorder, PROJECTION_DIRTY);
}

void
rig_camcorder_set_background_color (RigCamcorder *camcorder,
                                    CoglColor    *color)
{
  camcorder->background_color = *color;
}
