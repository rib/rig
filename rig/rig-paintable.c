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

#include "rig-paintable.h"
#include "components/rig-camera.h"

void
rig_paintable_init (RigObject *object)
{
#if 0
  RigPaintableProps *props =
    rig_object_get_properties (object, RIG_INTERFACE_ID_PAINTABLE);
#endif
}

void
rig_paintable_paint (RigObject *object, RigPaintContext *paint_ctx)
{
  RigPaintableVTable *paintable =
    rig_object_get_vtable (object, RIG_INTERFACE_ID_PAINTABLE);

  paintable->paint (object, paint_ctx);
}

void
rig_paint_context_queue_paint (RigPaintContext *paint_ctx,
                               RigObject *paintable)
{
  RigQueuedPaint *queue_entry = g_slice_new (RigQueuedPaint);
  CoglFramebuffer *fb = rig_camera_get_framebuffer (paint_ctx->camera);

  /* Get the modelview matrix that the widget was painted with so that
   * we don't need to calculate it again the next time it is
   * painted */
  cogl_framebuffer_get_modelview_matrix (fb, &queue_entry->modelview);

  queue_entry->paintable = paintable;

  rig_list_insert (paint_ctx->paint_queue.prev, &queue_entry->list_node);
}

void
rig_paint_graph_with_layers (RigObject *root,
                             RigTraverseCallback before_children_cb,
                             RigTraverseCallback after_children_cb,
                             RigPaintContext *paint_ctx)
{
  CoglFramebuffer *fb = rig_camera_get_framebuffer (paint_ctx->camera);

  /* The initial walk of the graph is in layer 0 */
  paint_ctx->layer_number = 0;

  rig_list_init (&paint_ctx->paint_queue);

  rig_graphable_traverse (root,
                          RIG_TRAVERSE_DEPTH_FIRST,
                          before_children_cb,
                          after_children_cb,
                          paint_ctx);

  /* Now paint anything that was queued to paint in higher layers */
  while (!rig_list_empty (&paint_ctx->paint_queue))
    {
      RigList queue;
      RigQueuedPaint *node, *tmp;

      paint_ctx->layer_number++;

      cogl_framebuffer_push_matrix (fb);

      /* Steal the list so that the widgets can start another layer by
       * adding stuff again */
      rig_list_init (&queue);
      rig_list_insert_list (&queue, &paint_ctx->paint_queue);
      rig_list_init (&paint_ctx->paint_queue);

      rig_list_for_each_safe (node, tmp, &queue, list_node)
        {
          /* Restore the modelview matrix that was used for this
           * widget */
          cogl_framebuffer_set_modelview_matrix (fb, &node->modelview);
          before_children_cb (node->paintable, 0 /* depth */, paint_ctx);
          after_children_cb (node->paintable, 0 /* depth */, paint_ctx);
          g_slice_free (RigQueuedPaint, node);
        }

      cogl_framebuffer_pop_matrix (fb);
    }
}
