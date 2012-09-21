/*
 * Rut
 *
 * A tiny toolkit
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>

#include "rut-closure.h"

void
rut_closure_disconnect (RutClosure *closure)
{
  rut_list_remove (&closure->list_node);

  if (closure->destroy_cb)
    closure->destroy_cb (closure->user_data);

  g_slice_free (RutClosure, closure);
}

void
rut_closure_list_disconnect_all (RutList *list)
{
  while (!rut_list_empty (list))
    {
      RutClosure *closure = rut_container_of (list->next, closure, list_node);
      rut_closure_disconnect (closure);
    }
}

RutClosure *
rut_closure_list_add (RutList *list,
                      void *function,
                      void *user_data,
                      RutClosureDestroyCallback destroy_cb)
{
  RutClosure *closure = g_slice_new (RutClosure);

  closure->function = function;
  closure->user_data = user_data;
  closure->destroy_cb = destroy_cb;

  rut_list_insert (list->prev, &closure->list_node);

  return closure;
}
