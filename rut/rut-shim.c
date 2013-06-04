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
#include "rut-shim.h"

struct _RutShim
{
  RutObjectProps _parent;
  int ref_count;

  RutContext *context;

  RutList preferred_size_cb_list;

  float width;
  float height;

  RutObject *child;

  RutGraphableProps graphable;
};

static void
_rut_shim_free (void *object)
{
  RutShim *shim = object;

  rut_closure_list_disconnect_all (&shim->preferred_size_cb_list);

  rut_graphable_destroy (shim);

  g_slice_free (RutShim, object);
}

static void
allocate_cb (RutObject *graphable,
             void *user_data)
{
  RutShim *shim = graphable;

  if (shim->child)
    rut_sizable_set_size (shim->child, shim->width, shim->height);
}

static void
queue_allocation (RutShim *shim)
{
  rut_shell_add_pre_paint_callback (shim->context->shell,
                                    shim,
                                    allocate_cb,
                                    NULL /* user_data */);
}

static void
rut_shim_get_preferred_width (void *sizable,
                              float for_height,
                              float *min_width_p,
                              float *natural_width_p)
{
  RutShim *shim = sizable;

  if (min_width_p)
    *min_width_p = shim->width;
  if (natural_width_p)
    *natural_width_p = shim->width;
}

static void
rut_shim_get_preferred_height (void *sizable,
                                float for_width,
                                float *min_height_p,
                                float *natural_height_p)
{
  RutShim *shim = sizable;

  if (min_height_p)
    *min_height_p = shim->height;
  if (natural_height_p)
    *natural_height_p = shim->height;
}

static RutClosure *
rut_shim_add_preferred_size_callback (void *object,
                                      RutSizablePreferredSizeCallback cb,
                                      void *user_data,
                                      RutClosureDestroyCallback destroy)
{
  RutShim *shim = object;

  return rut_closure_list_add (&shim->preferred_size_cb_list,
                               cb,
                               user_data,
                               destroy);
}


RutType rut_shim_type;

static void
_rut_shim_init_type (void)
{
  static RutRefCountableVTable refable_vtable = {
      rut_refable_simple_ref,
      rut_refable_simple_unref,
      _rut_shim_free
  };

  static RutGraphableVTable graphable_vtable = {
      NULL, /* child remove */
      NULL, /* child add */
      NULL /* parent changed */
  };

  static RutSizableVTable sizable_vtable = {
      rut_shim_set_size,
      rut_shim_get_size,
      rut_shim_get_preferred_width,
      rut_shim_get_preferred_height,
      rut_shim_add_preferred_size_callback
  };

  RutType *type = &rut_shim_type;
#define TYPE RutShim

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

RutShim *
rut_shim_new (RutContext *ctx,
              float width,
              float height)
{
  RutShim *shim = g_slice_new0 (RutShim);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rut_shim_init_type ();
      initialized = TRUE;
    }

  rut_object_init (&shim->_parent, &rut_shim_type);

  shim->ref_count = 1;

  shim->context = ctx;

  rut_list_init (&shim->preferred_size_cb_list);

  rut_graphable_init (shim);

  shim->width = width;
  shim->height = height;

  return shim;
}

void
rut_shim_set_width (RutShim *shim, float width)
{
  rut_shim_set_size (shim, width, shim->height);
}

void
rut_shim_set_height (RutShim *shim, float height)
{
  rut_shim_set_size (shim, shim->width, height);
}

void
rut_shim_set_size (RutObject *self,
                   float width,
                   float height)
{
  RutShim *shim = self;

  if (shim->width == width && shim->height == height)
    return;

  shim->width = width;
  shim->height = height;

  rut_closure_list_invoke (&shim->preferred_size_cb_list,
                           RutSizablePreferredSizeCallback,
                           shim);
}

void
rut_shim_get_size (RutObject *self,
                   float *width,
                   float *height)
{
  RutShim *shim = self;
  *width = shim->width;
  *height = shim->height;
}

void
rut_shim_set_child (RutShim *shim, RutObject *child)
{
  g_return_if_fail (rut_object_get_type (shim) == &rut_shim_type);

  if (shim->child == child)
    return;

  if (shim->child)
    {
      rut_graphable_remove_child (shim->child);
      rut_refable_unref (shim->child);
    }

  if (child)
    {
      shim->child = rut_refable_ref (child);
      rut_graphable_add_child (shim, child);
      queue_allocation (shim);
    }
  else
    shim->child = NULL;

  rut_shell_queue_redraw (shim->context->shell);
}

void
rut_shim_remove_child (RutShim *shim, RutObject *child)
{
  g_return_if_fail (rut_object_get_type (shim) == &rut_shim_type);
  rut_graphable_remove_child (child);
}
