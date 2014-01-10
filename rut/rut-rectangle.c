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

#include <cogl/cogl.h>

#include "rut-interfaces.h"
#include "rut-paintable.h"
#include "rut-camera-private.h"
#include "rut-rectangle.h"

struct _RutRectangle
{
  RutObjectBase _base;

  float width;
  float height;

  RutGraphableProps graphable;
  RutPaintableProps paintable;

  CoglPipeline *pipeline;

};

static void
_rut_rectangle_free (void *object)
{
  RutRectangle *rectangle = object;

  cogl_object_unref (rectangle->pipeline);

  rut_graphable_destroy (rectangle);

  rut_object_free (RutRectangle, object);
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

  rut_type_init (type, G_STRINGIFY (TYPE), _rut_rectangle_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_GRAPHABLE,
                      offsetof (TYPE, graphable),
                      &graphable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_PAINTABLE,
                      offsetof (TYPE, paintable),
                      &paintable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_SIZABLE,
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
  RutRectangle *rectangle =
    rut_object_alloc0 (RutRectangle, &rut_rectangle_type, _rut_rectangle_init_type);



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
