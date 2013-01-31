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

#include <cogl/cogl.h>
#include <string.h>
#include <math.h>

#include "rut.h"
#include "rut-bevel.h"

enum {
  RUT_BEVEL_PROP_WIDTH,
  RUT_BEVEL_PROP_HEIGHT,
  RUT_BEVEL_N_PROPS
};

struct _RutBevel
{
  RutObjectProps _parent;

  RutContext *ctx;

  int ref_count;

  RutGraphableProps graphable;
  RutPaintableProps paintable;

  CoglPipeline *pipeline;

  CoglPrimitive *prim;

  CoglColor colors[4];

  int width;
  int height;

  RutSimpleIntrospectableProps introspectable;
  RutProperty properties[RUT_BEVEL_N_PROPS];
};

static RutPropertySpec _rut_bevel_prop_specs[] = {
  {
    .name = "width",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutBevel, width),
    .setter.float_type = rut_bevel_set_width
  },
  {
    .name = "height",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutBevel, height),
    .setter.float_type = rut_bevel_set_height
  },
  { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rut_bevel_free (void *object)
{
  RutBevel *bevel = object;

  rut_refable_ref (bevel->ctx);

  if (bevel->pipeline)
    cogl_object_unref (bevel->pipeline);
  if (bevel->prim)
    cogl_object_unref (bevel->prim);

  rut_simple_introspectable_destroy (bevel);
  rut_graphable_destroy (bevel);

  g_slice_free (RutBevel, bevel);
}

RutRefCountableVTable _rut_bevel_ref_countable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rut_bevel_free
};

static RutGraphableVTable _rut_bevel_graphable_vtable = {
  NULL, /* child removed */
  NULL, /* child addded */
  NULL /* parent changed */
};

#if 0
static CoglPrimitive *
create_bevel_prim (RutBevel *bevel)
{
  CoglVertexP2C4 lines[] =
    {
      {0, 0, 0x80, 0x80, 0x80, 0x80},
      {0, bevel->height, 0x80, 0x80, 0x80, 0x80},

      {0, bevel->height, 0x80, 0x80, 0x80, 0x80},
      {bevel->width, bevel->height, 0x80, 0x80, 0x80, 0x80},

      {bevel->width, bevel->height, 0x80, 0x80, 0x80, 0x80},
      {bevel->width, 0, 0x80, 0x80, 0x80, 0x80},

      {bevel->width, 0, 0x80, 0x80, 0x80, 0x80},
      {0, 0, 0x80, 0x80, 0x80, 0x80},
    };

  return cogl_primitive_new_p2c4 (bevel->ctx->cogl_context,
                                  COGL_VERTICES_MODE_LINES,
                                  8,
                                  lines);
}
#endif

static void
_rut_bevel_paint (RutObject *object,
                  RutPaintContext *paint_ctx)
{
  RutBevel *bevel = (RutBevel *) object;
  RutCamera *camera = paint_ctx->camera;
  CoglFramebuffer *fb = rut_camera_get_framebuffer (camera);

  /* make sure the bevels are pixel aligned... */
  int width = bevel->width;
  int height = bevel->height;

#if 0
  if (!bevel->prim)
    bevel->prim = create_bevel_prim (bevel);

  cogl_framebuffer_draw_primitive (fb, bevel->pipeline, bevel->prim);
#endif

  cogl_pipeline_set_color4f (bevel->pipeline,
                             bevel->colors[0].red,
                             bevel->colors[0].green,
                             bevel->colors[0].blue,
                             bevel->colors[0].alpha);
  cogl_framebuffer_draw_rectangle (fb, bevel->pipeline,
                                   0, 0, width, 1);

  cogl_pipeline_set_color4f (bevel->pipeline,
                             bevel->colors[1].red,
                             bevel->colors[1].green,
                             bevel->colors[1].blue,
                             bevel->colors[1].alpha);
  cogl_framebuffer_draw_rectangle (fb, bevel->pipeline,
                                   width - 1, 0, width, height);

  cogl_pipeline_set_color4f (bevel->pipeline,
                             bevel->colors[2].red,
                             bevel->colors[2].green,
                             bevel->colors[2].blue,
                             bevel->colors[2].alpha);
  cogl_framebuffer_draw_rectangle (fb, bevel->pipeline,
                                   0, height - 1, width, height);

  cogl_pipeline_set_color4f (bevel->pipeline,
                             bevel->colors[3].red,
                             bevel->colors[3].green,
                             bevel->colors[3].blue,
                             bevel->colors[3].alpha);
  cogl_framebuffer_draw_rectangle (fb, bevel->pipeline,
                                   0, 0, 1, height);
}

RutPaintableVTable _rut_bevel_paintable_vtable = {
  _rut_bevel_paint
};

static RutSizableVTable _rut_bevel_sizable_vtable = {
  rut_bevel_set_size,
  rut_bevel_get_size,
  NULL,
  NULL,
  NULL /* add_preferred_size_callback */
};

static RutIntrospectableVTable _rut_bevel_introspectable_vtable = {
  rut_simple_introspectable_lookup_property,
  rut_simple_introspectable_foreach_property
};

RutType rut_bevel_type;

static void
_rut_bevel_init_type (void)
{
  rut_type_init (&rut_bevel_type, "RigBevel");
  rut_type_add_interface (&rut_bevel_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RutBevel, ref_count),
                          &_rut_bevel_ref_countable_vtable);
  rut_type_add_interface (&rut_bevel_type,
                          RUT_INTERFACE_ID_PAINTABLE,
                          offsetof (RutBevel, paintable),
                          &_rut_bevel_paintable_vtable);
  rut_type_add_interface (&rut_bevel_type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (RutBevel, graphable),
                          &_rut_bevel_graphable_vtable);
  rut_type_add_interface (&rut_bevel_type,
                          RUT_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &_rut_bevel_sizable_vtable);
  rut_type_add_interface (&rut_bevel_type,
                          RUT_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_rut_bevel_introspectable_vtable);
  rut_type_add_interface (&rut_bevel_type,
                          RUT_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RutBevel, introspectable),
                          NULL); /* no implied vtable */
}

void
rut_bevel_set_size (RutObject *self,
                    float width,
                    float height)
{
  RutBevel *bevel = self;
  if (bevel->prim)
    {
      cogl_object_unref (bevel->prim);
      bevel->prim = NULL;
    }

  bevel->width = width;
  bevel->height = height;

  rut_property_dirty (&bevel->ctx->property_ctx,
                      &bevel->properties[RUT_BEVEL_PROP_WIDTH]);
  rut_property_dirty (&bevel->ctx->property_ctx,
                      &bevel->properties[RUT_BEVEL_PROP_HEIGHT]);
}

void
rut_bevel_get_size (RutObject *self,
                    float *width,
                    float *height)
{
  RutBevel *bevel = self;
  *width = bevel->width;
  *height = bevel->height;
}

void
rut_bevel_set_width (RutObject *obj,
                     float width)
{
  RutBevel *bevel = RUT_BEVEL (obj);

  rut_bevel_set_size (bevel, width, bevel->height);
}

void
rut_bevel_set_height (RutObject *obj,
                      float height)
{
  RutBevel *bevel = RUT_BEVEL (obj);

  rut_bevel_set_size (bevel, bevel->width, height);
}

RutBevel *
rut_bevel_new (RutContext *context,
               float width,
               float height,
               const CoglColor *reference)
{
  RutBevel *bevel = g_slice_new0 (RutBevel);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rut_init ();
      _rut_bevel_init_type ();

      initialized = TRUE;
    }

  rut_object_init (&bevel->_parent, &rut_bevel_type);

  bevel->ref_count = 1;
  bevel->ctx = rut_refable_ref (context);

  rut_simple_introspectable_init (bevel,
                                  _rut_bevel_prop_specs,
                                  bevel->properties);

  bevel->pipeline = cogl_pipeline_new (context->cogl_context);

  rut_paintable_init (RUT_OBJECT (bevel));
  rut_graphable_init (RUT_OBJECT (bevel));

  bevel->colors[0] = *reference;
  rut_color_lighten (&bevel->colors[0], &bevel->colors[0]);
  rut_color_lighten (&bevel->colors[0], &bevel->colors[0]);

  bevel->colors[1] = *reference;
  rut_color_darken (&bevel->colors[1], &bevel->colors[1]);

  bevel->colors[2] = *reference;
  rut_color_darken (&bevel->colors[2], &bevel->colors[2]);
  rut_color_darken (&bevel->colors[2], &bevel->colors[2]);

  bevel->colors[3] = *reference;
  rut_color_lighten (&bevel->colors[3], &bevel->colors[3]);

  rut_bevel_set_size (bevel, width, height);

  return bevel;
}
