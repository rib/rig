/*
 * Rut.
 *
 * Copyright (C) 2013  Intel Corporation.
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

#include "rut-interfaces.h"
#include "rut-transform-private.h"
#include "rut-paintable.h"
#include "rut-camera-private.h"
#include "rut-graphables.h"

struct _RutGraph
{
  RutObjectProps _parent;
  int ref_count;

  RutGraphableProps graphable;
};

struct _RutRectangle
{
  RutObjectProps _parent;
  int ref_count;

  float width;
  float height;

  RutGraphableProps graphable;
  RutPaintableProps paintable;

  CoglPipeline *pipeline;

};

struct _RutNineSlice
{
  RutObjectProps _parent;
  int ref_count;

  CoglTexture *texture;

  float left;
  float right;
  float top;
  float bottom;

  float width;
  float height;

  CoglPipeline *pipeline;

  RutGraphableProps graphable;
  RutPaintableProps paintable;
};


static void
_rut_graph_free (void *object)
{
  RutGraph *graph = object;

  rut_graphable_destroy (graph);

  g_slice_free (RutGraph, object);
}

RutType rut_graph_type;

static void
_rut_graph_init_type (void)
{
  static RutRefCountableVTable refable_vtable = {
      rut_refable_simple_ref,
      rut_refable_simple_unref,
      _rut_graph_free
  };

  static RutGraphableVTable graphable_vtable = {
      NULL, /* child remove */
      NULL, /* child add */
      NULL /* parent changed */
  };

  RutType *type = &rut_graph_type;
#define TYPE RutGraph

  rut_type_init (type, G_STRINGIFY (TYPE));
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (TYPE, ref_count),
                          &refable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (TYPE, graphable),
                          &graphable_vtable);

#undef TYPE
}

RutGraph *
rut_graph_new (RutContext *ctx)
{
  RutGraph *graph = g_slice_new (RutGraph);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rut_graph_init_type ();
      initialized = TRUE;
    }

  rut_object_init (&graph->_parent, &rut_graph_type);

  graph->ref_count = 1;

  rut_graphable_init (graph);

  return graph;
}

static void
_rut_transform_free (void *object)
{
  RutTransform *transform = object;

  rut_graphable_destroy (transform);

  g_slice_free (RutTransform, object);
}

RutType rut_transform_type;

static void
_rut_transform_init_type (void)
{
  static RutRefCountableVTable refable_vtable = {
      rut_refable_simple_ref,
      rut_refable_simple_unref,
      _rut_transform_free
  };

  static RutGraphableVTable graphable_vtable = {
      NULL, /* child remove */
      NULL, /* child add */
      NULL /* parent changed */
  };

  static RutTransformableVTable transformable_vtable = {
      rut_transform_get_matrix
  };

  RutType *type = &rut_transform_type;
#define TYPE RutTransform

  rut_type_init (type, G_STRINGIFY (TYPE));
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (TYPE, ref_count),
                          &refable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (TYPE, graphable),
                          &graphable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_TRANSFORMABLE,
                          0,
                          &transformable_vtable);

#undef TYPE
}

RutTransform *
rut_transform_new (RutContext *ctx)
{
  RutTransform *transform = g_slice_new (RutTransform);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rut_transform_init_type ();
      initialized = TRUE;
    }

  rut_object_init (&transform->_parent, &rut_transform_type);

  transform->ref_count = 1;

  rut_graphable_init (transform);

  cogl_matrix_init_identity (&transform->matrix);

  return transform;
}

void
rut_transform_translate (RutTransform *transform,
                         float x,
                         float y,
                         float z)
{
  cogl_matrix_translate (&transform->matrix, x, y, z);
}

void
rut_transform_quaternion_rotate (RutTransform *transform,
                                 const CoglQuaternion *quaternion)
{
  CoglMatrix rotation;
  cogl_matrix_init_from_quaternion (&rotation, quaternion);
  cogl_matrix_multiply (&transform->matrix, &transform->matrix, &rotation);
}

void
rut_transform_rotate (RutTransform *transform,
                      float angle,
                      float x,
                      float y,
                      float z)
{
  cogl_matrix_rotate (&transform->matrix, angle, x, y, z);
}
void
rut_transform_scale (RutTransform *transform,
                     float x,
                     float y,
                     float z)
{
  cogl_matrix_scale (&transform->matrix, x, y, z);
}

void
rut_transform_init_identity (RutTransform *transform)
{
  cogl_matrix_init_identity (&transform->matrix);
}

const CoglMatrix *
rut_transform_get_matrix (RutObject *self)
{
  RutTransform *transform = self;
  return &transform->matrix;
}

static void
_rut_rectangle_free (void *object)
{
  RutRectangle *rectangle = object;

  cogl_object_unref (rectangle->pipeline);

  rut_graphable_destroy (rectangle);

  g_slice_free (RutRectangle, object);
}

static void
_rut_rectangle_paint (RutObject *object,
                      RutPaintContext *paint_ctx)
{
  RutRectangle *rectangle = object;
  RutCamera *camera = paint_ctx->camera;

  cogl_framebuffer_draw_rectangle (camera->fb,
                                   rectangle->pipeline,
                                   0, 0,
                                   rectangle->width,
                                   rectangle->height);
}

RutType rut_rectangle_type;

static void
_rut_rectangle_init_type (void)
{
  static RutRefCountableVTable refable_vtable = {
      rut_refable_simple_ref,
      rut_refable_simple_unref,
      _rut_rectangle_free
  };

  static RutGraphableVTable graphable_vtable = {
      NULL, /* child remove */
      NULL, /* child add */
      NULL /* parent changed */
  };

  static RutPaintableVTable paintable_vtable = {
      _rut_rectangle_paint
  };

  static RutSizableVTable sizable_vtable = {
      rut_rectangle_set_size,
      rut_rectangle_get_size,
      rut_simple_sizable_get_preferred_width,
      rut_simple_sizable_get_preferred_height,
      NULL /* add_preferred_size_callback */
  };

  RutType *type = &rut_rectangle_type;
#define TYPE RutRectangle

  rut_type_init (type, G_STRINGIFY (TYPE));
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (TYPE, ref_count),
                          &refable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (TYPE, graphable),
                          &graphable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_PAINTABLE,
                          offsetof (TYPE, paintable),
                          &paintable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &sizable_vtable);

#undef TYPE
}

RutRectangle *
rut_rectangle_new4f (RutContext *ctx,
                     float width,
                     float height,
                     float red,
                     float green,
                     float blue,
                     float alpha)
{
  RutRectangle *rectangle = g_slice_new0 (RutRectangle);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rut_rectangle_init_type ();
      initialized = TRUE;
    }

  rut_object_init (&rectangle->_parent, &rut_rectangle_type);

  rectangle->ref_count = 1;

  rut_graphable_init (rectangle);
  rut_paintable_init (rectangle);

  rectangle->width = width;
  rectangle->height = height;

  rectangle->pipeline = cogl_pipeline_new (ctx->cogl_context);
  cogl_pipeline_set_color4f (rectangle->pipeline,
                             red, green, blue, alpha);

  return rectangle;
}

void
rut_rectangle_set_width (RutRectangle *rectangle, float width)
{
  rectangle->width = width;
}

void
rut_rectangle_set_height (RutRectangle *rectangle, float height)
{
  rectangle->height = height;
}

void
rut_rectangle_set_size (RutObject *self,
                        float width,
                        float height)
{
  RutRectangle *rectangle = self;
  rectangle->width = width;
  rectangle->height = height;
}

void
rut_rectangle_get_size (RutObject *self,
                        float *width,
                        float *height)
{
  RutRectangle *rectangle = self;
  *width = rectangle->width;
  *height = rectangle->height;
}

static void
_rut_nine_slice_free (void *object)
{
  RutNineSlice *nine_slice = object;

  cogl_object_unref (nine_slice->texture);

  cogl_object_unref (nine_slice->pipeline);

  rut_graphable_destroy (nine_slice);

  g_slice_free (RutNineSlice, object);
}

static void
_rut_nine_slice_paint (RutObject *object,
                       RutPaintContext *paint_ctx)
{
  RutNineSlice *nine_slice = object;
  RutCamera *camera = paint_ctx->camera;
  CoglFramebuffer *fb = camera->fb;

  float left = nine_slice->left;
  float right = nine_slice->right;
  float top = nine_slice->top;
  float bottom = nine_slice->bottom;

  /* simple stretch */
  if (left == 0 && right == 0 && top == 0 && bottom == 0)
    {
      cogl_framebuffer_draw_rectangle (fb,
                                       nine_slice->pipeline,
                                       0, 0,
                                       nine_slice->width,
                                       nine_slice->height);
    }
  else
    {
      float width = nine_slice->width;
      float height = nine_slice->height;
      CoglTexture *texture = nine_slice->texture;
      float tex_width = cogl_texture_get_width (texture);
      float tex_height = cogl_texture_get_height (texture);

      /* s0,t0,s1,t1 define the texture coordinates for the center
       * rectangle... */
      float s0 = left / tex_width;
      float t0 = top / tex_height;
      float s1 = (tex_width - right) / tex_width;
      float t1 = (tex_height - bottom) / tex_height;

      float ex;
      float ey;

      ex = width - right;
      if (ex < left)
        ex = left;

      ey = height - bottom;
      if (ey < top)
        ey = top;

      {
        float rectangles[] =
          {
            /* top left corner */
            0, 0,
            left, top,
            0.0, 0.0,
            s0, t0,

            /* top middle */
            left, 0,
            MAX (left, ex), top,
            s0, 0.0,
            s1, t0,

            /* top right */
            ex, 0,
            MAX (ex + right, width), top,
            s1, 0.0,
            1.0, t0,

            /* mid left */
            0, top,
            left,  ey,
            0.0, t0,
            s0, t1,

            /* center */
            left, top,
            ex, ey,
            s0, t0,
            s1, t1,

            /* mid right */
            ex, top,
            MAX (ex + right, width), ey,
            s1, t0,
            1.0, t1,

            /* bottom left */
            0, ey,
            left, MAX (ey + bottom, height),
            0.0, t1,
            s0, 1.0,

            /* bottom center */
            left, ey,
            ex, MAX (ey + bottom, height),
            s0, t1,
            s1, 1.0,

            /* bottom right */
            ex, ey,
            MAX (ex + right, width), MAX (ey + bottom, height),
            s1, t1,
            1.0, 1.0
          };

          cogl_framebuffer_draw_textured_rectangles (fb,
                                                     nine_slice->pipeline,
                                                     rectangles,
                                                     9);
      }
    }
}

RutType rut_nine_slice_type;

static void
_rut_nine_slice_init_type (void)
{
  static RutRefCountableVTable refable_vtable = {
      rut_refable_simple_ref,
      rut_refable_simple_unref,
      _rut_nine_slice_free
  };

  static RutGraphableVTable graphable_vtable = {
      NULL, /* child remove */
      NULL, /* child add */
      NULL /* parent changed */
  };

  static RutPaintableVTable paintable_vtable = {
      _rut_nine_slice_paint
  };

  static RutSizableVTable sizable_vtable = {
      rut_nine_slice_set_size,
      rut_nine_slice_get_size,
      rut_simple_sizable_get_preferred_width,
      rut_simple_sizable_get_preferred_height,
      NULL /* add_preferred_size_callback */
  };

  RutType *type = &rut_nine_slice_type;
#define TYPE RutNineSlice

  rut_type_init (type, G_STRINGIFY (TYPE));
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (TYPE, ref_count),
                          &refable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (TYPE, graphable),
                          &graphable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_PAINTABLE,
                          offsetof (TYPE, paintable),
                          &paintable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &sizable_vtable);

#undef TYPE
}

RutNineSlice *
rut_nine_slice_new (RutContext *ctx,
                    CoglTexture *texture,
                    float top,
                    float right,
                    float bottom,
                    float left,
                    float width,
                    float height)
{
  RutNineSlice *nine_slice = g_slice_new (RutNineSlice);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rut_nine_slice_init_type ();
      initialized = TRUE;
    }

  rut_object_init (&nine_slice->_parent, &rut_nine_slice_type);

  nine_slice->ref_count = 1;

  rut_graphable_init (nine_slice);

  nine_slice->texture = cogl_object_ref (texture);

  nine_slice->left = left;
  nine_slice->right = right;
  nine_slice->top = top;
  nine_slice->bottom = bottom;

  nine_slice->width = width;
  nine_slice->height = height;

  nine_slice->pipeline = cogl_pipeline_copy (ctx->single_texture_2d_template);
  cogl_pipeline_set_layer_texture (nine_slice->pipeline, 0, texture);

  return nine_slice;
}

CoglTexture *
rut_nine_slice_get_texture (RutNineSlice *nine_slice)
{
  return nine_slice->texture;
}

void
rut_nine_slice_set_size (RutObject *self,
                         float width,
                         float height)
{
  RutNineSlice *nine_slice = self;
  nine_slice->width = width;
  nine_slice->height = height;
}

void
rut_nine_slice_get_size (RutObject *self,
                         float *width,
                         float *height)
{
  RutNineSlice *nine_slice = self;
  *width = nine_slice->width;
  *height = nine_slice->height;
}

CoglPipeline *
rut_nine_slice_get_pipeline (RutNineSlice *nine_slice)
{
  return nine_slice->pipeline;
}
