/*
 * Rut
 *
 * Rig Utilities
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
 * rut_memory_stack_t provides a really simple, but fast stack
 * allocation strategy:
 *
 * - The underlying pool of memory is grow-only.
 * - The pool is considered to be a stack which may be comprised of
 *   multiple smaller sub-stacks (such that resizing never involves
 *   copying existing data) and allocation is done as follows:
 *
 *    - If there's enough memory in the current sub-stack then the
 *      stack-pointer will be returned as the allocation and the
 *      stack-pointer will be incremented by the allocation size.
 *    - If there isn't enough memory in the current sub-stack then
 *      a new one is allocated such that the overall pool size grows
 *      exponentially and the stack-pointer is set to the start of
 *      the new sub-stack.
 *
 * - Allocations can't be freed in a random-order, you can only
 *   rewind the entire stack back to the start. There is currently no
 *   concept of stack frames to allow partial rewinds.
 * - The implementation is not threadsafe, though it doesn't require
 *   access to any global resources so users could provide their
 *   own locking if a stack needs to be shared between threads.
 */

#include <rut-config.h>

#include <stdint.h>

#include <clib.h>

#include "rut-memory-stack.h"

static rut_memory_sub_stack_t *
rut_memory_sub_stack_alloc(size_t bytes)
{
    rut_memory_sub_stack_t *sub_stack = c_slice_new(rut_memory_sub_stack_t);
    sub_stack->bytes = bytes;
    sub_stack->data = c_malloc(bytes);
    sub_stack->offset = 0;
    return sub_stack;
}

static void
rut_memory_stack_add_sub_stack(rut_memory_stack_t *stack,
                               size_t sub_stack_bytes)
{
    rut_memory_sub_stack_t *sub_stack =
        rut_memory_sub_stack_alloc(sub_stack_bytes);
    c_list_insert(stack->sub_stacks.prev, &sub_stack->link);
    stack->sub_stack = sub_stack;
}

rut_memory_stack_t *
rut_memory_stack_new(size_t initial_size_bytes)
{
    rut_memory_stack_t *stack = c_slice_new0(rut_memory_stack_t);

    c_list_init(&stack->sub_stacks);

    rut_memory_stack_add_sub_stack(stack, initial_size_bytes);

    return stack;
}

void *
_rut_memory_stack_alloc_in_next_sub_stack(rut_memory_stack_t *stack,
                                          size_t bytes)
{
    rut_memory_sub_stack_t *sub_stack;
    void *ret;
    size_t total_size = 0;
    size_t new_sub_stack_size;

    sub_stack = stack->sub_stack;
    if (C_LIKELY(sub_stack->bytes - sub_stack->offset >= bytes)) {
        ret = sub_stack->data + sub_stack->offset;
        sub_stack->offset += bytes;
        return ret;
    }

    /* If the stack has been rewound and then a large initial allocation
     * is made then we may need to skip over one or more of the
     * sub-stacks that are too small for the requested allocation
     * size...
     *
     * XXX: note we don't use c_list_for_each here because we need to be
     * careful to iterate the list starting from our current position, but
     * stopping if we loop back to the first sub_stack. (NB: A c_list_t
     * is a circular list)
     */
    for (c_list_set_iterator(sub_stack->link.next, sub_stack, link);
         &sub_stack->link != &stack->sub_stacks;
         c_list_set_iterator(sub_stack->link.next, sub_stack, link)) {
        if (sub_stack->bytes >= bytes) {
            ret = sub_stack->data;
            stack->sub_stack = sub_stack;
            sub_stack->offset = bytes;
            return ret;
        }
    }

    /* If we couldn't find a free sub-stack with enough space for the
     * requested allocation we allocate another sub-stack that's at
     * least half the current total stack size.
     */

    c_list_for_each(sub_stack, &stack->sub_stacks, link)
        total_size += sub_stack->bytes;

    new_sub_stack_size = total_size / 2;
    if (new_sub_stack_size < bytes * 2)
        new_sub_stack_size = bytes * 2;

    rut_memory_stack_add_sub_stack(stack, new_sub_stack_size);

    sub_stack =
        c_container_of(stack->sub_stacks.prev, rut_memory_sub_stack_t, link);

    sub_stack->offset += bytes;

    return sub_stack->data;
}

static void
rut_memory_sub_stack_free(rut_memory_sub_stack_t *sub_stack)
{
    c_free(sub_stack->data);
    c_slice_free(rut_memory_sub_stack_t, sub_stack);
}

void
rut_memory_stack_rewind(rut_memory_stack_t *stack)
{
    rut_memory_sub_stack_t *last_sub_stack =
        c_container_of(stack->sub_stacks.prev, rut_memory_sub_stack_t, link);
    rut_memory_sub_stack_t *sub_stack, *tmp;

    /* expanded c_list_for_each_safe() so we can we can terminate
     * before the last element... */
    for (c_list_set_iterator(stack->sub_stacks.next, sub_stack, link),
         c_list_set_iterator(sub_stack->link.next, tmp, link);
         sub_stack != last_sub_stack;
         sub_stack = tmp, c_list_set_iterator(sub_stack->link.next, tmp, link))
    {
        c_list_remove(&sub_stack->link);
        rut_memory_sub_stack_free(sub_stack);
    }

    /* Just keep the largest sub-stack to try and help reduce fragmentation */

    stack->sub_stack = last_sub_stack;
    stack->sub_stack->offset = 0;
}

void
rut_memory_stack_free(rut_memory_stack_t *stack)
{
    rut_memory_sub_stack_t *sub_stack, *tmp;

    c_list_for_each_safe(sub_stack, tmp, &stack->sub_stacks, link)
    rut_memory_sub_stack_free(sub_stack);

    c_slice_free(rut_memory_stack_t, stack);
}
