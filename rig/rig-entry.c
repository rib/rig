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
#include "rig-entry.h"

enum {
  RIG_ENTRY_PROP_WIDTH,
  RIG_ENTRY_PROP_HEIGHT,
  RIG_ENTRY_N_PROPS
};

struct _RigEntry
{
  RigObjectProps _parent;

  RigContext *ctx;

  int ref_count;

  RigGraphableProps graphable;
  RigPaintableProps paintable;

  CoglPipeline *pipeline;
  CoglPipeline *circle_pipeline;
  CoglPipeline *border_pipeline;
  CoglPipeline *border_circle_pipeline;

  RigText *text;

  CoglPrimitive *prim;

  RigColor colors[4];

  float width;
  float height;

  RigSimpleIntrospectableProps introspectable;
  RigProperty properties[RIG_ENTRY_N_PROPS];
};

static RigPropertySpec _rig_entry_prop_specs[] = {
  {
    .name = "width",
    .type = RIG_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RigEntry, width),
    .setter = rig_entry_set_width
  },
  {
    .name = "height",
    .type = RIG_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RigEntry, height),
    .setter = rig_entry_set_height
  },
  { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rig_entry_free (void *object)
{
  RigEntry *entry = object;

  rig_ref_countable_ref (entry->ctx);

  if (entry->pipeline)
    cogl_object_unref (entry->pipeline);
  if (entry->prim)
    cogl_object_unref (entry->prim);

  rig_simple_introspectable_destroy (entry);

  rig_graphable_remove_child (entry->text);
  rig_ref_countable_unref (entry->text);

  g_slice_free (RigEntry, entry);
}

RigRefCountableVTable _rig_entry_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_entry_free
};

static RigGraphableVTable _rig_entry_graphable_vtable = {
  NULL, /* child removed */
  NULL, /* child addded */
  NULL /* parent changed */
};

#if 0
static CoglPrimitive *
create_entry_prim (RigEntry *entry)
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
_rig_entry_paint (RigObject *object,
                  RigPaintContext *paint_ctx)
{
  RigEntry *entry = (RigEntry *) object;
  RigCamera *camera = paint_ctx->camera;
  CoglFramebuffer *fb = rig_camera_get_framebuffer (camera);

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

RigPaintableVTable _rig_entry_paintable_vtable = {
  _rig_entry_paint
};

static RigSizableVTable _rig_entry_sizable_vtable = {
  rig_entry_set_size,
  rig_entry_get_size,
  NULL,
  NULL
};

static RigIntrospectableVTable _rig_entry_introspectable_vtable = {
  rig_simple_introspectable_lookup_property,
  rig_simple_introspectable_foreach_property
};

RigType rig_entry_type;

static void
_rig_entry_init_type (void)
{
  rig_type_init (&rig_entry_type);
  rig_type_add_interface (&rig_entry_type,
                          RIG_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigEntry, ref_count),
                          &_rig_entry_ref_countable_vtable);
  rig_type_add_interface (&rig_entry_type,
                          RIG_INTERFACE_ID_PAINTABLE,
                          offsetof (RigEntry, paintable),
                          &_rig_entry_paintable_vtable);
  rig_type_add_interface (&rig_entry_type,
                          RIG_INTERFACE_ID_GRAPHABLE,
                          offsetof (RigEntry, graphable),
                          &_rig_entry_graphable_vtable);
  rig_type_add_interface (&rig_entry_type,
                          RIG_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &_rig_entry_sizable_vtable);
  rig_type_add_interface (&rig_entry_type,
                          RIG_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_rig_entry_introspectable_vtable);
  rig_type_add_interface (&rig_entry_type,
                          RIG_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RigEntry, introspectable),
                          NULL); /* no implied vtable */
}

void
rig_entry_set_size (RigEntry *entry,
                    float width,
                    float height)
{
  if (entry->prim)
    {
      cogl_object_unref (entry->prim);
      entry->prim = NULL;
    }

  rig_sizable_set_size (entry->text, width, height);

  entry->width = width;
  entry->height = height;

  rig_property_dirty (&entry->ctx->property_ctx,
                      &entry->properties[RIG_ENTRY_PROP_WIDTH]);
  rig_property_dirty (&entry->ctx->property_ctx,
                      &entry->properties[RIG_ENTRY_PROP_HEIGHT]);
}

void
rig_entry_get_size (RigEntry *entry,
                    float *width,
                    float *height)
{
  rig_sizable_get_size (entry->text, width, height);
}

void
rig_entry_set_width (RigEntry *entry,
                     float width)
{
  rig_entry_set_size (entry, width, entry->height);
}

void
rig_entry_set_height (RigEntry *entry,
                      float height)
{
  rig_entry_set_size (entry, entry->width, height);
}

RigEntry *
rig_entry_new (RigContext *ctx)
{
  RigEntry *entry = g_slice_new0 (RigEntry);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rig_init ();
      _rig_entry_init_type ();

      initialized = TRUE;
    }

  rig_object_init (&entry->_parent, &rig_entry_type);

  entry->ref_count = 1;
  entry->ctx = rig_ref_countable_ref (ctx);

  rig_simple_introspectable_init (entry,
                                  _rig_entry_prop_specs,
                                  entry->properties);

  entry->pipeline = cogl_pipeline_new (ctx->cogl_context);
  cogl_pipeline_set_color4f (entry->pipeline, 0.87, 0.87, 0.87, 1);

  entry->border_pipeline = cogl_pipeline_copy (entry->pipeline);
  cogl_pipeline_set_color4f (entry->border_pipeline, 1, 1, 1, 1);

  entry->circle_pipeline = cogl_pipeline_copy (entry->pipeline);
  cogl_pipeline_set_layer_texture (entry->circle_pipeline, 0, ctx->circle_texture);
  entry->border_circle_pipeline = cogl_pipeline_copy (entry->circle_pipeline);
  cogl_pipeline_set_color4f (entry->border_circle_pipeline, 1, 1, 1, 1);

  rig_paintable_init (RIG_OBJECT (entry));
  rig_graphable_init (RIG_OBJECT (entry));

  entry->text = rig_text_new (ctx);
  rig_graphable_add_child (entry, entry->text);

  rig_sizable_get_size (entry->text, &entry->width, &entry->height);
  rig_property_set_copy_binding (&ctx->property_ctx,
                                 &entry->properties[RIG_ENTRY_PROP_WIDTH],
                                 rig_introspectable_lookup_property (entry->text, "width"));
  rig_property_set_copy_binding (&ctx->property_ctx,
                                 &entry->properties[RIG_ENTRY_PROP_HEIGHT],
                                 rig_introspectable_lookup_property (entry->text, "height"));
  return entry;
}

RigText *
rig_entry_get_text (RigEntry *entry)
{
  return entry->text;
}
