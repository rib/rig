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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "rig-diamond.h"
#include "rig-global.h"
#include "math.h"

static void
_diamond_slice_free (void *object)
{
  RigDiamondSlice *diamond_slice = object;

  cogl_object_unref (diamond_slice->pipeline);
  cogl_object_unref (diamond_slice->primitive);

  g_slice_free (RigDiamondSlice, object);
}

static RigRefCountableVTable _diamond_slice_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _diamond_slice_free
};

RigType rig_diamond_slice_type;

void
_rig_diamond_slice_init_type (void)
{
  rig_type_init (&rig_diamond_slice_type);
  rig_type_add_interface (&rig_diamond_slice_type,
                           RIG_INTERFACE_ID_REF_COUNTABLE,
                           offsetof (RigDiamondSlice, ref_count),
                           &_diamond_slice_ref_countable_vtable);
}

typedef struct _VertexP2T2T2
{
  float x, y, s0, t0, s1, t1;
} VertexP2T2T2;

static CoglPrimitive *
primitive_new_p2t2t2 (CoglContext *ctx,
                      CoglVerticesMode mode,
                      int n_vertices,
                      const VertexP2T2T2 *data)
{
  CoglAttributeBuffer *attribute_buffer =
    cogl_attribute_buffer_new (ctx, n_vertices * sizeof (VertexP2T2T2), data);
  CoglAttribute *attributes[3];
  CoglPrimitive *primitive;
  int i;

  attributes[0] = cogl_attribute_new (attribute_buffer,
                                      "cogl_position_in",
                                      sizeof (VertexP2T2T2),
                                      offsetof (VertexP2T2T2, x),
                                      2,
                                      COGL_ATTRIBUTE_TYPE_FLOAT);
  attributes[1] = cogl_attribute_new (attribute_buffer,
                                      "cogl_tex_coord0_in",
                                      sizeof (VertexP2T2T2),
                                      offsetof (VertexP2T2T2, s0),
                                      2,
                                      COGL_ATTRIBUTE_TYPE_FLOAT);
  attributes[2] = cogl_attribute_new (attribute_buffer,
                                      "cogl_tex_coord1_in",
                                      sizeof (VertexP2T2T2),
                                      offsetof (VertexP2T2T2, s1),
                                      2,
                                      COGL_ATTRIBUTE_TYPE_FLOAT);

  cogl_object_unref (attribute_buffer);

  primitive = cogl_primitive_new_with_attributes (mode,
                                                  n_vertices,
                                                  attributes,
                                                  3);

  for (i = 0; i < 3; i++)
    cogl_object_unref (attributes[i]);

  return primitive;
}

RigDiamondSlice *
diamond_slice_new (RigContext *ctx,
                   float size,
                   int tex_width,
                   int tex_height)
{
  RigDiamondSlice *diamond_slice = g_slice_new (RigDiamondSlice);
  float width = size;
  float height = size;
#define DIAMOND_SLICE_CORNER_RADIUS 20
  CoglMatrix matrix;
  float tex_aspect;
  float t;

  rig_object_init (&diamond_slice->_parent, &rig_diamond_slice_type);

  diamond_slice->ref_count = 1;

  diamond_slice->size = size;

  diamond_slice->pipeline = cogl_pipeline_new (ctx->cogl_context);
  cogl_pipeline_set_layer_texture (diamond_slice->pipeline, 0, ctx->circle_texture);

  /* XXX: For the sake of setting up a pipeline that can be used as a
   * template for rendering the diamond shape we are adding a second
   * texture 2D layer, but it's arbitrary that we are using the
   * circle_texture here. */
  cogl_pipeline_set_layer_texture (diamond_slice->pipeline, 1,
                                   ctx->circle_texture);

    {
      /* x0,y0,x1,y1 and s0,t0,s1,t1 define the postion and texture
       * coordinates for the center rectangle... */
      float x0 = DIAMOND_SLICE_CORNER_RADIUS;
      float y0 = DIAMOND_SLICE_CORNER_RADIUS;
      float x1 = width - DIAMOND_SLICE_CORNER_RADIUS;
      float y1 = height - DIAMOND_SLICE_CORNER_RADIUS;

      /* The center region of the nine-slice can simply map to the
       * degenerate center of the circle */
      float s0 = 0.5;
      float t0 = 0.5;
      float s1 = 0.5;
      float t1 = 0.5;

      int n_vertices;
      int i;

      /*
       * 0,0      x0,0      x1,0      width,0
       * 0,0      s0,0      s1,0      1,0
       * 0        1         2         3
       *
       * 0,y0     x0,y0     x1,y0     width,y0
       * 0,t0     s0,t0     s1,t0     1,t0
       * 4        5         6         7
       *
       * 0,y1     x0,y1     x1,y1     width,y1
       * 0,t1     s0,t1     s1,t1     1,t1
       * 8        9         10        11
       *
       * 0,height x0,height x1,height width,height
       * 0,1      s0,1      s1,1      1,1
       * 12       13        14        15
       */

      VertexP2T2T2 vertices[] =
        {
          { 0,  0, 0, 0, 0, 0 },
          { x0, 0, s0, 0, x0, 0},
          { x1, 0, s1, 0, x1, 0},
          { width, 0, 1, 0, width, 0},

          { 0, y0, 0, t0, 0, y0},
          { x0, y0, s0, t0, x0, y0},
          { x1, y0, s1, t0, x1, y0},
          { width, y0, 1, t0, width, y0},

          { 0, y1, 0, t1, 0, y1},
          { x0, y1, s0, t1, x0, y1},
          { x1, y1, s1, t1, x1, y1},
          { width, y1, 1, t1, width, y1},

          { 0, height, 0, 1, 0, height},
          { x0, height, s0, 1, x0, height},
          { x1, height, s1, 1, x1, height},
          { width, height, 1, 1, width, height},
        };

      cogl_matrix_init_identity (&diamond_slice->rotate_matrix);
      cogl_matrix_rotate (&diamond_slice->rotate_matrix, 45, 0, 0, 1);
      cogl_matrix_translate (&diamond_slice->rotate_matrix, - width / 2.0, - height / 2.0, 0);

      n_vertices = sizeof (vertices) / sizeof (VertexP2T2T2);
      for (i = 0; i < n_vertices; i++)
        {
          float z = 0, w = 1;

          cogl_matrix_transform_point (&diamond_slice->rotate_matrix,
                                       &vertices[i].x,
                                       &vertices[i].y,
                                       &z,
                                       &w);
        }

      cogl_matrix_init_identity (&matrix);
      tex_aspect = (float)tex_width / (float)tex_height;

      t = 0.5 / sinf (G_PI_4);

      /* FIXME: hack */
      cogl_matrix_translate (&matrix, 0.5, 0, 0);

      if (tex_aspect < 1) /* taller than it is wide */
        {
          float s_scale = (1.0 / width) * t;

          float t_scale = s_scale * (1.0 / tex_aspect);

          cogl_matrix_scale (&matrix, s_scale, t_scale, t_scale);
        }
      else /* wider than it is tall */
        {
          float t_scale = (1.0 / height) * t;

          float s_scale = t_scale * tex_aspect;

          cogl_matrix_scale (&matrix, s_scale, t_scale, t_scale);
        }

      cogl_matrix_rotate (&matrix, 45, 0, 0, 1);

      n_vertices = sizeof (vertices) / sizeof (VertexP2T2T2);
      for (i = 0; i < n_vertices; i++)
        {
          float z = 0, w = 1;

          cogl_matrix_transform_point (&matrix,
                                       &vertices[i].s1,
                                       &vertices[i].t1,
                                       &z,
                                       &w);
        }


      diamond_slice->primitive =
        primitive_new_p2t2t2 (ctx->cogl_context,
                              COGL_VERTICES_MODE_TRIANGLES,
                              n_vertices,
                              vertices);

      /* The vertices uploaded only map to the key intersection points of the
       * 9-slice grid which isn't a topology that GPUs can handle directly so
       * this specifies an array of indices that allow the GPU to interpret the
       * vertices as a list of triangles... */
      cogl_primitive_set_indices (diamond_slice->primitive,
                                  ctx->nine_slice_indices,
                                  sizeof (_rig_nine_slice_indices_data) /
                                  sizeof (_rig_nine_slice_indices_data[0]));
    }

  return diamond_slice;
}

CoglPipeline *
rig_diamond_slice_get_pipeline_template (RigDiamondSlice *slice)
{
  return slice->pipeline;
}

RigType rig_diamond_type;

static RigComponentableVTable _rig_diamond_componentable_vtable = {
    0
};

static RigPrimableVTable _rig_diamond_primable_vtable = {
  .get_primitive = rig_diamond_get_primitive
};

void
_rig_diamond_init_type (void)
{
  rig_type_init (&rig_diamond_type);
  rig_type_add_interface (&rig_diamond_type,
                          RIG_INTERFACE_ID_COMPONENTABLE,
                          offsetof (RigDiamond, component),
                          &_rig_diamond_componentable_vtable);
  rig_type_add_interface (&rig_diamond_type,
                          RIG_INTERFACE_ID_PRIMABLE,
                          0, /* no associated properties */
                          &_rig_diamond_primable_vtable);
}

RigDiamond *
rig_diamond_new (RigContext *ctx,
                 float size,
                 int tex_width,
                 int tex_height)
{
  RigDiamond *diamond = g_slice_new0 (RigDiamond);

  rig_object_init (&diamond->_parent, &rig_diamond_type);
  diamond->component.type = RIG_COMPONENT_TYPE_GEOMETRY;

  diamond->ctx = rig_ref_countable_ref (ctx);

  diamond->size = size;

  /* XXX: It could be worth maintaining a cache of diamond slices
   * indexed by the <size, tex_width, tex_height> tuple... */
  diamond->slice = diamond_slice_new (ctx, size, tex_width, tex_height);

  return diamond;
}

RigDiamondSlice *
rig_diamond_get_slice (RigDiamond *diamond)
{
  return diamond->slice;
}

float
rig_diamond_get_size (RigDiamond *diamond)
{
  return diamond->size;
}

CoglPrimitive *
rig_diamond_get_primitive (RigObject *object)
{
  RigDiamond *diamond = object;
  return diamond->slice->primitive;
}

void
rig_diamond_apply_mask (RigDiamond *diamond,
                        CoglPipeline *pipeline)
{
  RigContext *ctx = diamond->ctx;
  cogl_pipeline_set_layer_texture (pipeline,
                                   0,
                                   ctx->circle_texture);
}

