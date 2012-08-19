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

#include <cogl/cogl.h>
#include <string.h>
#include <math.h>

#include "rig.h"
#include "rig-bevel.h"

enum {
  RIG_BEVEL_PROP_WIDTH,
  RIG_BEVEL_PROP_HEIGHT,
  RIG_BEVEL_N_PROPS
};

struct _RigBevel
{
  RigObjectProps _parent;

  RigContext *ctx;

  int ref_count;

  RigGraphableProps graphable;
  RigPaintableProps paintable;

  CoglPipeline *pipeline;

  CoglPrimitive *prim;

  RigColor colors[4];

  int width;
  int height;

  RigSimpleIntrospectableProps introspectable;
  RigProperty properties[RIG_BEVEL_N_PROPS];
};

static RigPropertySpec _rig_bevel_prop_specs[] = {
  {
    .name = "width",
    .type = RIG_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RigBevel, width),
    .setter = rig_bevel_set_width
  },
  {
    .name = "height",
    .type = RIG_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RigBevel, height),
    .setter = rig_bevel_set_height
  },
  { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rig_bevel_free (void *object)
{
  RigBevel *bevel = object;

  rig_ref_countable_ref (bevel->ctx);

  if (bevel->pipeline)
    cogl_object_unref (bevel->pipeline);
  if (bevel->prim)
    cogl_object_unref (bevel->prim);

  rig_simple_introspectable_destroy (bevel);

  g_slice_free (RigBevel, bevel);
}

RigRefCountableVTable _rig_bevel_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_bevel_free
};

static RigGraphableVTable _rig_bevel_graphable_vtable = {
  NULL, /* child removed */
  NULL, /* child addded */
  NULL /* parent changed */
};

#if 0
static CoglPrimitive *
create_bevel_prim (RigBevel *bevel)
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
_rig_bevel_paint (RigObject *object,
                  RigPaintContext *paint_ctx)
{
  RigBevel *bevel = (RigBevel *) object;
  RigCamera *camera = paint_ctx->camera;
  CoglFramebuffer *fb = rig_camera_get_framebuffer (camera);

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

RigPaintableVTable _rig_bevel_paintable_vtable = {
  _rig_bevel_paint
};

static RigSizableVTable _rig_bevel_sizable_vtable = {
  rig_bevel_set_size,
  rig_bevel_get_size,
  NULL,
  NULL
};

static RigIntrospectableVTable _rig_bevel_introspectable_vtable = {
  rig_simple_introspectable_lookup_property,
  rig_simple_introspectable_foreach_property
};

RigType rig_bevel_type;

static void
_rig_bevel_init_type (void)
{
  rig_type_init (&rig_bevel_type);
  rig_type_add_interface (&rig_bevel_type,
                          RIG_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigBevel, ref_count),
                          &_rig_bevel_ref_countable_vtable);
  rig_type_add_interface (&rig_bevel_type,
                          RIG_INTERFACE_ID_PAINTABLE,
                          offsetof (RigBevel, paintable),
                          &_rig_bevel_paintable_vtable);
  rig_type_add_interface (&rig_bevel_type,
                          RIG_INTERFACE_ID_GRAPHABLE,
                          offsetof (RigBevel, graphable),
                          &_rig_bevel_graphable_vtable);
  rig_type_add_interface (&rig_bevel_type,
                          RIG_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &_rig_bevel_sizable_vtable);
  rig_type_add_interface (&rig_bevel_type,
                          RIG_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_rig_bevel_introspectable_vtable);
  rig_type_add_interface (&rig_bevel_type,
                          RIG_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RigBevel, introspectable),
                          NULL); /* no implied vtable */
}

void
rig_bevel_set_size (RigBevel *bevel,
                    float width,
                    float height)
{
  if (bevel->prim)
    {
      cogl_object_unref (bevel->prim);
      bevel->prim = NULL;
    }

  bevel->width = width;
  bevel->height = height;

  rig_property_dirty (&bevel->ctx->property_ctx,
                      &bevel->properties[RIG_BEVEL_PROP_WIDTH]);
  rig_property_dirty (&bevel->ctx->property_ctx,
                      &bevel->properties[RIG_BEVEL_PROP_HEIGHT]);
}

void
rig_bevel_get_size (RigBevel *bevel,
                    float *width,
                    float *height)
{
  *width = bevel->width;
  *height = bevel->height;
}

void
rig_bevel_set_width (RigBevel *bevel,
                     float width)
{
  rig_bevel_set_size (bevel, width, bevel->height);
}

void
rig_bevel_set_height (RigBevel *bevel,
                      float height)
{
  rig_bevel_set_size (bevel, bevel->width, height);
}

RigBevel *
rig_bevel_new (RigContext *context,
               float width,
               float height,
               const RigColor *reference)
{
  RigBevel *bevel = g_slice_new0 (RigBevel);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rig_init ();
      _rig_bevel_init_type ();

      initialized = TRUE;
    }

  rig_object_init (&bevel->_parent, &rig_bevel_type);

  bevel->ref_count = 1;
  bevel->ctx = rig_ref_countable_ref (context);

  rig_simple_introspectable_init (bevel,
                                  _rig_bevel_prop_specs,
                                  bevel->properties);

  bevel->pipeline = cogl_pipeline_new (context->cogl_context);

  rig_paintable_init (RIG_OBJECT (bevel));
  rig_graphable_init (RIG_OBJECT (bevel));

  bevel->colors[0] = *reference;
  rig_color_lighten (&bevel->colors[0], &bevel->colors[0]);
  rig_color_lighten (&bevel->colors[0], &bevel->colors[0]);

  bevel->colors[1] = *reference;
  rig_color_darken (&bevel->colors[1], &bevel->colors[1]);

  bevel->colors[2] = *reference;
  rig_color_darken (&bevel->colors[2], &bevel->colors[2]);
  rig_color_darken (&bevel->colors[2], &bevel->colors[2]);

  bevel->colors[3] = *reference;
  rig_color_lighten (&bevel->colors[3], &bevel->colors[3]);

  rig_bevel_set_size (bevel, width, height);

  return bevel;
}
