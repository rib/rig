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

#include <rut-config.h>

#include <cglib/cglib.h>

#include "rut-graphable.h"
#include "rut-interfaces.h"
#include "rut-util.h"
#include "rut-camera.h"
#include "rut-queue.h"

void
rut_graphable_init(rut_object_t *object)
{
    rut_graphable_props_t *props =
        rut_object_get_properties(object, RUT_TRAIT_ID_GRAPHABLE);

    props->parent = NULL;
    rut_queue_init(&props->children);
}

void
rut_graphable_destroy(rut_object_t *object)
{
    rut_graphable_props_t *props =
        rut_object_get_properties(object, RUT_TRAIT_ID_GRAPHABLE);

    /* The node shouldn't have a parent, because if it did then it would
     * still have a reference and it shouldn't be being destroyed */
    c_warn_if_fail(props->parent == NULL);

    rut_graphable_remove_all_children(object);
}

void
rut_graphable_add_child(rut_object_t *parent, rut_object_t *child)
{
    rut_graphable_props_t *parent_props =
        rut_object_get_properties(parent, RUT_TRAIT_ID_GRAPHABLE);
    rut_graphable_vtable_t *parent_vtable =
        rut_object_get_vtable(parent, RUT_TRAIT_ID_GRAPHABLE);
    rut_graphable_props_t *child_props =
        rut_object_get_properties(child, RUT_TRAIT_ID_GRAPHABLE);
    rut_graphable_vtable_t *child_vtable =
        rut_object_get_vtable(child, RUT_TRAIT_ID_GRAPHABLE);
    rut_object_t *old_parent = child_props->parent;

    rut_object_claim(child, parent);

    if (old_parent)
        rut_graphable_remove_child(child);

    child_props->parent = parent;
    if (child_vtable && child_vtable->parent_changed)
        child_vtable->parent_changed(child, old_parent, parent);

    if (parent_vtable && parent_vtable->child_added)
        parent_vtable->child_added(parent, child);

    /* XXX: maybe this should be deferred to parent_vtable->child_added ? */
    rut_queue_push_tail(&parent_props->children, child);
}

void
rut_graphable_remove_child(rut_object_t *child)
{
    rut_graphable_props_t *child_props =
        rut_object_get_properties(child, RUT_TRAIT_ID_GRAPHABLE);
    rut_object_t *parent = child_props->parent;
    rut_graphable_vtable_t *parent_vtable;
    rut_graphable_props_t *parent_props;

    if (!parent)
        return;

    parent_vtable = rut_object_get_vtable(parent, RUT_TRAIT_ID_GRAPHABLE);
    parent_props = rut_object_get_properties(parent, RUT_TRAIT_ID_GRAPHABLE);

    /* Note: we set ->parent to NULL here to avoid re-entrancy so
     * ->child_removed can be a general function for removing a child
     *  that might itself call rut_graphable_remove_child() */
    child_props->parent = NULL;

    if (parent_vtable->child_removed)
        parent_vtable->child_removed(parent, child);

    rut_queue_remove(&parent_props->children, child);
    rut_object_release(child, parent);
}

void
rut_graphable_remove_all_children(rut_object_t *parent)
{
    rut_graphable_props_t *parent_props =
        rut_object_get_properties(parent, RUT_TRAIT_ID_GRAPHABLE);
    rut_object_t *child;

    while ((child = rut_queue_pop_tail(&parent_props->children)))
        rut_graphable_remove_child(child);
}

static rut_object_t *
_rut_graphable_get_parent(rut_object_t *child)
{
    rut_graphable_props_t *child_props =
        rut_object_get_properties(child, RUT_TRAIT_ID_GRAPHABLE);

    return child_props->parent;
}

rut_object_t *
rut_graphable_get_parent(rut_object_t *child)
{
    return _rut_graphable_get_parent(child);
}

void
rut_graphable_set_parent(rut_object_t *self, rut_object_t *parent)
{
    if (parent)
        rut_graphable_add_child(parent, self);
    else
        rut_graphable_remove_child(self);
}

rut_object_t *
rut_graphable_first(rut_object_t *parent)
{
    rut_graphable_props_t *graphable =
        rut_object_get_properties(parent, RUT_TRAIT_ID_GRAPHABLE);

    return rut_queue_peek_head(&graphable->children);
}

rut_object_t *
rut_graphable_nth(rut_object_t *parent, int n)
{
    rut_graphable_props_t *graphable =
        rut_object_get_properties(parent, RUT_TRAIT_ID_GRAPHABLE);

    return rut_queue_peek_nth(&graphable->children, n);
}

rut_object_t *
rut_graphable_get_root(rut_object_t *child)
{
    rut_object_t *root, *parent;

    root = child;
    while ((parent = _rut_graphable_get_parent(root)))
        root = parent;

    return root;
}

static rut_traverse_visit_flags_t
_rut_graphable_traverse_breadth(
    rut_object_t *graphable, rut_traverse_callback_t callback, void *user_data)
{
    rut_queue_t queue;
    int dummy;
    int current_depth = 0;
    rut_traverse_visit_flags_t flags = 0;

    rut_queue_init(&queue);

    rut_queue_push_tail(&queue, graphable);
    rut_queue_push_tail(&queue, &dummy); /* use to delimit depth changes */

    while ((graphable = rut_queue_pop_head(&queue))) {
        if (graphable == &dummy) {
            current_depth++;
            rut_queue_push_tail(&queue, &dummy);
            continue;
        }

        flags = callback(graphable, current_depth, user_data);
        if (flags & RUT_TRAVERSE_VISIT_BREAK)
            break;
        else if (!(flags & RUT_TRAVERSE_VISIT_SKIP_CHILDREN)) {
            rut_graphable_props_t *props =
                rut_object_get_properties(graphable, RUT_TRAIT_ID_GRAPHABLE);
            rut_queue_item_t *item;

            c_list_for_each(item, &props->children.items, list_node)
            {
                rut_queue_push_tail(&queue, item->data);
            }
        }
    }

    return flags;
}

static rut_traverse_visit_flags_t
_rut_graphable_traverse_depth(rut_object_t *graphable,
                              rut_traverse_callback_t before_children_callback,
                              rut_traverse_callback_t after_children_callback,
                              int current_depth,
                              void *user_data)
{
    rut_traverse_visit_flags_t flags = 0;

    if (before_children_callback) {
        flags = before_children_callback(graphable, current_depth, user_data);
        if (flags & RUT_TRAVERSE_VISIT_BREAK)
            return RUT_TRAVERSE_VISIT_BREAK;
    }

    if (!(flags & RUT_TRAVERSE_VISIT_SKIP_CHILDREN)) {
        rut_graphable_props_t *props =
            rut_object_get_properties(graphable, RUT_TRAIT_ID_GRAPHABLE);
        rut_queue_item_t *item, *tmp;

        c_list_for_each_safe(item, tmp, &props->children.items, list_node)
        {
            flags = _rut_graphable_traverse_depth(item->data,
                                                  before_children_callback,
                                                  after_children_callback,
                                                  current_depth + 1,
                                                  user_data);

            if (flags & RUT_TRAVERSE_VISIT_BREAK)
                return RUT_TRAVERSE_VISIT_BREAK;
        }
    }

    if (after_children_callback)
        return after_children_callback(graphable, current_depth, user_data);
    else
        return RUT_TRAVERSE_VISIT_CONTINUE;
}

/* rut_graphable_traverse:
 * @graphable: The graphable object to start traversing the graph from
 * @flags: These flags may affect how the traversal is done
 * @before_children_callback: A function to call before visiting the
 *   children of the current object.
 * @after_children_callback: A function to call after visiting the
 *   children of the current object. (Ignored if
 *   %RUT_TRAVERSE_BREADTH_FIRST is passed to @flags.)
 * @user_data: The private data to pass to the callbacks
 *
 * Traverses the graph starting at the specified @root and descending
 * through all its children and its children's children.  For each
 * object traversed @before_children_callback and
 * @after_children_callback are called with the specified @user_data,
 * before and after visiting that object's children.
 *
 * The callbacks can return flags that affect the ongoing traversal
 * such as by skipping over an object's children or bailing out of
 * any further traversing.
 */
rut_traverse_visit_flags_t
rut_graphable_traverse(rut_object_t *root,
                       rut_traverse_flags_t flags,
                       rut_traverse_callback_t before_children_callback,
                       rut_traverse_callback_t after_children_callback,
                       void *user_data)
{
    if (flags & RUT_TRAVERSE_BREADTH_FIRST)
        return _rut_graphable_traverse_breadth(
            root, before_children_callback, user_data);
    else /* DEPTH_FIRST */
        return _rut_graphable_traverse_depth(root,
                                             before_children_callback,
                                             after_children_callback,
                                             0, /* start depth */
                                             user_data);
}

#if 0
static rut_traverse_visit_flags_t
_rut_graphable_paint_cb (rut_object_t *object,
                         int depth,
                         void *user_data)
{
    rut_paint_context_t *paint_ctx = user_data;
    rut_paintable_vtable_t *vtable =
        rut_object_get_vtable (object, RUT_TRAIT_ID_PAINTABLE);

    vtable->paint (object, paint_ctx);

    return RUT_TRAVERSE_VISIT_CONTINUE;
}

void
rut_graphable_paint (rut_object_t *root,
                     rut_object_t *camera)
{
    rut_paint_context_t paint_ctx;

    paint_ctx.camera = camera;

    rut_graphable_traverse (root,
                            RUT_TRAVERSE_DEPTH_FIRST,
                            _rut_graphable_paint_cb,
                            NULL, /* after callback */
                            &paint_ctx);
}
#endif

#if 0
rut_object_t *
rut_graphable_find_camera (rut_object_t *object)
{
    do {
        rut_graphable_props_t *graphable_priv;

        if (rut_object_is (object, RUT_TRAIT_ID_CAMERA))
            return object;

        graphable_priv =
            rut_object_get_properties (object, RUT_TRAIT_ID_GRAPHABLE);

        object = graphable_priv->parent;

    } while (object);

    return NULL;
}
#endif

void
rut_graphable_apply_transform(rut_object_t *graphable,
                              c_matrix_t *transform_matrix)
{
    int depth = 0;
    rut_object_t **transform_nodes;
    rut_object_t *node = graphable;
    int i;

    do {
        rut_graphable_props_t *graphable_priv =
            rut_object_get_properties(node, RUT_TRAIT_ID_GRAPHABLE);

        depth++;

        node = graphable_priv->parent;
    } while (node);

    transform_nodes = c_alloca(sizeof(rut_object_t *) * depth);

    node = graphable;
    i = 0;
    do {
        rut_graphable_props_t *graphable_priv;

        if (rut_object_is(node, RUT_TRAIT_ID_TRANSFORMABLE))
            transform_nodes[i++] = node;

        graphable_priv =
            rut_object_get_properties(node, RUT_TRAIT_ID_GRAPHABLE);
        node = graphable_priv->parent;
    } while (node);

    for (i--; i >= 0; i--) {
        const c_matrix_t *matrix =
            rut_transformable_get_matrix(transform_nodes[i]);
        c_matrix_multiply(transform_matrix, transform_matrix, matrix);
    }
}

void
rut_graphable_get_transform(rut_object_t *graphable,
                            c_matrix_t *transform)
{
    c_matrix_init_identity(transform);
    rut_graphable_apply_transform(graphable, transform);
}

void
rut_graphable_get_modelview(rut_object_t *graphable,
                            rut_object_t *camera,
                            c_matrix_t *transform)
{
    const c_matrix_t *view = rut_camera_get_view_transform(camera);
    *transform = *view;
    rut_graphable_apply_transform(graphable, transform);
}

void
rut_graphable_fully_transform_point(
    rut_object_t *graphable, rut_object_t *camera, float *x, float *y, float *z)
{
    c_matrix_t modelview;
    const c_matrix_t *projection;
    const float *viewport;
    float point[3] = { *x, *y, *z };

    rut_graphable_get_modelview(graphable, camera, &modelview);
    projection = rut_camera_get_projection(camera);
    viewport = rut_camera_get_viewport(camera);

    rut_util_fully_transform_vertices(
        &modelview, projection, viewport, point, point, 1);

    *x = point[0];
    *y = point[1];
    *z = point[2];
}
