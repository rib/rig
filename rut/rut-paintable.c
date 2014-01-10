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

#include "rut-paintable.h"
#include "components/rut-camera.h"

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
  RutQueuedPaint *queue_entry = g_slice_new (RutQueuedPaint);
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
          g_slice_free (RutQueuedPaint, node);
        }

      cogl_framebuffer_pop_matrix (fb);
    }
}
