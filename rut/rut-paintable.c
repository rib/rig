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

#include <rut-config.h>

#include "rut-paintable.h"
#include "rut-camera.h"

void
rut_paintable_init(rut_object_t *object)
{
#if 0
    rut_paintable_props_t *props =
        rut_object_get_properties (object, RUT_TRAIT_ID_PAINTABLE);
#endif
}

void
rut_paintable_paint(rut_object_t *object, rut_paint_context_t *paint_ctx)
{
    rut_paintable_vtable_t *paintable =
        rut_object_get_vtable(object, RUT_TRAIT_ID_PAINTABLE);

    paintable->paint(object, paint_ctx);
}

void
rut_paint_context_queue_paint(rut_paint_context_t *paint_ctx,
                              rut_object_t *paintable)
{
    rut_queued_paint_t *queue_entry = c_slice_new(rut_queued_paint_t);
    cg_framebuffer_t *fb = rut_camera_get_framebuffer(paint_ctx->camera);

    /* Get the modelview matrix that the widget was painted with so that
     * we don't need to calculate it again the next time it is
     * painted */
    cg_framebuffer_get_modelview_matrix(fb, &queue_entry->modelview);

    queue_entry->paintable = paintable;

    c_list_insert(paint_ctx->paint_queue.prev, &queue_entry->list_node);
}

void
rut_paint_graph_with_layers(rut_object_t *root,
                            rut_traverse_callback_t before_children_cb,
                            rut_traverse_callback_t after_children_cb,
                            rut_paint_context_t *paint_ctx)
{
    cg_framebuffer_t *fb = rut_camera_get_framebuffer(paint_ctx->camera);

    /* The initial walk of the graph is in layer 0 */
    paint_ctx->layer_number = 0;

    c_list_init(&paint_ctx->paint_queue);

    rut_graphable_traverse(root,
                           RUT_TRAVERSE_DEPTH_FIRST,
                           before_children_cb,
                           after_children_cb,
                           paint_ctx);

    /* Now paint anything that was queued to paint in higher layers */
    while (!c_list_empty(&paint_ctx->paint_queue)) {
        c_list_t queue;
        rut_queued_paint_t *node, *tmp;

        paint_ctx->layer_number++;

        cg_framebuffer_push_matrix(fb);

        /* Steal the list so that the widgets can start another layer by
         * adding stuff again */
        c_list_init(&queue);
        c_list_insert_list(&queue, &paint_ctx->paint_queue);
        c_list_init(&paint_ctx->paint_queue);

        c_list_for_each_safe(node, tmp, &queue, list_node)
        {
            /* Restore the modelview matrix that was used for this
             * widget */
            cg_framebuffer_set_modelview_matrix(fb, &node->modelview);
            before_children_cb(node->paintable, 0 /* depth */, paint_ctx);
            after_children_cb(node->paintable, 0 /* depth */, paint_ctx);
            c_slice_free(rut_queued_paint_t, node);
        }

        cg_framebuffer_pop_matrix(fb);
    }
}
