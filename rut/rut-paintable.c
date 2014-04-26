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

#include <config.h>

#include "rut-paintable.h"
#include "rut-camera.h"

void
rut_paintable_init (RutObject *object)
{
#if 0
  RutPaintableProps *props =
    rut_object_get_properties (object, RUT_TRAIT_ID_PAINTABLE);
#endif
}

void
rut_paintable_paint (RutObject *object, RutPaintContext *paint_ctx)
{
  RutPaintableVTable *paintable =
    rut_object_get_vtable (object, RUT_TRAIT_ID_PAINTABLE);

  paintable->paint (object, paint_ctx);
}

void
rut_paint_context_queue_paint (RutPaintContext *paint_ctx,
                               RutObject *paintable)
{
  RutQueuedPaint *queue_entry = c_slice_new (RutQueuedPaint);
  CoglFramebuffer *fb = rut_camera_get_framebuffer (paint_ctx->camera);

  /* Get the modelview matrix that the widget was painted with so that
   * we don't need to calculate it again the next time it is
   * painted */
  cogl_framebuffer_get_modelview_matrix (fb, &queue_entry->modelview);

  queue_entry->paintable = paintable;

  rut_list_insert (paint_ctx->paint_queue.prev, &queue_entry->list_node);
}

void
rut_paint_graph_with_layers (RutObject *root,
                             RutTraverseCallback before_children_cb,
                             RutTraverseCallback after_children_cb,
                             RutPaintContext *paint_ctx)
{
  CoglFramebuffer *fb = rut_camera_get_framebuffer (paint_ctx->camera);

  /* The initial walk of the graph is in layer 0 */
  paint_ctx->layer_number = 0;

  rut_list_init (&paint_ctx->paint_queue);

  rut_graphable_traverse (root,
                          RUT_TRAVERSE_DEPTH_FIRST,
                          before_children_cb,
                          after_children_cb,
                          paint_ctx);

  /* Now paint anything that was queued to paint in higher layers */
  while (!rut_list_empty (&paint_ctx->paint_queue))
    {
      RutList queue;
      RutQueuedPaint *node, *tmp;

      paint_ctx->layer_number++;

      cogl_framebuffer_push_matrix (fb);

      /* Steal the list so that the widgets can start another layer by
       * adding stuff again */
      rut_list_init (&queue);
      rut_list_insert_list (&queue, &paint_ctx->paint_queue);
      rut_list_init (&paint_ctx->paint_queue);

      rut_list_for_each_safe (node, tmp, &queue, list_node)
        {
          /* Restore the modelview matrix that was used for this
           * widget */
          cogl_framebuffer_set_modelview_matrix (fb, &node->modelview);
          before_children_cb (node->paintable, 0 /* depth */, paint_ctx);
          after_children_cb (node->paintable, 0 /* depth */, paint_ctx);
          c_slice_free (RutQueuedPaint, node);
        }

      cogl_framebuffer_pop_matrix (fb);
    }
}
