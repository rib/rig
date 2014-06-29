/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
  cg_matrix_t modelview;
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
