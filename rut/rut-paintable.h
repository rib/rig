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

#ifndef _RUT_PAINTABLE_H_
#define _RUT_PAINTABLE_H_

#include "rut-object.h"
#include "rut-types.h"
#include "rut-list.h"
#include "rut-interfaces.h"

typedef struct _RutPaintableProps
{
  int padding;
} RutPaintableProps;

typedef struct _RutQueuedPaint
{
  RutList list_node;
  CoglMatrix modelview;
  RutObject *paintable;
} RutQueuedPaint;

typedef struct _RutPaintContext
{
  RutObject *camera;

  /* The next two members are used to implement a layer mechanism so
   * that widgets can draw something above all other widgets without
   * having to add a separate node to the graph. During the initial
   * walk of the tree for painting the layer_number is zero and the
   * paint method on all of the paintable objects in the graph are
   * called. If a widget wants to paint in another layer it can add
   * itself to the queue using rut_paint_context_queue_paint(). Once
   * the initial walk of the graph is complete the layer_number will
   * be incremented and everything in the list will be painted again.
   * This will be repeated until the list becomes empty. */
  int layer_number;
  RutList paint_queue;
} RutPaintContext;

#define RUT_PAINT_CONTEXT(X) ((RutPaintContext *)X)

typedef struct _RutPaintableVtable
{
  void (*paint) (RutObject *object, RutPaintContext *paint_ctx);
} RutPaintableVTable;

void
rut_paintable_init (RutObject *object);

void
rut_paintable_paint (RutObject *object, RutPaintContext *paint_ctx);

void
rut_paint_context_queue_paint (RutPaintContext *paint_ctx,
                               RutObject *paintable);

void
rut_paint_graph_with_layers (RutObject *root,
                             RutTraverseCallback before_children_cb,
                             RutTraverseCallback after_children_cb,
                             RutPaintContext *paint_ctx);

#endif /* _RUT_PAINTABLE_H_ */
