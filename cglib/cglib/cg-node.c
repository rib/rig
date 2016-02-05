/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#include <cglib-config.h>

#include "cg-util.h"
#include "cg-node-private.h"

void
_cg_pipeline_node_init(cg_node_t *node)
{
    node->parent = NULL;
    c_list_init(&node->children);
}

void
_cg_pipeline_node_set_parent_real(cg_node_t *node,
                                  cg_node_t *parent,
                                  cg_node_unparent_vfunc_t unparent)
{
    /* NB: the old parent may indirectly be keeping the new parent
     * alive so we have to ref the new parent before unrefing the old.
     */
    cg_object_ref(parent);

    if (node->parent)
        unparent(node);

    c_list_insert(&parent->children, &node->link);

    node->parent = parent;
}

void
_cg_pipeline_node_unparent_real(cg_node_t *node)
{
    cg_node_t *parent = node->parent;

    if (parent == NULL)
        return;

    c_return_if_fail(!c_list_empty(&parent->children));

    c_list_remove(&node->link);

    cg_object_unref(parent);

    node->parent = NULL;
}

void
_cg_pipeline_node_foreach_child(cg_node_t *node,
                                cg_node_child_callback_t callback,
                                void *user_data)
{
    cg_node_t *child, *next;

    c_list_for_each_safe(child, next, &node->children, link)
    callback(child, user_data);
}
