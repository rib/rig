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

#ifndef _RIG_PAINTABLE_H_
#define _RIG_PAINTABLE_H_

#include "rig-object.h"
#include "rig-types.h"
#include "rig-list.h"
#include "rig-interfaces.h"

typedef struct _RigPaintableProps
{
  int padding;
} RigPaintableProps;

typedef struct _RigQueuedPaint
{
  RigList list_node;
  CoglMatrix modelview;
  RigObject *paintable;
} RigQueuedPaint;

typedef struct _RigPaintContext
{
  RigCamera *camera;

  /* The next two members are used to implement a layer mechanism so
   * that widgets can draw something above all other widgets without
   * having to add a separate node to the graph. During the initial
   * walk of the tree for painting the layer_number is zero and the
   * paint method on all of the paintable objects in the graph are
   * called. If a widget wants to paint in another layer it can add
   * itself to the queue using rig_paint_context_queue_paint(). Once
   * the initial walk of the graph is complete the layer_number will
   * be incremented and everything in the list will be painted again.
   * This will be repeated until the list becomes empty. */
  int layer_number;
  RigList paint_queue;
} RigPaintContext;

#define RIG_PAINT_CONTEXT(X) ((RigPaintContext *)X)

typedef struct _RigPaintableVtable
{
  void (*paint) (RigObject *object, RigPaintContext *paint_ctx);
} RigPaintableVTable;

void
rig_paintable_init (RigObject *object);

void
rig_paintable_paint (RigObject *object, RigPaintContext *paint_ctx);

void
rig_paint_context_queue_paint (RigPaintContext *paint_ctx,
                               RigObject *paintable);

void
rig_paint_graph_with_layers (RigObject *root,
                             RigTraverseCallback before_children_cb,
                             RigTraverseCallback after_children_cb,
                             RigPaintContext *paint_ctx);

#endif /* _RIG_PAINTABLE_H_ */
