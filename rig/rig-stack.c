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
#include "rig-stack.h"

enum {
  RIG_STACK_PROP_WIDTH,
  RIG_STACK_PROP_HEIGHT,
  RIG_STACK_N_PROPS
};

struct _RigStack
{
  RigObjectProps _parent;

  RigContext *ctx;

  int ref_count;

  RigGraphableProps graphable;

  int width;
  int height;

  GList *children;

  RigSimpleIntrospectableProps introspectable;
  RigProperty properties[RIG_STACK_N_PROPS];
};

static RigPropertySpec _rig_stack_prop_specs[] = {
  {
    .name = "width",
    .type = RIG_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RigStack, width),
    .setter = rig_stack_set_width
  },
  {
    .name = "height",
    .type = RIG_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RigStack, height),
    .setter = rig_stack_set_height
  },
  { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rig_stack_free (void *object)
{
  RigStack *stack = object;

  rig_ref_countable_ref (stack->ctx);

  g_list_foreach (stack->children, (GFunc)rig_ref_countable_unref, NULL);
  g_list_free (stack->children);

  rig_simple_introspectable_destroy (stack);

  g_slice_free (RigStack, stack);
}

RigRefCountableVTable _rig_stack_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_stack_free
};

static void
_rig_stack_child_removed_cb (RigObject *parent, RigObject *child)
{
  RigStack *stack = RIG_STACK (parent);

  stack->children = g_list_remove (stack->children, child);
}

static void
_rig_stack_child_added_cb (RigObject *parent, RigObject *child)
{
  RigStack *stack = RIG_STACK (parent);

  stack->children = g_list_append (stack->children, child);
}

static RigGraphableVTable _rig_stack_graphable_vtable = {
  _rig_stack_child_removed_cb,
  _rig_stack_child_added_cb,
  NULL /* parent changed */
};

static void
rig_stack_get_preferred_width (void *object,
                               float for_height,
                               float *min_width_p,
                               float *natural_width_p)
{
  RigStack *stack = RIG_STACK (object);
  float max_min_width = 0.0f;
  float max_natural_width = 0.0f;
  GList *l;

  for (l = stack->children; l; l = l->next)
    {
      RigObject *child = l->data;
      float child_min_width;
      float child_natural_width;
      rig_sizable_get_preferred_width (child,
                                       for_height,
                                       &child_min_width,
                                       &child_natural_width);
      if (child_min_width > max_min_width)
        max_min_width = child_min_width;
      if (child_natural_width > max_natural_width)
        max_natural_width = child_natural_width;
    }

  if (min_width_p)
    *min_width_p = max_natural_width;
  if (natural_width_p)
    *natural_width_p = max_natural_width;
}

static void
rig_stack_get_preferred_height (void *object,
                                float for_width,
                                float *min_height_p,
                                float *natural_height_p)
{
  RigStack *stack = RIG_STACK (object);
  float max_min_height = 0.0f;
  float max_natural_height = 0.0f;
  GList *l;

  for (l = stack->children; l; l = l->next)
    {
      RigObject *child = l->data;
      float child_min_height;
      float child_natural_height;
      rig_sizable_get_preferred_height (child,
                                        for_width,
                                        &child_min_height,
                                        &child_natural_height);
      if (child_min_height > max_min_height)
        max_min_height = child_min_height;
      if (child_natural_height > max_natural_height)
        max_natural_height = child_natural_height;
    }

  if (min_height_p)
    *min_height_p = max_natural_height;
  if (natural_height_p)
    *natural_height_p = max_natural_height;
}

static RigSizableVTable _rig_stack_sizable_vtable = {
  rig_stack_set_size,
  rig_stack_get_size,
  rig_stack_get_preferred_width,
  rig_stack_get_preferred_height
};

static RigIntrospectableVTable _rig_stack_introspectable_vtable = {
  rig_simple_introspectable_lookup_property,
  rig_simple_introspectable_foreach_property
};

RigType rig_stack_type;

static void
_rig_stack_init_type (void)
{
  rig_type_init (&rig_stack_type);
  rig_type_add_interface (&rig_stack_type,
                          RIG_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigStack, ref_count),
                          &_rig_stack_ref_countable_vtable);
  rig_type_add_interface (&rig_stack_type,
                          RIG_INTERFACE_ID_GRAPHABLE,
                          offsetof (RigStack, graphable),
                          &_rig_stack_graphable_vtable);
  rig_type_add_interface (&rig_stack_type,
                          RIG_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &_rig_stack_sizable_vtable);
  rig_type_add_interface (&rig_stack_type,
                          RIG_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_rig_stack_introspectable_vtable);
  rig_type_add_interface (&rig_stack_type,
                          RIG_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RigStack, introspectable),
                          NULL); /* no implied vtable */
}

void
rig_stack_set_size (RigStack *stack,
                    float width,
                    float height)
{
  GList *l;

  stack->width = width;
  stack->height = height;

  for (l = stack->children; l; l = l->next)
    {
      RigObject *child = l->data;
      if (rig_object_is (child, RIG_INTERFACE_ID_SIZABLE))
        rig_sizable_set_size (child, width, height);
    }

  rig_property_dirty (&stack->ctx->property_ctx,
                      &stack->properties[RIG_STACK_PROP_WIDTH]);
  rig_property_dirty (&stack->ctx->property_ctx,
                      &stack->properties[RIG_STACK_PROP_HEIGHT]);
}

void
rig_stack_set_width (RigStack *stack,
                     float width)
{
  rig_stack_set_size (stack, width, stack->height);
}

void
rig_stack_set_height (RigStack *stack,
                      float height)
{
  rig_stack_set_size (stack, stack->width, height);
}

void
rig_stack_get_size (RigStack *stack,
                    float *width,
                    float *height)
{
  *width = stack->width;
  *height = stack->height;
}

RigStack *
rig_stack_new (RigContext *context,
               float width,
               float height,
               ...)
{
  RigStack *stack = g_slice_new0 (RigStack);
  static CoglBool initialized = FALSE;
  va_list ap;
  RigObject *object;

  if (initialized == FALSE)
    {
      _rig_init ();
      _rig_stack_init_type ();

      initialized = TRUE;
    }

  stack->ref_count = 1;
  stack->ctx = rig_ref_countable_ref (context);

  rig_object_init (&stack->_parent, &rig_stack_type);

  rig_simple_introspectable_init (stack,
                                  _rig_stack_prop_specs,
                                  stack->properties);

  rig_graphable_init (RIG_OBJECT (stack));

  rig_stack_set_size (stack, width, height);

  va_start (ap, height);
  while ((object = va_arg (ap, RigObject *)))
    rig_graphable_add_child (RIG_OBJECT (stack), object);
  va_end (ap);

  return stack;
}

void
rig_stack_append_child (RigStack *stack,
                        RigObject *child)
{
  rig_graphable_add_child (stack, child);
  stack->children = g_list_append (stack->children, child);
}
