/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012,2013  Intel Corporation
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
 */

#pragma once

#include "rut-type.h"
#include "rut-object.h"
#include "rut-types.h"
#include "rut-queue.h"

typedef struct _rut_graphable_vtable_t {
    void (*child_removed)(rut_object_t *parent, rut_object_t *child);
    void (*child_added)(rut_object_t *parent, rut_object_t *child);

    void (*parent_changed)(rut_object_t *child,
                           rut_object_t *old_parent,
                           rut_object_t *new_parent);
} rut_graphable_vtable_t;

typedef struct _rut_graphable_props_t {
    rut_object_t *parent;
    rut_queue_t children;
} rut_graphable_props_t;

#if 0
rut_object_t *
rut_graphable_find_camera (rut_object_t *object);
#endif

/* rut_traverse_flags_t:
 * RUT_TRAVERSE_DEPTH_FIRST: Traverse the graph in
 *   a depth first order.
 * RUT_TRAVERSE_BREADTH_FIRST: Traverse the graph in a
 *   breadth first order.
 *
 * Controls some options for how rut_graphable_traverse() iterates
 * through a graph.
 */
typedef enum {
    RUT_TRAVERSE_DEPTH_FIRST = 1L << 0,
    RUT_TRAVERSE_BREADTH_FIRST = 1L << 1
} rut_traverse_flags_t;

/* rut_traverse_visit_flags_t:
 * RUT_TRAVERSE_VISIT_CONTINUE: Continue traversing as
 *   normal
 * RUT_TRAVERSE_VISIT_SKIP_CHILDREN: Don't traverse the
 *   children of the last visited object. (Not applicable when using
 *   %RUT_TRAVERSE_DEPTH_FIRST_POST_ORDER since the children
 *   are visited before having an opportunity to bail out)
 * RUT_TRAVERSE_VISIT_BREAK: Immediately bail out without
 *   visiting any more objects.
 *
 * Each time an object is visited during a graph traversal the
 * rut_traverse_callback_t can return a set of flags that may affect the
 * continuing traversal. It may stop traversal completely, just skip
 * over children for the current object or continue as normal.
 */
typedef enum {
    RUT_TRAVERSE_VISIT_CONTINUE = 1L << 0,
    RUT_TRAVERSE_VISIT_SKIP_CHILDREN = 1L << 1,
    RUT_TRAVERSE_VISIT_BREAK = 1L << 2
} rut_traverse_visit_flags_t;

/* The callback prototype used with rut_graphable_traverse. The
 * returned flags can be used to affect the continuing traversal
 * either by continuing as normal, skipping over children of an
 * object or bailing out completely.
 */
typedef rut_traverse_visit_flags_t (*rut_traverse_callback_t)(
    rut_object_t *object, int depth, void *user_data);

rut_traverse_visit_flags_t
rut_graphable_traverse(rut_object_t *root,
                       rut_traverse_flags_t flags,
                       rut_traverse_callback_t before_children_callback,
                       rut_traverse_callback_t after_children_callback,
                       void *user_data);

void rut_graphable_init(rut_object_t *object);

void rut_graphable_destroy(rut_object_t *object);

void rut_graphable_add_child(rut_object_t *parent, rut_object_t *child);

void rut_graphable_remove_child(rut_object_t *child);

void rut_graphable_remove_all_children(rut_object_t *parent);

rut_object_t *rut_graphable_get_parent(rut_object_t *child);
void rut_graphable_set_parent(rut_object_t *self, rut_object_t *parent);

rut_object_t *rut_graphable_first(rut_object_t *parent);

rut_object_t *rut_graphable_nth(rut_object_t *parent, int n);

rut_object_t *rut_graphable_get_root(rut_object_t *child);

void rut_graphable_apply_transform(rut_object_t *graphable,
                                   c_matrix_t *transform);

void rut_graphable_get_transform(rut_object_t *graphable,
                                 c_matrix_t *transform);

void rut_graphable_get_modelview(rut_object_t *graphable,
                                 rut_object_t *camera,
                                 c_matrix_t *transform);

void rut_graphable_fully_transform_point(rut_object_t *graphable,
                                         rut_object_t *camera,
                                         float *x,
                                         float *y,
                                         float *z);
