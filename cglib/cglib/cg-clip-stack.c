/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
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
 */

#include <cglib-config.h>

#include <string.h>
#include <math.h>

#include <clib.h>

#include "cg-clip-stack.h"
#include "cg-device-private.h"
#include "cg-framebuffer-private.h"
#include "cg-util.h"
#include "cg-private.h"
#include "cg-pipeline-opengl-private.h"
#include "cg-attribute-private.h"
#include "cg-primitive-private.h"
#include "cg-offscreen.h"
#include "cg-matrix-stack.h"

static void *
_cg_clip_stack_push_entry(cg_clip_stack_t *clip_stack,
                          size_t size,
                          cg_clip_stack_type_t type)
{
    cg_clip_stack_t *entry = c_slice_alloc(size);

    /* The new entry starts with a ref count of 1 because the stack
       holds a reference to it as it is the top entry */
    entry->ref_count = 1;
    entry->type = type;
    entry->parent = clip_stack;

    /* We don't need to take a reference to the parent from the entry
       because the we are stealing the ref in the new stack top */

    return entry;
}

static void
get_transformed_corners(float x_1,
                        float y_1,
                        float x_2,
                        float y_2,
                        c_matrix_t *modelview,
                        c_matrix_t *projection,
                        const float *viewport,
                        float *transformed_corners)
{
    int i;

    transformed_corners[0] = x_1;
    transformed_corners[1] = y_1;
    transformed_corners[2] = x_2;
    transformed_corners[3] = y_1;
    transformed_corners[4] = x_2;
    transformed_corners[5] = y_2;
    transformed_corners[6] = x_1;
    transformed_corners[7] = y_2;

    /* Project the coordinates to window space coordinates */
    for (i = 0; i < 4; i++) {
        float *v = transformed_corners + i * 2;
        _cg_transform_point(modelview, projection, viewport, v, v + 1);
    }
}

/* Sets the window-space bounds of the entry based on the projected
   coordinates of the given rectangle */
static void
_cg_clip_stack_entry_set_bounds(cg_clip_stack_t *entry,
                                float *transformed_corners)
{
    float min_x = FLT_MAX, min_y = FLT_MAX;
    float max_x = -FLT_MAX, max_y = -FLT_MAX;
    int i;

    for (i = 0; i < 4; i++) {
        float *v = transformed_corners + i * 2;

        if (v[0] > max_x)
            max_x = v[0];
        if (v[0] < min_x)
            min_x = v[0];
        if (v[1] > max_y)
            max_y = v[1];
        if (v[1] < min_y)
            min_y = v[1];
    }

    entry->bounds_x0 = floorf(min_x);
    entry->bounds_x1 = ceilf(max_x);
    entry->bounds_y0 = floorf(min_y);
    entry->bounds_y1 = ceilf(max_y);
}

cg_clip_stack_t *
_cg_clip_stack_push_window_rectangle(
    cg_clip_stack_t *stack, int x_offset, int y_offset, int width, int height)
{
    cg_clip_stack_t *entry;

    entry = _cg_clip_stack_push_entry(
        stack, sizeof(cg_clip_stack_window_rect_t), CG_CLIP_STACK_WINDOW_RECT);

    entry->bounds_x0 = x_offset;
    entry->bounds_x1 = x_offset + width;
    entry->bounds_y0 = y_offset;
    entry->bounds_y1 = y_offset + height;

    return entry;
}

cg_clip_stack_t *
_cg_clip_stack_push_rectangle(cg_clip_stack_t *stack,
                              float x_1,
                              float y_1,
                              float x_2,
                              float y_2,
                              cg_matrix_entry_t *modelview_entry,
                              cg_matrix_entry_t *projection_entry,
                              const float *viewport)
{
    cg_clip_stack_rect_t *entry;
    c_matrix_t modelview;
    c_matrix_t projection;
    c_matrix_t modelview_projection;

    /* Corners of the given rectangle in an clockwise order:
     *  (0, 1)     (2, 3)
     *
     *
     *
     *  (6, 7)     (4, 5)
     */
    float rect[] = { x_1, y_1, x_2, y_1, x_2, y_2, x_1, y_2 };

    /* Make a new entry */
    entry = _cg_clip_stack_push_entry(
        stack, sizeof(cg_clip_stack_rect_t), CG_CLIP_STACK_RECT);

    entry->x0 = x_1;
    entry->y0 = y_1;
    entry->x1 = x_2;
    entry->y1 = y_2;

    entry->matrix_entry = cg_matrix_entry_ref(modelview_entry);

    cg_matrix_entry_get(modelview_entry, &modelview);
    cg_matrix_entry_get(projection_entry, &projection);

    c_matrix_multiply(&modelview_projection, &projection, &modelview);

    /* Technically we could avoid the viewport transform at this point
     * if we want to make this a bit faster. */
    _cg_transform_point(&modelview, &projection, viewport, &rect[0], &rect[1]);
    _cg_transform_point(&modelview, &projection, viewport, &rect[2], &rect[3]);
    _cg_transform_point(&modelview, &projection, viewport, &rect[4], &rect[5]);
    _cg_transform_point(&modelview, &projection, viewport, &rect[6], &rect[7]);

    /* If the fully transformed rectangle isn't still axis aligned we
     * can't handle it using a scissor.
     *
     * We don't use an epsilon here since we only really aim to catch
     * simple cases where the transform doesn't leave the rectangle screen
     * aligned and don't mind some false positives.
     */
    if (rect[0] != rect[6] || rect[1] != rect[3] || rect[2] != rect[4] ||
        rect[7] != rect[5]) {
        entry->can_be_scissor = false;

        _cg_clip_stack_entry_set_bounds((cg_clip_stack_t *)entry, rect);
    } else {
        cg_clip_stack_t *base_entry = (cg_clip_stack_t *)entry;
        x_1 = rect[0];
        y_1 = rect[1];
        x_2 = rect[4];
        y_2 = rect[5];

/* Consider that the modelview matrix may flip the rectangle
 * along the x or y axis... */
#define SWAP(A, B)                                                             \
    do {                                                                       \
        float tmp = B;                                                         \
        B = A;                                                                 \
        A = tmp;                                                               \
    } while (0)
        if (x_1 > x_2)
            SWAP(x_1, x_2);
        if (y_1 > y_2)
            SWAP(y_1, y_2);
#undef SWAP

        base_entry->bounds_x0 = c_nearbyint(x_1);
        base_entry->bounds_y0 = c_nearbyint(y_1);
        base_entry->bounds_x1 = c_nearbyint(x_2);
        base_entry->bounds_y1 = c_nearbyint(y_2);
        entry->can_be_scissor = true;
    }

    return (cg_clip_stack_t *)entry;
}

cg_clip_stack_t *
_cg_clip_stack_push_primitive(cg_clip_stack_t *stack,
                              cg_primitive_t *primitive,
                              float bounds_x1,
                              float bounds_y1,
                              float bounds_x2,
                              float bounds_y2,
                              cg_matrix_entry_t *modelview_entry,
                              cg_matrix_entry_t *projection_entry,
                              const float *viewport)
{
    cg_clip_stack_primitive_t *entry;
    c_matrix_t modelview;
    c_matrix_t projection;
    float transformed_corners[8];

    entry = _cg_clip_stack_push_entry(
        stack, sizeof(cg_clip_stack_primitive_t), CG_CLIP_STACK_PRIMITIVE);

    entry->primitive = cg_object_ref(primitive);

    entry->matrix_entry = cg_matrix_entry_ref(modelview_entry);

    entry->bounds_x1 = bounds_x1;
    entry->bounds_y1 = bounds_y1;
    entry->bounds_x2 = bounds_x2;
    entry->bounds_y2 = bounds_y2;

    cg_matrix_entry_get(modelview_entry, &modelview);
    cg_matrix_entry_get(projection_entry, &projection);

    get_transformed_corners(bounds_x1,
                            bounds_y1,
                            bounds_x2,
                            bounds_y2,
                            &modelview,
                            &projection,
                            viewport,
                            transformed_corners);

    /* NB: this is referring to the bounds in window coordinates as opposed
     * to the bounds above in primitive local coordinates. */
    _cg_clip_stack_entry_set_bounds((cg_clip_stack_t *)entry,
                                    transformed_corners);

    return (cg_clip_stack_t *)entry;
}

cg_clip_stack_t *
_cg_clip_stack_ref(cg_clip_stack_t *entry)
{
    /* A NULL pointer is considered a valid stack so we should accept
       that as an argument */
    if (entry)
        entry->ref_count++;

    return entry;
}

void
_cg_clip_stack_unref(cg_clip_stack_t *entry)
{
    /* Unref all of the entries until we hit the root of the list or the
       entry still has a remaining reference */
    while (entry && --entry->ref_count <= 0) {
        cg_clip_stack_t *parent = entry->parent;

        switch (entry->type) {
        case CG_CLIP_STACK_RECT: {
            cg_clip_stack_rect_t *rect = (cg_clip_stack_rect_t *)entry;
            cg_matrix_entry_unref(rect->matrix_entry);
            c_slice_free1(sizeof(cg_clip_stack_rect_t), entry);
            break;
        }
        case CG_CLIP_STACK_WINDOW_RECT:
            c_slice_free1(sizeof(cg_clip_stack_window_rect_t), entry);
            break;
        case CG_CLIP_STACK_PRIMITIVE: {
            cg_clip_stack_primitive_t *primitive_entry =
                (cg_clip_stack_primitive_t *)entry;
            cg_matrix_entry_unref(primitive_entry->matrix_entry);
            cg_object_unref(primitive_entry->primitive);
            c_slice_free1(sizeof(cg_clip_stack_primitive_t), entry);
            break;
        }
        default:
            c_assert_not_reached();
        }

        entry = parent;
    }
}

cg_clip_stack_t *
_cg_clip_stack_pop(cg_clip_stack_t *stack)
{
    cg_clip_stack_t *new_top;

    c_return_val_if_fail(stack != NULL, NULL);

    /* To pop we are moving the top of the stack to the old top's parent
       node. The stack always needs to have a reference to the top entry
       so we must take a reference to the new top. The stack would have
       previously had a reference to the old top so we need to decrease
       the ref count on that. We need to ref the new head first in case
       this stack was the only thing referencing the old top. In that
       case the call to _cg_clip_stack_entry_unref will unref the
       parent. */
    new_top = stack->parent;

    _cg_clip_stack_ref(new_top);

    _cg_clip_stack_unref(stack);

    return new_top;
}

void
_cg_clip_stack_get_bounds(cg_clip_stack_t *stack,
                          int *scissor_x0,
                          int *scissor_y0,
                          int *scissor_x1,
                          int *scissor_y1)
{
    cg_clip_stack_t *entry;

    *scissor_x0 = 0;
    *scissor_y0 = 0;
    *scissor_x1 = INT_MAX;
    *scissor_y1 = INT_MAX;

    for (entry = stack; entry; entry = entry->parent) {
        /* Get the intersection of the current scissor and the bounding
           box of this clip */
        _cg_util_scissor_intersect(entry->bounds_x0,
                                   entry->bounds_y0,
                                   entry->bounds_x1,
                                   entry->bounds_y1,
                                   scissor_x0,
                                   scissor_y0,
                                   scissor_x1,
                                   scissor_y1);
    }
}

void
_cg_clip_stack_flush(cg_clip_stack_t *stack, cg_framebuffer_t *framebuffer)
{
    cg_device_t *dev = framebuffer->dev;

    dev->driver_vtable->clip_stack_flush(stack, framebuffer);
}
