/*
 * Rig
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

#include "rig-closure.h"

void
rig_closure_disconnect (RigClosure *closure)
{
  rig_list_remove (&closure->list_node);

  if (closure->destroy_cb)
    closure->destroy_cb (closure->user_data);

  g_slice_free (RigClosure, closure);
}

void
rig_closure_list_disconnect_all (RigList *list)
{
  while (!rig_list_empty (list))
    {
      RigClosure *closure = rig_container_of (list->next, closure, list_node);
      rig_closure_disconnect (closure);
    }
}

RigClosure *
rig_closure_list_add (RigList *list,
                      void *function,
                      void *user_data,
                      RigClosureDestroyCallback destroy_cb)
{
  RigClosure *closure = g_slice_new (RigClosure);

  closure->function = function;
  closure->user_data = user_data;
  closure->destroy_cb = destroy_cb;

  rig_list_insert (list->prev, &closure->list_node);

  return closure;
}
