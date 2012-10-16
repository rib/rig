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
#include "rut-stack.h"

enum {
  RUT_STACK_PROP_WIDTH,
  RUT_STACK_PROP_HEIGHT,
  RUT_STACK_N_PROPS
};

struct _RutStack
{
  RutObjectProps _parent;

  RutContext *ctx;

  int ref_count;

  RutGraphableProps graphable;

  int width;
  int height;

  GList *children;

  RutSimpleIntrospectableProps introspectable;
  RutProperty properties[RUT_STACK_N_PROPS];
};

static RutPropertySpec _rut_stack_prop_specs[] = {
  {
    .name = "width",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutStack, width),
    .setter = rut_stack_set_width
  },
  {
    .name = "height",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutStack, height),
    .setter = rut_stack_set_height
  },
  { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rut_stack_free (void *object)
{
  RutStack *stack = object;

  rut_refable_unref (stack->ctx);

  g_list_foreach (stack->children, (GFunc)rut_refable_unref, NULL);
  g_list_free (stack->children);

  rut_simple_introspectable_destroy (stack);
  rut_graphable_destroy (stack);

  g_slice_free (RutStack, stack);
}

RutRefCountableVTable _rut_stack_ref_countable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rut_stack_free
};

static void
_rut_stack_child_removed_cb (RutObject *parent, RutObject *child)
{
  RutStack *stack = RUT_STACK (parent);

  stack->children = g_list_remove (stack->children, child);
}

static void
_rut_stack_child_added_cb (RutObject *parent, RutObject *child)
{
  RutStack *stack = RUT_STACK (parent);

  stack->children = g_list_append (stack->children, child);
}

static RutGraphableVTable _rut_stack_graphable_vtable = {
  _rut_stack_child_removed_cb,
  _rut_stack_child_added_cb,
  NULL /* parent changed */
};

static void
rut_stack_get_preferred_width (void *object,
                               float for_height,
                               float *min_width_p,
                               float *natural_width_p)
{
  RutStack *stack = RUT_STACK (object);
  float max_min_width = 0.0f;
  float max_natural_width = 0.0f;
  GList *l;

  for (l = stack->children; l; l = l->next)
    {
      RutObject *child = l->data;
      float child_min_width;
      float child_natural_width;
      rut_sizable_get_preferred_width (child,
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
rut_stack_get_preferred_height (void *object,
                                float for_width,
                                float *min_height_p,
                                float *natural_height_p)
{
  RutStack *stack = RUT_STACK (object);
  float max_min_height = 0.0f;
  float max_natural_height = 0.0f;
  GList *l;

  for (l = stack->children; l; l = l->next)
    {
      RutObject *child = l->data;
      float child_min_height;
      float child_natural_height;
      rut_sizable_get_preferred_height (child,
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

static RutSizableVTable _rut_stack_sizable_vtable = {
  rut_stack_set_size,
  rut_stack_get_size,
  rut_stack_get_preferred_width,
  rut_stack_get_preferred_height
};

static RutIntrospectableVTable _rut_stack_introspectable_vtable = {
  rut_simple_introspectable_lookup_property,
  rut_simple_introspectable_foreach_property
};

RutType rut_stack_type;

static void
_rut_stack_init_type (void)
{
  rut_type_init (&rut_stack_type);
  rut_type_add_interface (&rut_stack_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RutStack, ref_count),
                          &_rut_stack_ref_countable_vtable);
  rut_type_add_interface (&rut_stack_type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (RutStack, graphable),
                          &_rut_stack_graphable_vtable);
  rut_type_add_interface (&rut_stack_type,
                          RUT_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &_rut_stack_sizable_vtable);
  rut_type_add_interface (&rut_stack_type,
                          RUT_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_rut_stack_introspectable_vtable);
  rut_type_add_interface (&rut_stack_type,
                          RUT_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RutStack, introspectable),
                          NULL); /* no implied vtable */
}

void
rut_stack_set_size (RutStack *stack,
                    float width,
                    float height)
{
  GList *l;

  stack->width = width;
  stack->height = height;

  for (l = stack->children; l; l = l->next)
    {
      RutObject *child = l->data;
      if (rut_object_is (child, RUT_INTERFACE_ID_SIZABLE))
        rut_sizable_set_size (child, width, height);
    }

  rut_property_dirty (&stack->ctx->property_ctx,
                      &stack->properties[RUT_STACK_PROP_WIDTH]);
  rut_property_dirty (&stack->ctx->property_ctx,
                      &stack->properties[RUT_STACK_PROP_HEIGHT]);
}

void
rut_stack_set_width (RutStack *stack,
                     float width)
{
  rut_stack_set_size (stack, width, stack->height);
}

void
rut_stack_set_height (RutStack *stack,
                      float height)
{
  rut_stack_set_size (stack, stack->width, height);
}

void
rut_stack_get_size (RutStack *stack,
                    float *width,
                    float *height)
{
  *width = stack->width;
  *height = stack->height;
}

RutStack *
rut_stack_new (RutContext *context,
               float width,
               float height,
               ...)
{
  RutStack *stack = g_slice_new0 (RutStack);
  static CoglBool initialized = FALSE;
  va_list ap;
  RutObject *object;

  if (initialized == FALSE)
    {
      _rut_init ();
      _rut_stack_init_type ();

      initialized = TRUE;
    }

  stack->ref_count = 1;
  stack->ctx = rut_refable_ref (context);

  rut_object_init (&stack->_parent, &rut_stack_type);

  rut_simple_introspectable_init (stack,
                                  _rut_stack_prop_specs,
                                  stack->properties);

  rut_graphable_init (RUT_OBJECT (stack));

  rut_stack_set_size (stack, width, height);

  va_start (ap, height);
  while ((object = va_arg (ap, RutObject *)))
    rut_graphable_add_child (RUT_OBJECT (stack), object);
  va_end (ap);

  return stack;
}

void
rut_stack_append_child (RutStack *stack,
                        RutObject *child)
{
  rut_graphable_add_child (stack, child);
  stack->children = g_list_append (stack->children, child);
}
