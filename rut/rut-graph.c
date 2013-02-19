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
#include "rut-graph.h"

struct _RutGraph
{
  RutObjectProps _parent;
  int ref_count;

  RutGraphableProps graphable;
};

static void
_rut_graph_free (void *object)
{
  RutGraph *graph = object;

  rut_graphable_destroy (graph);

  g_slice_free (RutGraph, object);
}

RutType rut_graph_type;

static void
_rut_graph_init_type (void)
{
  static RutRefCountableVTable refable_vtable = {
      rut_refable_simple_ref,
      rut_refable_simple_unref,
      _rut_graph_free
  };

  static RutGraphableVTable graphable_vtable = {
      NULL, /* child remove */
      NULL, /* child add */
      NULL /* parent changed */
  };

  RutType *type = &rut_graph_type;
#define TYPE RutGraph

  rut_type_init (type, G_STRINGIFY (TYPE));
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (TYPE, ref_count),
                          &refable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (TYPE, graphable),
                          &graphable_vtable);

#undef TYPE
}

RutGraph *
rut_graph_new (RutContext *ctx)
{
  RutGraph *graph = g_slice_new (RutGraph);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rut_graph_init_type ();
      initialized = TRUE;
    }

  rut_object_init (&graph->_parent, &rut_graph_type);

  graph->ref_count = 1;

  rut_graphable_init (graph);

  return graph;
}
