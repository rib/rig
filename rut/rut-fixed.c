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
#include "rut-fixed.h"

struct _RutFixed
{
  RutObjectProps _parent;
  int ref_count;

  RutContext *context;

  RutList preferred_size_cb_list;

  float width;
  float height;

  RutGraphableProps graphable;
};

static void
_rut_fixed_free (void *object)
{
  RutFixed *fixed = object;

  rut_closure_list_disconnect_all (&fixed->preferred_size_cb_list);

  rut_graphable_destroy (fixed);

  g_slice_free (RutFixed, object);
}

static void
rut_fixed_get_preferred_width (void *sizable,
                               float for_height,
                               float *min_width_p,
                               float *natural_width_p)
{
  RutFixed *fixed = sizable;

  if (min_width_p)
    *min_width_p = fixed->width;
  if (natural_width_p)
    *natural_width_p = fixed->width;
}

static void
rut_fixed_get_preferred_height (void *sizable,
                                float for_width,
                                float *min_height_p,
                                float *natural_height_p)
{
  RutFixed *fixed = sizable;

  if (min_height_p)
    *min_height_p = fixed->height;
  if (natural_height_p)
    *natural_height_p = fixed->height;
}

RutType rut_fixed_type;

static void
_rut_fixed_init_type (void)
{
  static RutRefableVTable refable_vtable = {
      rut_refable_simple_ref,
      rut_refable_simple_unref,
      _rut_fixed_free
  };

  static RutGraphableVTable graphable_vtable = {
      NULL, /* child remove */
      NULL, /* child add */
      NULL /* parent changed */
  };

  static RutSizableVTable sizable_vtable = {
      rut_fixed_set_size,
      rut_fixed_get_size,
      rut_fixed_get_preferred_width,
      rut_fixed_get_preferred_height,
      NULL /* add_preferred_size_callback */
  };

  RutType *type = &rut_fixed_type;
#define TYPE RutFixed

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
                          RUT_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &sizable_vtable);

#undef TYPE
}

RutFixed *
rut_fixed_new (RutContext *ctx,
               float width,
               float height)
{
  RutFixed *fixed = g_slice_new0 (RutFixed);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rut_fixed_init_type ();
      initialized = TRUE;
    }

  rut_object_init (&fixed->_parent, &rut_fixed_type);

  fixed->ref_count = 1;

  fixed->context = ctx;

  rut_list_init (&fixed->preferred_size_cb_list);

  rut_graphable_init (fixed);

  fixed->width = width;
  fixed->height = height;

  return fixed;
}

void
rut_fixed_set_width (RutFixed *fixed, float width)
{
  rut_fixed_set_size (fixed, width, fixed->height);
}

void
rut_fixed_set_height (RutFixed *fixed, float height)
{
  rut_fixed_set_size (fixed, fixed->width, height);
}

void
rut_fixed_set_size (RutObject *self,
                    float width,
                    float height)
{
  RutFixed *fixed = self;

  if (fixed->width == width && fixed->height == height)
    return;

  fixed->width = width;
  fixed->height = height;

  rut_closure_list_invoke (&fixed->preferred_size_cb_list,
                           RutSizablePreferredSizeCallback,
                           fixed);
}

void
rut_fixed_get_size (RutObject *self,
                    float *width,
                    float *height)
{
  RutFixed *fixed = self;
  *width = fixed->width;
  *height = fixed->height;
}

void
rut_fixed_add_child (RutFixed *fixed, RutObject *child)
{
  g_return_if_fail (rut_object_get_type (fixed) == &rut_fixed_type);
  rut_graphable_add_child (fixed, child);
}

void
rut_fixed_remove_child (RutFixed *fixed, RutObject *child)
{
  g_return_if_fail (rut_object_get_type (fixed) == &rut_fixed_type);
  rut_graphable_remove_child (child);
}
