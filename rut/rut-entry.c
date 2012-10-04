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
#include "rut-entry.h"

enum {
  RUT_ENTRY_PROP_WIDTH,
  RUT_ENTRY_PROP_HEIGHT,
  RUT_ENTRY_N_PROPS
};

struct _RutEntry
{
  RutObjectProps _parent;

  RutContext *ctx;

  int ref_count;

  RutGraphableProps graphable;
  RutPaintableProps paintable;

  CoglPipeline *pipeline;
  CoglPipeline *circle_pipeline;
  CoglPipeline *border_pipeline;
  CoglPipeline *border_circle_pipeline;

  RutText *text;

  CoglPrimitive *prim;

  RutColor colors[4];

  float width;
  float height;

  RutSimpleIntrospectableProps introspectable;
  RutProperty properties[RUT_ENTRY_N_PROPS];
};

static RutPropertySpec _rut_entry_prop_specs[] = {
  {
    .name = "width",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutEntry, width),
    .setter = rut_entry_set_width
  },
  {
    .name = "height",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutEntry, height),
    .setter = rut_entry_set_height
  },
  { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rut_entry_free (void *object)
{
  RutEntry *entry = object;

  rut_refable_ref (entry->ctx);

  if (entry->pipeline)
    cogl_object_unref (entry->pipeline);
  if (entry->prim)
    cogl_object_unref (entry->prim);

  rut_simple_introspectable_destroy (entry);

  rut_graphable_remove_child (entry->text);
  rut_refable_unref (entry->text);

  rut_graphable_destroy (entry);

  g_slice_free (RutEntry, entry);
}

RutRefCountableVTable _rut_entry_ref_countable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rut_entry_free
};

static RutGraphableVTable _rut_entry_graphable_vtable = {
  NULL, /* child removed */
  NULL, /* child addded */
  NULL /* parent changed */
};

#if 0
static CoglPrimitive *
create_entry_prim (RutEntry *entry)
{
  CoglVertexP2C4 lines[] =
    {
      {0, 0, 0x80, 0x80, 0x80, 0x80},
      {0, entry->height, 0x80, 0x80, 0x80, 0x80},

      {0, entry->height, 0x80, 0x80, 0x80, 0x80},
      {entry->width, entry->height, 0x80, 0x80, 0x80, 0x80},

      {entry->width, entry->height, 0x80, 0x80, 0x80, 0x80},
      {entry->width, 0, 0x80, 0x80, 0x80, 0x80},

      {entry->width, 0, 0x80, 0x80, 0x80, 0x80},
      {0, 0, 0x80, 0x80, 0x80, 0x80},
    };

  return cogl_primitive_new_p2c4 (entry->ctx->cogl_context,
                                  COGL_VERTICES_MODE_LINES,
                                  8,
                                  lines);
}
#endif

static void
_rut_entry_paint (RutObject *object,
                  RutPaintContext *paint_ctx)
{
  RutEntry *entry = (RutEntry *) object;
  RutCamera *camera = paint_ctx->camera;
  CoglFramebuffer *fb = rut_camera_get_framebuffer (camera);

  /* make sure the entrys are pixel aligned... */
  float width = entry->width;
  float height = entry->height;
  float half_height = height / 2.0;


#if 0
  if (!entry->prim)
    entry->prim = create_entry_prim (entry);

  cogl_framebuffer_draw_primitive (fb, entry->pipeline, entry->prim);
#endif

  /* NB ctx->circle_texture is padded such that the texture itself is twice as
   * wide as the circle */
  cogl_framebuffer_draw_rectangle (fb,
                                   entry->circle_pipeline,
                                   -height, -half_height, height, height + half_height);

  cogl_framebuffer_draw_rectangle (fb,
                                   entry->circle_pipeline,
                                   width - height, -half_height, width + height, height + half_height);

  cogl_framebuffer_draw_textured_rectangle (fb,
                                            entry->circle_pipeline,
                                            0, -half_height, width, height + half_height,
                                            0.5, 0, 0.5, 1);


#if 0
  cogl_framebuffer_draw_rectangle (fb,
                                   entry->pipeline,
                                   0, 0, height, height);
  cogl_framebuffer_draw_rectangle (fb, entry->pipeline,
                                   0, 0, width, 1);
  cogl_framebuffer_draw_rectangle (fb, entry->pipeline,
                                   width - 1, 0, width, height);
  cogl_framebuffer_draw_rectangle (fb, entry->pipeline,
                                   0, height - 1, width, height);
  cogl_framebuffer_draw_rectangle (fb, entry->pipeline,
                                   0, 0, 1, height);
#endif
}

void
rut_entry_set_size (RutEntry *entry,
                    float width,
                    float height)
{
  if (entry->prim)
    {
      cogl_object_unref (entry->prim);
      entry->prim = NULL;
    }

  rut_sizable_set_size (entry->text, width, height);

  entry->width = width;
  entry->height = height;

  rut_property_dirty (&entry->ctx->property_ctx,
                      &entry->properties[RUT_ENTRY_PROP_WIDTH]);
  rut_property_dirty (&entry->ctx->property_ctx,
                      &entry->properties[RUT_ENTRY_PROP_HEIGHT]);
}

void
rut_entry_get_size (RutEntry *entry,
                    float *width,
                    float *height)
{
  rut_sizable_get_size (entry->text, width, height);
}

static void
_rut_entry_get_preferred_width (RutObject *object,
                                float for_height,
                                float *min_width_p,
                                float *natural_width_p)
{
  RutEntry *entry = RUT_ENTRY (object);
  rut_sizable_get_preferred_width (entry->text, for_height,
                                   min_width_p, natural_width_p);
}

static void
_rut_entry_get_preferred_height (RutObject *object,
                                 float for_width,
                                 float *min_height_p,
                                 float *natural_height_p)
{
  RutEntry *entry = RUT_ENTRY (object);
  rut_sizable_get_preferred_height (entry->text, for_width,
                                    min_height_p, natural_height_p);
}

RutPaintableVTable _rut_entry_paintable_vtable = {
  _rut_entry_paint
};

static RutSizableVTable _rut_entry_sizable_vtable = {
  rut_entry_set_size,
  rut_entry_get_size,
  _rut_entry_get_preferred_width,
  _rut_entry_get_preferred_height
};

static RutIntrospectableVTable _rut_entry_introspectable_vtable = {
  rut_simple_introspectable_lookup_property,
  rut_simple_introspectable_foreach_property
};

RutType rut_entry_type;

static void
_rut_entry_init_type (void)
{
  rut_type_init (&rut_entry_type);
  rut_type_add_interface (&rut_entry_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RutEntry, ref_count),
                          &_rut_entry_ref_countable_vtable);
  rut_type_add_interface (&rut_entry_type,
                          RUT_INTERFACE_ID_PAINTABLE,
                          offsetof (RutEntry, paintable),
                          &_rut_entry_paintable_vtable);
  rut_type_add_interface (&rut_entry_type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (RutEntry, graphable),
                          &_rut_entry_graphable_vtable);
  rut_type_add_interface (&rut_entry_type,
                          RUT_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &_rut_entry_sizable_vtable);
  rut_type_add_interface (&rut_entry_type,
                          RUT_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_rut_entry_introspectable_vtable);
  rut_type_add_interface (&rut_entry_type,
                          RUT_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RutEntry, introspectable),
                          NULL); /* no implied vtable */
}


void
rut_entry_set_width (RutEntry *entry,
                     float width)
{
  rut_entry_set_size (entry, width, entry->height);
}

void
rut_entry_set_height (RutEntry *entry,
                      float height)
{
  rut_entry_set_size (entry, entry->width, height);
}

RutEntry *
rut_entry_new (RutContext *ctx)
{
  RutEntry *entry = g_slice_new0 (RutEntry);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rut_init ();
      _rut_entry_init_type ();

      initialized = TRUE;
    }

  rut_object_init (&entry->_parent, &rut_entry_type);

  entry->ref_count = 1;
  entry->ctx = rut_refable_ref (ctx);

  rut_simple_introspectable_init (entry,
                                  _rut_entry_prop_specs,
                                  entry->properties);

  entry->pipeline = cogl_pipeline_new (ctx->cogl_context);
  cogl_pipeline_set_color4f (entry->pipeline, 0.87, 0.87, 0.87, 1);

  entry->border_pipeline = cogl_pipeline_copy (entry->pipeline);
  cogl_pipeline_set_color4f (entry->border_pipeline, 1, 1, 1, 1);

  entry->circle_pipeline = cogl_pipeline_copy (entry->pipeline);
  cogl_pipeline_set_layer_texture (entry->circle_pipeline, 0, ctx->circle_texture);
  entry->border_circle_pipeline = cogl_pipeline_copy (entry->circle_pipeline);
  cogl_pipeline_set_color4f (entry->border_circle_pipeline, 1, 1, 1, 1);

  rut_paintable_init (RUT_OBJECT (entry));
  rut_graphable_init (RUT_OBJECT (entry));

  entry->text = rut_text_new (ctx);
  rut_graphable_add_child (entry, entry->text);

  rut_sizable_get_size (entry->text, &entry->width, &entry->height);
  rut_property_set_copy_binding (&ctx->property_ctx,
                                 &entry->properties[RUT_ENTRY_PROP_WIDTH],
                                 rut_introspectable_lookup_property (entry->text, "width"));
  rut_property_set_copy_binding (&ctx->property_ctx,
                                 &entry->properties[RUT_ENTRY_PROP_HEIGHT],
                                 rut_introspectable_lookup_property (entry->text, "height"));
  return entry;
}

RutText *
rut_entry_get_text (RutEntry *entry)
{
  return entry->text;
}
