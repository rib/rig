/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
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
 * cg_memory_stack_t provides a really simple, but lightning fast
 * memory stack allocation strategy:
 *
 * - The underlying pool of memory is grow-only.
 * - The pool is considered to be a stack which may be comprised
 *   of multiple smaller stacks. Allocation is done as follows:
 *    - If there's enough memory in the current sub-stack then the
 *      stack-pointer will be returned as the allocation and the
 *      stack-pointer will be incremented by the allocation size.
 *    - If there isn't enough memory in the current sub-stack
 *      then a new sub-stack is allocated twice as big as the current
 *      sub-stack or twice as big as the requested allocation size if
 *      that's bigger and the stack-pointer is set to the start of the
 *      new sub-stack.
 * - Allocations can't be freed in a random-order, you can only
 *   rewind the entire stack back to the start. There is no
 *   the concept of stack frames to allow partial rewinds.
 *
 * For example; we plan to use this in our tesselator which has to
 * allocate lots of small vertex, edge and face structures because
 * when tesselation has been finished we just want to free the whole
 * lot in one go.
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#include <cglib-config.h>

#include <stdint.h>

#include <clib.h>

#include "cg-memory-stack-private.h"


typedef struct _cg_memory_sub_stack_t {
    c_list_t link;
    size_t bytes;
    uint8_t *data;
} cg_memory_sub_stack_t;

struct _cg_memory_stack_t {
    c_list_t sub_stacks;

    cg_memory_sub_stack_t *sub_stack;
    size_t sub_stack_offset;
};

static cg_memory_sub_stack_t *
_cg_memory_sub_stack_alloc(size_t bytes)
{
    cg_memory_sub_stack_t *sub_stack = c_slice_new(cg_memory_sub_stack_t);
    sub_stack->bytes = bytes;
    sub_stack->data = c_malloc(bytes);
    return sub_stack;
}

static void
_cg_memory_stack_add_sub_stack(cg_memory_stack_t *stack,
                               size_t sub_stack_bytes)
{
    cg_memory_sub_stack_t *sub_stack =
        _cg_memory_sub_stack_alloc(sub_stack_bytes);
    c_list_insert(stack->sub_stacks.prev, &sub_stack->link);
    stack->sub_stack = sub_stack;
    stack->sub_stack_offset = 0;
}

cg_memory_stack_t *
_cg_memory_stack_new(size_t initial_size_bytes)
{
    cg_memory_stack_t *stack = c_slice_new0(cg_memory_stack_t);

    c_list_init(&stack->sub_stacks);

    _cg_memory_stack_add_sub_stack(stack, initial_size_bytes);

    return stack;
}

void *
_cg_memory_stack_alloc(cg_memory_stack_t *stack, size_t bytes)
{
    cg_memory_sub_stack_t *sub_stack;
    void *ret;

    sub_stack = stack->sub_stack;
    if (C_LIKELY(sub_stack->bytes - stack->sub_stack_offset >= bytes)) {
        ret = sub_stack->data + stack->sub_stack_offset;
        stack->sub_stack_offset += bytes;
        return ret;
    }

    /* If the stack has been rewound and then a large initial allocation
     * is made then we may need to skip over one or more of the
     * sub-stacks that are too small for the requested allocation
     * size... */
    for (c_list_set_iterator(sub_stack->link.next, sub_stack, link);
         &sub_stack->link != &stack->sub_stacks;
         c_list_set_iterator(sub_stack->link.next, sub_stack, link)) {
        if (sub_stack->bytes >= bytes) {
            ret = sub_stack->data;
            stack->sub_stack = sub_stack;
            stack->sub_stack_offset = bytes;
            return ret;
        }
    }

    /* Finally if we couldn't find a free sub-stack with enough space
     * for the requested allocation we allocate another sub-stack that's
     * twice as big as the last sub-stack or twice as big as the
     * requested allocation if that's bigger.
     */

    sub_stack =
        c_container_of(stack->sub_stacks.prev, cg_memory_sub_stack_t, link);

    _cg_memory_stack_add_sub_stack(stack, MAX(sub_stack->bytes, bytes) * 2);

    sub_stack =
        c_container_of(stack->sub_stacks.prev, cg_memory_sub_stack_t, link);

    stack->sub_stack_offset += bytes;

    return sub_stack->data;
}

void
_cg_memory_stack_rewind(cg_memory_stack_t *stack)
{
    stack->sub_stack =
        c_container_of(stack->sub_stacks.next, cg_memory_sub_stack_t, link);
    stack->sub_stack_offset = 0;
}

static void
_cg_memory_sub_stack_free(cg_memory_sub_stack_t *sub_stack)
{
    c_free(sub_stack->data);
    c_slice_free(cg_memory_sub_stack_t, sub_stack);
}

void
_cg_memory_stack_free(cg_memory_stack_t *stack)
{

    while (!c_list_empty(&stack->sub_stacks)) {
        cg_memory_sub_stack_t *sub_stack = c_container_of(
            stack->sub_stacks.next, cg_memory_sub_stack_t, link);
        c_list_remove(&sub_stack->link);
        _cg_memory_sub_stack_free(sub_stack);
    }

    c_slice_free(cg_memory_stack_t, stack);
}
