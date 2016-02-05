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

#ifndef __CG_NODE_PRIVATE_H
#define __CG_NODE_PRIVATE_H

#include <clib.h>

#include "cg-object-private.h"

typedef struct _cg_node_t cg_node_t;

/* Pipelines and layers represent their state in a tree structure where
 * some of the state relating to a given pipeline or layer may actually
 * be owned by one if is ancestors in the tree. We have a common data
 * type to track the tree heirachy so we can share code... */
struct _cg_node_t {
    /* the parent in terms of class hierarchy, so anything inheriting
     * from cg_node_t also inherits from cg_object_t. */
    cg_object_t _parent;

    /* The parent pipeline/layer */
    cg_node_t *parent;

    /* The list entry here contains pointers to the node's siblings */
    c_list_t link;

    /* List of children */
    c_list_t children;
};

#define CG_NODE(X) ((cg_node_t *)(X))

void _cg_pipeline_node_init(cg_node_t *node);

typedef void (*cg_node_unparent_vfunc_t)(cg_node_t *node);

void _cg_pipeline_node_set_parent_real(cg_node_t *node,
                                       cg_node_t *parent,
                                       cg_node_unparent_vfunc_t unparent);

void _cg_pipeline_node_unparent_real(cg_node_t *node);

typedef bool (*cg_node_child_callback_t)(cg_node_t *child, void *user_data);

void _cg_pipeline_node_foreach_child(cg_node_t *node,
                                     cg_node_child_callback_t callback,
                                     void *user_data);

#endif /* __CG_NODE_PRIVATE_H */
