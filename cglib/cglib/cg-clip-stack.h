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

#ifndef __CG_CLIP_STACK_H
#define __CG_CLIP_STACK_H

#include "cg-primitive.h"
#include "cg-framebuffer.h"
#include "cg-matrix-stack.h"

/* The clip stack works like a c_sllist_t where only a pointer to the top
   of the stack is stored. The empty clip stack is represented simply
   by the NULL pointer. When an entry is added to or removed from the
   stack the new top of the stack is returned. When an entry is pushed
   a new clip stack entry is created which effectively takes ownership
   of the reference on the old entry. Therefore unrefing the top entry
   effectively loses ownership of all entries in the stack */

typedef struct _cg_clip_stack_t cg_clip_stack_t;
typedef struct _cg_clip_stack_rect_t cg_clip_stack_rect_t;
typedef struct _cg_clip_stack_window_rect_t cg_clip_stack_window_rect_t;
typedef struct _cg_clip_stack_primitive_t cg_clip_stack_primitive_t;

typedef enum {
    CG_CLIP_STACK_RECT,
    CG_CLIP_STACK_WINDOW_RECT,
    CG_CLIP_STACK_PRIMITIVE
} cg_clip_stack_type_t;

/* A clip stack consists a list of entries. Each entry has a reference
 * count and a link to its parent node. The child takes a reference on
 * the parent and the cg_clip_stack_t holds a reference to the top of
 * the stack. There are no links back from the parent to the
 * children. This allows stacks that have common ancestry to share the
 * entries.
 *
 * For example, the following sequence of operations would generate
 * the tree below:
 *
 * cg_clip_stack_t *stack_a = NULL;
 * stack_a = _cg_clip_stack_push_rectangle (stack_a, ...);
 * stack_a = _cg_clip_stack_push_rectangle (stack_a, ...);
 * stack_a = _cg_clip_stack_push_primitive (stack_a, ...);
 * cg_clip_stack_t *stack_b = NULL;
 * stack_b = cg_clip_stack_push_window_rectangle (stack_b, ...);
 *
 *  stack_a
 *         \ holds a ref to
 *          +-----------+
 *          | prim node |
 *          |ref count 1|
 *          +-----------+
 *                       \
 *                        +-----------+  +-----------+
 *       both tops hold   | rect node |  | rect node |
 *       a ref to the     |ref count 2|--|ref count 1|
 *       same rect node   +-----------+  +-----------+
 *                       /
 *          +-----------+
 *          | win. rect |
 *          |ref count 1|
 *          +-----------+
 *         / holds a ref to
 *  stack_b
 *
 */

struct _cg_clip_stack_t {
    /* This will be null if there is no parent. If it is not null then
       this node must be holding a reference to the parent */
    cg_clip_stack_t *parent;

    cg_clip_stack_type_t type;

    /* All clip entries have a window-space bounding box which we can
       use to calculate a scissor. The scissor limits the clip so that
       we don't need to do a full stencil clear if the stencil buffer is
       needed. This is stored in CGlib's coordinate space (ie, 0,0 is the
       top left) */
    int bounds_x0;
    int bounds_y0;
    int bounds_x1;
    int bounds_y1;

    unsigned int ref_count;
};

struct _cg_clip_stack_rect_t {
    cg_clip_stack_t _parent_data;

    /* The rectangle for this clip */
    float x0;
    float y0;
    float x1;
    float y1;

    /* The matrix that was current when the clip was set */
    cg_matrix_entry_t *matrix_entry;

    /* If this is true then the clip for this rectangle is entirely
       described by the scissor bounds. This implies that the rectangle
       is screen aligned and we don't need to use the stencil buffer to
       set the clip. */
    bool can_be_scissor;
};

struct _cg_clip_stack_window_rect_t {
    cg_clip_stack_t _parent_data;

    /* The window rect clip doesn't need any specific data because it
       just adds to the scissor clip */
};

struct _cg_clip_stack_primitive_t {
    cg_clip_stack_t _parent_data;

    /* The matrix that was current when the clip was set */
    cg_matrix_entry_t *matrix_entry;

    cg_primitive_t *primitive;

    float bounds_x1;
    float bounds_y1;
    float bounds_x2;
    float bounds_y2;
};

cg_clip_stack_t *_cg_clip_stack_push_window_rectangle(
    cg_clip_stack_t *stack, int x_offset, int y_offset, int width, int height);

cg_clip_stack_t *
_cg_clip_stack_push_rectangle(cg_clip_stack_t *stack,
                              float x_1,
                              float y_1,
                              float x_2,
                              float y_2,
                              cg_matrix_entry_t *modelview_entry,
                              cg_matrix_entry_t *projection_entry,
                              const float *viewport);

cg_clip_stack_t *
_cg_clip_stack_push_primitive(cg_clip_stack_t *stack,
                              cg_primitive_t *primitive,
                              float bounds_x1,
                              float bounds_y1,
                              float bounds_x2,
                              float bounds_y2,
                              cg_matrix_entry_t *modelview_entry,
                              cg_matrix_entry_t *projection_entry,
                              const float *viewport);

cg_clip_stack_t *_cg_clip_stack_pop(cg_clip_stack_t *stack);

void _cg_clip_stack_get_bounds(cg_clip_stack_t *stack,
                               int *scissor_x0,
                               int *scissor_y0,
                               int *scissor_x1,
                               int *scissor_y1);

void _cg_clip_stack_flush(cg_clip_stack_t *stack,
                          cg_framebuffer_t *framebuffer);

cg_clip_stack_t *_cg_clip_stack_ref(cg_clip_stack_t *stack);

void _cg_clip_stack_unref(cg_clip_stack_t *stack);

#endif /* __CG_CLIP_STACK_H */
