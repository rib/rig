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

#include <stdbool.h>

#include "rig-global.h"
#include "rig-camcorder.h"

typedef struct
{
  float x, y, z, w;
  float r, g, b, a;
} RigVertex4C4;

static void
rig_camcorder_update (RigObject *object,
                      int64_t time)
{
  RigCamcorder *camcorder = object;

  if (camcorder->projection_dirty)
    {
      camcorder->projection_dirty = FALSE;

      if (camcorder->orthographic)
        {
          cogl_matrix_orthographic (&camcorder->projection,
                                    camcorder->right,
                                    camcorder->top,
                                    camcorder->left,
                                    camcorder->bottom,
                                    camcorder->z_near,
                                    camcorder->z_far);
        }
      else /* perspective */
        {
          float aspect_ratio;

          aspect_ratio = (float) cogl_framebuffer_get_width (camcorder->fb) /
                         cogl_framebuffer_get_height (camcorder->fb);

          cogl_matrix_perspective (&camcorder->projection,
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
rig_camcorder_draw (RigObject *object,
                    CoglFramebuffer *fb)
{
  RigCamcorder *camcorder = object;
  float r, g, b;

  if (camcorder->fb == fb)
    {
      /* we are responsible for drawing in that fb, let's set up the camera */

      /* this is a no-op if the viewport stays the same on the framebuffer */
      cogl_framebuffer_set_viewport (fb,
                                     camcorder->viewport[0],
                                     camcorder->viewport[1],
                                     camcorder->viewport[2],
                                     camcorder->viewport[3]);

      cogl_framebuffer_set_projection_matrix (fb, &camcorder->projection);

      if (camcorder->clear_fb)
        {
          r = cogl_color_get_red_float (&camcorder->background_color);
          g = cogl_color_get_green_float (&camcorder->background_color);
          b = cogl_color_get_blue_float (&camcorder->background_color);

          cogl_framebuffer_clear4f (camcorder->fb,
                                    COGL_BUFFER_BIT_COLOR |
                                    COGL_BUFFER_BIT_DEPTH |
                                    COGL_BUFFER_BIT_STENCIL,
                                    r, g, b, 1);
        }
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
      CoglMatrix projection_inv;
      int i;

      cogl_matrix_get_inverse (&camcorder->projection, &projection_inv);

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

RigType rig_camcorder_type;

static RigComponentableVTable _rig_camcorder_componentable_vtable = {
  .update = rig_camcorder_update,
  .draw = rig_camcorder_draw
};

void
_rig_camcorder_init_type (void)
{
  rig_type_init (&rig_camcorder_type);
  rig_type_add_interface (&rig_camcorder_type,
                           RIG_INTERFACE_ID_COMPONENTABLE,
                           offsetof (RigCamcorder, component),
                           &_rig_camcorder_componentable_vtable);
}

RigCamcorder *
rig_camcorder_new (void)
{
  RigCamcorder *camcorder;

  camcorder = g_slice_new0 (RigCamcorder);
  rig_object_init (&camcorder->_parent, &rig_camcorder_type);

  camcorder->component.type = RIG_COMPONENT_TYPE_CAMCORDER;

  cogl_matrix_init_identity (&camcorder->projection);
  camcorder->projection_dirty = TRUE;
  camcorder->clear_fb = TRUE;

  return camcorder;
}

void
rig_camcorder_free (RigCamcorder *camcorder)
{
  g_slice_free (RigCamcorder, camcorder);
}

void
rig_camcorder_set_clear (RigCamcorder *camcorder,
                         bool          clear)
{
  if (clear)
    camcorder->clear_fb = TRUE;
  else
    camcorder->clear_fb = FALSE;
}

void
rig_camcorder_set_near_plane (RigCamcorder *camcorder,
                              float         z_near)
{
  camcorder->z_near = z_near;
  camcorder->projection_dirty = TRUE;
}

float
rig_camcorder_get_near_plane (RigCamcorder *camcorder)
{
  return camcorder->z_near;
}

void
rig_camcorder_set_far_plane (RigCamcorder *camcorder,
                             float         z_far)
{
  camcorder->z_far = z_far;
  camcorder->projection_dirty = TRUE;
}

float
rig_camcorder_get_far_plane (RigCamcorder *camcorder)
{
  return camcorder->z_far;
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
    {
      float w, h;

      camcorder->fb = cogl_object_ref (fb);

      /* the viewport defaults to be the whole framebuffer */
      w = cogl_framebuffer_get_width (fb);
      h = cogl_framebuffer_get_height (fb);

      camcorder->viewport[0] = 0;
      camcorder->viewport[1] = 0;
      camcorder->viewport[2] = w;
      camcorder->viewport[3] = h;
    }
}

RigProjection
rig_camcorder_get_projection_mode (RigCamcorder *camcorder)
{
  if (camcorder->orthographic)
    return RIG_PROJECTION_ORTHOGRAPHIC;
  else
    return RIG_PROJECTION_PERSPECTIVE;
}

void
rig_camcorder_set_projection_mode (RigCamcorder  *camcorder,
                                   RigProjection  projection)
{
  if (projection == RIG_PROJECTION_ORTHOGRAPHIC)
    camcorder->orthographic = TRUE;
  else
    camcorder->orthographic = FALSE;
}

void
rig_camcorder_set_field_of_view (RigCamcorder *camcorder,
                                 float         fov)
{
  camcorder->fov = fov;
  camcorder->projection_dirty = TRUE;
}

void
rig_camcorder_set_size_of_view (RigCamcorder *camcorder,
                                float         right,
                                float         top,
                                float         left,
                                float         bottom)
{
  camcorder->right = right;
  camcorder->top = top;
  camcorder->left = left;
  camcorder->bottom = bottom;
  camcorder->projection_dirty = TRUE;
}

void
rig_camcorder_set_background_color (RigCamcorder *camcorder,
                                    CoglColor    *color)
{
  camcorder->background_color = *color;
}

float *
rig_camcorder_get_viewport (RigCamcorder *camcorder)
{
  return camcorder->viewport;
}

void
rig_camcorder_set_viewport (RigCamcorder *camcorder,
                            float         viewport[4])
{
  memcpy (camcorder->viewport, viewport, sizeof (float) * 4);
}

CoglMatrix *
rig_camcorder_get_projection (RigCamcorder *camcorder)
{
  return &camcorder->projection;
}
