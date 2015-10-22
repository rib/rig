/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2009 Intel Corporation.
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
 *  Neil Roberts   <neil@linux.intel.com>
 */

#include <cglib-config.h>

#include <clib.h>

#include "cg-util.h"
#include "cg-rectangle-map.h"
#include "cg-debug.h"

/* Implements a data structure which keeps track of unused
   sub-rectangles within a larger rectangle using a binary tree
   structure. The algorithm for this is based on the description here:

   http://www.blackpawn.com/texts/lightmaps/default.html
 */

#if defined(CG_ENABLE_DEBUG) && defined(HAVE_CAIRO)

/* The cairo header is only used for debugging to generate an image of
   the atlas */
#include <cairo.h>

static void _cg_rectangle_map_dump_image(cg_rectangle_map_t *map);

#endif /* CG_ENABLE_DEBUG && HAVE_CAIRO */

typedef struct _cg_rectangle_map_node_t cg_rectangle_map_node_t;
typedef struct _cg_rectangle_map_stack_entry_t cg_rectangle_map_stack_entry_t;

typedef void (*cg_rectangle_map_internal_foreach_cb_t)(
    cg_rectangle_map_node_t *node, void *data);

typedef enum {
    CG_RECTANGLE_MAP_BRANCH,
    CG_RECTANGLE_MAP_FILLED_LEAF,
    CG_RECTANGLE_MAP_EMPTY_LEAF
} cg_rectangle_map_node_type_t;

struct _cg_rectangle_map_t {
    cg_rectangle_map_node_t *root;

    unsigned int n_rectangles;

    unsigned int space_remaining;

    c_destroy_func_t value_destroy_func;

    /* Stack used for walking the structure. This is only used during
       the lifetime of a single function call but it is kept here as an
       optimisation to avoid reallocating it every time it is needed */
    c_array_t *stack;
};

struct _cg_rectangle_map_node_t {
    cg_rectangle_map_node_type_t type;

    cg_rectangle_map_entry_t rectangle;

    unsigned int largest_gap;

    cg_rectangle_map_node_t *parent;

    union {
        /* Fields used when this is a branch */
        struct {
            cg_rectangle_map_node_t *left;
            cg_rectangle_map_node_t *right;
        } branch;

        /* Field used when this is a filled leaf */
        void *data;
    } d;
};

struct _cg_rectangle_map_stack_entry_t {
    /* The node to search */
    cg_rectangle_map_node_t *node;
    /* Index of next branch of this node to explore. Basically either 0
       to go left or 1 to go right */
    bool next_index;
};

static cg_rectangle_map_node_t *
_cg_rectangle_map_node_new(void)
{
    return c_slice_new(cg_rectangle_map_node_t);
}

static void
_cg_rectangle_map_node_free(cg_rectangle_map_node_t *node)
{
    c_slice_free(cg_rectangle_map_node_t, node);
}

cg_rectangle_map_t *
_cg_rectangle_map_new(unsigned int width,
                      unsigned int height,
                      c_destroy_func_t value_destroy_func)
{
    cg_rectangle_map_t *map = c_new(cg_rectangle_map_t, 1);
    cg_rectangle_map_node_t *root = _cg_rectangle_map_node_new();

    root->type = CG_RECTANGLE_MAP_EMPTY_LEAF;
    root->parent = NULL;
    root->rectangle.x = 0;
    root->rectangle.y = 0;
    root->rectangle.width = width;
    root->rectangle.height = height;
    root->largest_gap = width * height;

    map->root = root;
    map->n_rectangles = 0;
    map->value_destroy_func = value_destroy_func;
    map->space_remaining = width * height;

    map->stack =
        c_array_new(false, false, sizeof(cg_rectangle_map_stack_entry_t));

    return map;
}

static void
_cg_rectangle_map_stack_push(c_array_t *stack,
                             cg_rectangle_map_node_t *node,
                             bool next_index)
{
    cg_rectangle_map_stack_entry_t *new_entry;

    c_array_set_size(stack, stack->len + 1);

    new_entry =
        &c_array_index(stack, cg_rectangle_map_stack_entry_t, stack->len - 1);

    new_entry->node = node;
    new_entry->next_index = next_index;
}

static void
_cg_rectangle_map_stack_pop(c_array_t *stack)
{
    c_array_set_size(stack, stack->len - 1);
}

static cg_rectangle_map_stack_entry_t *
_cg_rectangle_map_stack_get_top(c_array_t *stack)
{
    return &c_array_index(
        stack, cg_rectangle_map_stack_entry_t, stack->len - 1);
}

static cg_rectangle_map_node_t *
_cg_rectangle_map_node_split_horizontally(cg_rectangle_map_node_t *node,
                                          unsigned int left_width)
{
    /* Splits the node horizontally (according to emacs' definition, not
       vim) by converting it to a branch and adding two new leaf
       nodes. The leftmost branch will have the width left_width and
       will be returned. If the node is already just the right size it
       won't do anything */

    cg_rectangle_map_node_t *left_node, *right_node;

    if (node->rectangle.width == left_width)
        return node;

    left_node = _cg_rectangle_map_node_new();
    left_node->type = CG_RECTANGLE_MAP_EMPTY_LEAF;
    left_node->parent = node;
    left_node->rectangle.x = node->rectangle.x;
    left_node->rectangle.y = node->rectangle.y;
    left_node->rectangle.width = left_width;
    left_node->rectangle.height = node->rectangle.height;
    left_node->largest_gap =
        (left_node->rectangle.width * left_node->rectangle.height);
    node->d.branch.left = left_node;

    right_node = _cg_rectangle_map_node_new();
    right_node->type = CG_RECTANGLE_MAP_EMPTY_LEAF;
    right_node->parent = node;
    right_node->rectangle.x = node->rectangle.x + left_width;
    right_node->rectangle.y = node->rectangle.y;
    right_node->rectangle.width = node->rectangle.width - left_width;
    right_node->rectangle.height = node->rectangle.height;
    right_node->largest_gap =
        (right_node->rectangle.width * right_node->rectangle.height);
    node->d.branch.right = right_node;

    node->type = CG_RECTANGLE_MAP_BRANCH;

    return left_node;
}

static cg_rectangle_map_node_t *
_cg_rectangle_map_node_split_vertically(cg_rectangle_map_node_t *node,
                                        unsigned int top_height)
{
    /* Splits the node vertically (according to emacs' definition, not
       vim) by converting it to a branch and adding two new leaf
       nodes. The topmost branch will have the height top_height and
       will be returned. If the node is already just the right size it
       won't do anything */

    cg_rectangle_map_node_t *top_node, *bottom_node;

    if (node->rectangle.height == top_height)
        return node;

    top_node = _cg_rectangle_map_node_new();
    top_node->type = CG_RECTANGLE_MAP_EMPTY_LEAF;
    top_node->parent = node;
    top_node->rectangle.x = node->rectangle.x;
    top_node->rectangle.y = node->rectangle.y;
    top_node->rectangle.width = node->rectangle.width;
    top_node->rectangle.height = top_height;
    top_node->largest_gap =
        (top_node->rectangle.width * top_node->rectangle.height);
    node->d.branch.left = top_node;

    bottom_node = _cg_rectangle_map_node_new();
    bottom_node->type = CG_RECTANGLE_MAP_EMPTY_LEAF;
    bottom_node->parent = node;
    bottom_node->rectangle.x = node->rectangle.x;
    bottom_node->rectangle.y = node->rectangle.y + top_height;
    bottom_node->rectangle.width = node->rectangle.width;
    bottom_node->rectangle.height = node->rectangle.height - top_height;
    bottom_node->largest_gap =
        (bottom_node->rectangle.width * bottom_node->rectangle.height);
    node->d.branch.right = bottom_node;

    node->type = CG_RECTANGLE_MAP_BRANCH;

    return top_node;
}

#ifdef CG_ENABLE_DEBUG

static unsigned int
_cg_rectangle_map_verify_recursive(cg_rectangle_map_node_t *node)
{
    /* This is just used for debugging the data structure. It
       recursively walks the tree to verify that the largest gap values
       all add up */

    switch (node->type) {
    case CG_RECTANGLE_MAP_BRANCH: {
        int sum = _cg_rectangle_map_verify_recursive(node->d.branch.left) +
                  _cg_rectangle_map_verify_recursive(node->d.branch.right);
        c_assert(node->largest_gap == MAX(node->d.branch.left->largest_gap,
                                          node->d.branch.right->largest_gap));
        return sum;
    }

    case CG_RECTANGLE_MAP_EMPTY_LEAF:
        c_assert(node->largest_gap ==
                 node->rectangle.width * node->rectangle.height);
        return 0;

    case CG_RECTANGLE_MAP_FILLED_LEAF:
        c_assert(node->largest_gap == 0);
        return 1;
    }

    return 0;
}

static unsigned int
_cg_rectangle_map_get_space_remaining_recursive(cg_rectangle_map_node_t *node)
{
    /* This is just used for debugging the data structure. It
       recursively walks the tree to verify that the remaining space
       value adds up */

    switch (node->type) {
    case CG_RECTANGLE_MAP_BRANCH: {
        cg_rectangle_map_node_t *l = node->d.branch.left;
        cg_rectangle_map_node_t *r = node->d.branch.right;

        return (_cg_rectangle_map_get_space_remaining_recursive(l) +
                _cg_rectangle_map_get_space_remaining_recursive(r));
    }

    case CG_RECTANGLE_MAP_EMPTY_LEAF:
        return node->rectangle.width * node->rectangle.height;

    case CG_RECTANGLE_MAP_FILLED_LEAF:
        return 0;
    }

    return 0;
}

static void
_cg_rectangle_map_verify(cg_rectangle_map_t *map)
{
    unsigned int actual_n_rectangles =
        _cg_rectangle_map_verify_recursive(map->root);
    unsigned int actual_space_remaining =
        _cg_rectangle_map_get_space_remaining_recursive(map->root);

    c_assert_cmpuint(actual_n_rectangles, ==, map->n_rectangles);
    c_assert_cmpuint(actual_space_remaining, ==, map->space_remaining);
}

#endif /* CG_ENABLE_DEBUG */

bool
_cg_rectangle_map_add(cg_rectangle_map_t *map,
                      unsigned int width,
                      unsigned int height,
                      void *data,
                      cg_rectangle_map_entry_t *rectangle)
{
    unsigned int rectangle_size = width * height;
    /* Stack of nodes to search in */
    c_array_t *stack = map->stack;
    cg_rectangle_map_node_t *found_node = NULL;

    /* Zero-sized rectangles break the algorithm for removing rectangles
       so we'll disallow them */
    c_return_val_if_fail(width > 0 && height > 0, false);

    /* Start with the root node */
    c_array_set_size(stack, 0);
    _cg_rectangle_map_stack_push(stack, map->root, false);

    /* Depth-first search for an empty node that is big enough */
    while (stack->len > 0) {
        cg_rectangle_map_stack_entry_t *stack_top;
        cg_rectangle_map_node_t *node;
        int next_index;

        /* Pop an entry off the stack */
        stack_top = _cg_rectangle_map_stack_get_top(stack);
        node = stack_top->node;
        next_index = stack_top->next_index;
        _cg_rectangle_map_stack_pop(stack);

        /* Regardless of the type of the node, there's no point
           descending any further if the new rectangle won't fit within
           it */
        if (node->rectangle.width >= width &&
            node->rectangle.height >= height &&
            node->largest_gap >= rectangle_size) {
            if (node->type == CG_RECTANGLE_MAP_EMPTY_LEAF) {
                /* We've found a node we can use */
                found_node = node;
                break;
            } else if (node->type == CG_RECTANGLE_MAP_BRANCH) {
                if (next_index)
                    /* Try the right branch */
                    _cg_rectangle_map_stack_push(
                        stack, node->d.branch.right, 0);
                else {
                    /* Make sure we remember to try the right branch once
                       we've finished descending the left branch */
                    _cg_rectangle_map_stack_push(stack, node, 1);
                    /* Try the left branch */
                    _cg_rectangle_map_stack_push(stack, node->d.branch.left, 0);
                }
            }
        }
    }

    if (found_node) {
        cg_rectangle_map_node_t *node;

        /* Split according to whichever axis will leave us with the
           largest space */
        if (found_node->rectangle.width - width >
            found_node->rectangle.height - height) {
            found_node =
                _cg_rectangle_map_node_split_horizontally(found_node, width);
            found_node =
                _cg_rectangle_map_node_split_vertically(found_node, height);
        } else {
            found_node =
                _cg_rectangle_map_node_split_vertically(found_node, height);
            found_node =
                _cg_rectangle_map_node_split_horizontally(found_node, width);
        }

        found_node->type = CG_RECTANGLE_MAP_FILLED_LEAF;
        found_node->d.data = data;
        found_node->largest_gap = 0;
        if (rectangle)
            *rectangle = found_node->rectangle;

        /* Walk back up the tree and update the stored largest gap for
           the node's sub tree */
        for (node = found_node->parent; node; node = node->parent) {
            /* This node is a parent so it should always be a branch */
            c_assert(node->type == CG_RECTANGLE_MAP_BRANCH);

            node->largest_gap = MAX(node->d.branch.left->largest_gap,
                                    node->d.branch.right->largest_gap);
        }

        /* There is now an extra rectangle in the map */
        map->n_rectangles++;
        /* and less space */
        map->space_remaining -= rectangle_size;

#ifdef CG_ENABLE_DEBUG
        if (C_UNLIKELY(CG_DEBUG_ENABLED(CG_DEBUG_DUMP_ATLAS_IMAGE))) {
#ifdef HAVE_CAIRO
            _cg_rectangle_map_dump_image(map);
#endif
            /* Dumping the rectangle map is really slow so we might as well
               verify the space remaining here as it is also quite slow */
            _cg_rectangle_map_verify(map);
        }
#endif

        return true;
    } else
        return false;
}

void
_cg_rectangle_map_remove(cg_rectangle_map_t *map,
                         const cg_rectangle_map_entry_t *rectangle)
{
    cg_rectangle_map_node_t *node = map->root;
    unsigned int rectangle_size = rectangle->width * rectangle->height;

    /* We can do a binary-chop down the search tree to find the rectangle */
    while (node->type == CG_RECTANGLE_MAP_BRANCH) {
        cg_rectangle_map_node_t *left_node = node->d.branch.left;

        /* If and only if the rectangle is in the left node then the x,y
           position of the rectangle will be within the node's
           rectangle */
        if (rectangle->x <
            left_node->rectangle.x + left_node->rectangle.width &&
            rectangle->y < left_node->rectangle.y + left_node->rectangle.height)
            /* Go left */
            node = left_node;
        else
            /* Go right */
            node = node->d.branch.right;
    }

    /* Make sure we found the right node */
    if (node->type != CG_RECTANGLE_MAP_FILLED_LEAF ||
        node->rectangle.x != rectangle->x ||
        node->rectangle.y != rectangle->y ||
        node->rectangle.width != rectangle->width ||
        node->rectangle.height != rectangle->height)
        /* This should only happen if someone tried to remove a rectangle
           that was not in the map so something has gone wrong */
        c_return_if_reached();
    else {
        /* Convert the node back to an empty node */
        if (map->value_destroy_func)
            map->value_destroy_func(node->d.data);
        node->type = CG_RECTANGLE_MAP_EMPTY_LEAF;
        node->largest_gap = rectangle_size;

        /* Walk back up the tree combining branch nodes that have two
           empty leaves back into a single empty leaf */
        for (node = node->parent; node; node = node->parent) {
            /* This node is a parent so it should always be a branch */
            c_assert(node->type == CG_RECTANGLE_MAP_BRANCH);

            if (node->d.branch.left->type == CG_RECTANGLE_MAP_EMPTY_LEAF &&
                node->d.branch.right->type == CG_RECTANGLE_MAP_EMPTY_LEAF) {
                _cg_rectangle_map_node_free(node->d.branch.left);
                _cg_rectangle_map_node_free(node->d.branch.right);
                node->type = CG_RECTANGLE_MAP_EMPTY_LEAF;

                node->largest_gap =
                    (node->rectangle.width * node->rectangle.height);
            } else
                break;
        }

        /* Reduce the amount of space remaining in all of the parents
           further up the chain */
        for (; node; node = node->parent)
            node->largest_gap = MAX(node->d.branch.left->largest_gap,
                                    node->d.branch.right->largest_gap);

        /* There is now one less rectangle */
        c_assert(map->n_rectangles > 0);
        map->n_rectangles--;
        /* and more space */
        map->space_remaining += rectangle_size;
    }

#ifdef CG_ENABLE_DEBUG
    if (C_UNLIKELY(CG_DEBUG_ENABLED(CG_DEBUG_DUMP_ATLAS_IMAGE))) {
#ifdef HAVE_CAIRO
        _cg_rectangle_map_dump_image(map);
#endif
        /* Dumping the rectangle map is really slow so we might as well
           verify the space remaining here as it is also quite slow */
        _cg_rectangle_map_verify(map);
    }
#endif
}

unsigned int
_cg_rectangle_map_get_width(cg_rectangle_map_t *map)
{
    return map->root->rectangle.width;
}

unsigned int
_cg_rectangle_map_get_height(cg_rectangle_map_t *map)
{
    return map->root->rectangle.height;
}

unsigned int
_cg_rectangle_map_get_remaining_space(cg_rectangle_map_t *map)
{
    return map->space_remaining;
}

unsigned int
_cg_rectangle_map_get_n_rectangles(cg_rectangle_map_t *map)
{
    return map->n_rectangles;
}

static void
_cg_rectangle_map_internal_foreach(cg_rectangle_map_t *map,
                                   cg_rectangle_map_internal_foreach_cb_t func,
                                   void *data)
{
    /* Stack of nodes to search in */
    c_array_t *stack = map->stack;

    /* Start with the root node */
    c_array_set_size(stack, 0);
    _cg_rectangle_map_stack_push(stack, map->root, 0);

    /* Iterate all nodes depth-first */
    while (stack->len > 0) {
        cg_rectangle_map_stack_entry_t *stack_top =
            _cg_rectangle_map_stack_get_top(stack);
        cg_rectangle_map_node_t *node = stack_top->node;

        switch (node->type) {
        case CG_RECTANGLE_MAP_BRANCH:
            if (stack_top->next_index == 0) {
                /* Next time we come back to this node, go to the right */
                stack_top->next_index = 1;

                /* Explore the left branch next */
                _cg_rectangle_map_stack_push(stack, node->d.branch.left, 0);
            } else if (stack_top->next_index == 1) {
                /* Next time we come back to this node, stop processing it */
                stack_top->next_index = 2;

                /* Explore the right branch next */
                _cg_rectangle_map_stack_push(stack, node->d.branch.right, 0);
            } else {
                /* We're finished with this node so we can call the callback */
                func(node, data);
                _cg_rectangle_map_stack_pop(stack);
            }
            break;

        default:
            /* Some sort of leaf node, just call the callback */
            func(node, data);
            _cg_rectangle_map_stack_pop(stack);
            break;
        }
    }

    /* The stack should now be empty */
    c_assert(stack->len == 0);
}

typedef struct _cg_rectangle_map_foreach_closure_t {
    cg_rectangle_map_callback_t callback;
    void *data;
} cg_rectangle_map_foreach_closure_t;

static void
_cg_rectangle_map_foreach_cb(cg_rectangle_map_node_t *node,
                             void *data)
{
    cg_rectangle_map_foreach_closure_t *closure = data;

    if (node->type == CG_RECTANGLE_MAP_FILLED_LEAF)
        closure->callback(&node->rectangle, node->d.data, closure->data);
}

void
_cg_rectangle_map_foreach(cg_rectangle_map_t *map,
                          cg_rectangle_map_callback_t callback,
                          void *data)
{
    cg_rectangle_map_foreach_closure_t closure;

    closure.callback = callback;
    closure.data = data;

    _cg_rectangle_map_internal_foreach(
        map, _cg_rectangle_map_foreach_cb, &closure);
}

static void
_cg_rectangle_map_free_cb(cg_rectangle_map_node_t *node, void *data)
{
    cg_rectangle_map_t *map = data;

    if (node->type == CG_RECTANGLE_MAP_FILLED_LEAF && map->value_destroy_func)
        map->value_destroy_func(node->d.data);

    _cg_rectangle_map_node_free(node);
}

void
_cg_rectangle_map_free(cg_rectangle_map_t *map)
{
    _cg_rectangle_map_internal_foreach(map, _cg_rectangle_map_free_cb, map);

    c_array_free(map->stack, true);

    c_free(map);
}

#if defined(CG_ENABLE_DEBUG) && defined(HAVE_CAIRO)

static void
_cg_rectangle_map_dump_image_cb(cg_rectangle_map_node_t *node,
                                void *data)
{
    cairo_t *cr = data;

    if (node->type == CG_RECTANGLE_MAP_FILLED_LEAF ||
        node->type == CG_RECTANGLE_MAP_EMPTY_LEAF) {
        /* Fill the rectangle using a different colour depending on
           whether the rectangle is used */
        if (node->type == CG_RECTANGLE_MAP_FILLED_LEAF)
            cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
        else
            cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);

        cairo_rectangle(cr,
                        node->rectangle.x,
                        node->rectangle.y,
                        node->rectangle.width,
                        node->rectangle.height);

        cairo_fill_preserve(cr);

        /* Draw a white outline around the rectangle */
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_stroke(cr);
    }
}

static void
_cg_rectangle_map_dump_image(cg_rectangle_map_t *map)
{
    /* This dumps a png to help visualize the map. Each leaf rectangle
       is drawn with a white outline. Unused leaves are filled in black
       and used leaves are blue */

    cairo_surface_t *surface =
        cairo_image_surface_create(CAIRO_FORMAT_RGB24,
                                   _cg_rectangle_map_get_width(map),
                                   _cg_rectangle_map_get_height(map));
    cairo_t *cr = cairo_create(surface);

    _cg_rectangle_map_internal_foreach(
        map, _cg_rectangle_map_dump_image_cb, cr);

    cairo_destroy(cr);

    cairo_surface_write_to_png(surface, "cg-rectangle-map-dump.png");

    cairo_surface_destroy(surface);
}

#endif /* CG_ENABLE_DEBUG && HAVE_CAIRO */
