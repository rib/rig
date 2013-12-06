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

#include <config.h>

#include <cogl/cogl.h>
#include <string.h>
#include <math.h>

#include "rut-context.h"
#include "rut-closure.h"
#include "rut-interfaces.h"
#include "rut-list.h"
#include "rut-stack.h"

enum {
  RUT_STACK_PROP_WIDTH,
  RUT_STACK_PROP_HEIGHT,
  RUT_STACK_N_PROPS
};

typedef struct
{
  RutList list_node;

  RutClosure *preferred_size_closure;

  RutObject *child;
} RutStackChild;

struct _RutStack
{
  RutObjectProps _parent;

  RutContext *ctx;

  int ref_count;

  RutGraphableProps graphable;

  int width;
  int height;

  RutList children;

  RutList preferred_size_cb_list;

  RutSimpleIntrospectableProps introspectable;
  RutProperty properties[RUT_STACK_N_PROPS];
};

static RutPropertySpec _rut_stack_prop_specs[] = {
  {
    .name = "width",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutStack, width),
    .setter.float_type = rut_stack_set_width
  },
  {
    .name = "height",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutStack, height),
    .setter.float_type = rut_stack_set_height
  },
  { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rut_stack_free (void *object)
{
  RutStack *stack = object;

  rut_simple_introspectable_destroy (stack);
  rut_graphable_destroy (stack);

  rut_shell_remove_pre_paint_callback_by_graphable (stack->ctx->shell, stack);

  /* Destroying the graphable state should remove all the children */
  g_warn_if_fail (rut_list_empty (&stack->children));

  g_slice_free (RutStack, stack);
}

RutRefableVTable _rut_stack_refable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rut_stack_free
};

static void
allocate_cb (RutObject *graphable,
             void *user_data)
{
  RutStack *stack = graphable;
  RutStackChild *child_data;

  rut_list_for_each (child_data, &stack->children, list_node)
    {
      RutObject *child = child_data->child;
      if (rut_object_is (child, RUT_INTERFACE_ID_SIZABLE))
        rut_sizable_set_size (child, stack->width, stack->height);
    }
}

static void
queue_allocation (RutStack *stack)
{
  rut_shell_add_pre_paint_callback (stack->ctx->shell,
                                    stack,
                                    allocate_cb,
                                    NULL /* user_data */);
}

static void
preferred_size_changed (RutStack *stack)
{
  rut_closure_list_invoke (&stack->preferred_size_cb_list,
                           RutSizablePreferredSizeCallback,
                           stack);
}

static void
_rut_stack_child_removed_cb (RutObject *parent, RutObject *child)
{
  RutStack *stack = parent;
  RutStackChild *child_data;

  /* non-sizable children are allowed but we don't track any
   * child-data for them... */
  if (!rut_object_is (child, RUT_INTERFACE_ID_SIZABLE))
    return;

  rut_list_for_each (child_data, &stack->children, list_node)
    if (child_data->child == child)
      {
        rut_closure_disconnect (child_data->preferred_size_closure);
        rut_list_remove (&child_data->list_node);
        g_slice_free (RutStackChild, child_data);
        rut_refable_release (child, parent);

        preferred_size_changed (stack);
        if (!rut_list_empty (&stack->children))
          queue_allocation (stack);
        return;
      }

  g_warn_if_reached ();
}

static void
child_preferred_size_cb (RutObject *sizable,
                         void *user_data)
{
  RutStack *stack = user_data;

  preferred_size_changed (stack);
  queue_allocation (stack);
}

static void
_rut_stack_child_added_cb (RutObject *parent, RutObject *child)
{
  RutStack *stack = parent;
  RutStackChild *child_data;

  /* non-sizable children are allowed but we don't track any
   * child-data for them... */
  if (!rut_object_is (child, RUT_INTERFACE_ID_SIZABLE))
    return;

  child_data = g_slice_new (RutStackChild);
  child_data->child = rut_refable_claim (child, parent);

  child_data->preferred_size_closure =
    rut_sizable_add_preferred_size_callback (child,
                                             child_preferred_size_cb,
                                             stack,
                                             NULL /* destroy */);

  rut_list_insert (stack->children.prev, &child_data->list_node);

  preferred_size_changed (stack);
  queue_allocation (stack);
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
  RutStack *stack = object;
  float max_min_width = 0.0f;
  float max_natural_width = 0.0f;
  RutStackChild *child_data;

  rut_list_for_each (child_data, &stack->children, list_node)
    {
      RutObject *child = child_data->child;
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
    *min_width_p = max_min_width;
  if (natural_width_p)
    *natural_width_p = max_natural_width;
}

static void
rut_stack_get_preferred_height (void *object,
                                float for_width,
                                float *min_height_p,
                                float *natural_height_p)
{
  RutStack *stack = object;
  float max_min_height = 0.0f;
  float max_natural_height = 0.0f;
  RutStackChild *child_data;

  rut_list_for_each (child_data, &stack->children, list_node)
    {
      RutObject *child = child_data->child;
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
    *min_height_p = max_min_height;
  if (natural_height_p)
    *natural_height_p = max_natural_height;
}

static RutClosure *
rut_stack_add_preferred_size_callback (void *object,
                                       RutSizablePreferredSizeCallback cb,
                                       void *user_data,
                                       RutClosureDestroyCallback destroy)
{
  RutStack *stack = object;

  return rut_closure_list_add (&stack->preferred_size_cb_list,
                               cb,
                               user_data,
                               destroy);
}

static RutSizableVTable _rut_stack_sizable_vtable = {
  rut_stack_set_size,
  rut_stack_get_size,
  rut_stack_get_preferred_width,
  rut_stack_get_preferred_height,
  rut_stack_add_preferred_size_callback
};

static RutIntrospectableVTable _rut_stack_introspectable_vtable = {
  rut_simple_introspectable_lookup_property,
  rut_simple_introspectable_foreach_property
};

RutType rut_stack_type;

static void
_rut_stack_init_type (void)
{
  rut_type_init (&rut_stack_type, "RigStack");
  rut_type_add_interface (&rut_stack_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RutStack, ref_count),
                          &_rut_stack_refable_vtable);
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
rut_stack_set_size (RutObject *self,
                    float width,
                    float height)
{
  RutStack *stack = self;

  if (stack->width == width && stack->height == height)
    return;

  stack->width = width;
  stack->height = height;

  rut_property_dirty (&stack->ctx->property_ctx,
                      &stack->properties[RUT_STACK_PROP_WIDTH]);
  rut_property_dirty (&stack->ctx->property_ctx,
                      &stack->properties[RUT_STACK_PROP_HEIGHT]);

  queue_allocation (stack);
}

void
rut_stack_set_width (RutObject *self,
                     float width)
{
  RutStack *stack = self;

  rut_stack_set_size (stack, width, stack->height);
}

void
rut_stack_set_height (RutObject *self,
                      float height)
{
  RutStack *stack = self;

  rut_stack_set_size (stack, stack->width, height);
}

void
rut_stack_get_size (RutObject *self,
                    float *width,
                    float *height)
{
  RutStack *stack = self;

  *width = stack->width;
  *height = stack->height;
}

RutStack *
rut_stack_new (RutContext *context,
               float width,
               float height)
{
  RutStack *stack = rut_object_alloc0 (RutStack,
                                       &rut_stack_type,
                                       _rut_stack_init_type);

  stack->ref_count = 1;
  stack->ctx = context;

  rut_list_init (&stack->children);
  rut_list_init (&stack->preferred_size_cb_list);

  rut_simple_introspectable_init (stack,
                                  _rut_stack_prop_specs,
                                  stack->properties);

  rut_graphable_init (stack);

  rut_stack_set_size (stack, width, height);

  queue_allocation (stack);

  return stack;
}

void
rut_stack_add (RutStack *stack,
               RutObject *child)
{
  rut_graphable_add_child (stack, child);
}
