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
 * RutMemoryStack provides a really simple, but lightning fast
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

#include <config.h>

#include "rut-memory-stack.h"
#include "rut-list.h"

#include <stdint.h>

#include <glib.h>


static RutMemorySubStack *
rut_memory_sub_stack_alloc (size_t bytes)
{
  RutMemorySubStack *sub_stack = g_slice_new (RutMemorySubStack);
  sub_stack->bytes = bytes;
  sub_stack->data = g_malloc (bytes);
  sub_stack->offset = 0;
  return sub_stack;
}

static void
rut_memory_stack_add_sub_stack (RutMemoryStack *stack,
                                  size_t sub_stack_bytes)
{
  RutMemorySubStack *sub_stack =
    rut_memory_sub_stack_alloc (sub_stack_bytes);
  rut_list_insert (stack->sub_stacks.prev, &sub_stack->list_node);
  stack->sub_stack = sub_stack;
}

RutMemoryStack *
rut_memory_stack_new (size_t initial_size_bytes)
{
  RutMemoryStack *stack = g_slice_new0 (RutMemoryStack);

  rut_list_init (&stack->sub_stacks);

  rut_memory_stack_add_sub_stack (stack, initial_size_bytes);

  return stack;
}

void *
_rut_memory_stack_alloc_in_next_sub_stack (RutMemoryStack *stack,
                                           size_t bytes)
{
  RutMemorySubStack *sub_stack;

  /* If the stack has been rewound and then a large initial allocation
   * is made then we may need to skip over one or more of the
   * sub-stacks that are too small for the requested allocation
   * size...
   *
   * XXX: note we don't use rut_list_for_each here because we need to be
   * careful to iterate the list starting from our current position, but
   * stopping if we loop back to the first sub_stack. (NB: A RutList
   * is a circular list)
   */
  for (sub_stack = rut_container_of (stack->sub_stack->list_node.next,
                                     sub_stack, list_node);
       &sub_stack->list_node != &stack->sub_stacks;
       sub_stack = rut_container_of (sub_stack->list_node.next,
                                     sub_stack, list_node))
    {
      if (sub_stack->bytes >= bytes)
        {
          stack->sub_stack = sub_stack;
          sub_stack->offset = bytes;
          return sub_stack->data;
        }
    }

  /* If we couldn't find a free sub-stack with enough space for the
   * requested allocation we allocate another sub-stack that's twice
   * as big as the last sub-stack or twice as big as the requested
   * allocation if that's bigger.
   */

  sub_stack = rut_container_of (stack->sub_stacks.prev,
                                sub_stack,
                                list_node);

  rut_memory_stack_add_sub_stack (stack, MAX (sub_stack->bytes, bytes) * 2);

  sub_stack = rut_container_of (stack->sub_stacks.prev,
                                sub_stack,
                                list_node);

  sub_stack->offset += bytes;

  return sub_stack->data;
}

void
rut_memory_stack_foreach_region (RutMemoryStack *stack,
                                 RutMemoryStackRegionCallback callback,
                                 void *user_data)
{
  RutMemorySubStack *sub_stack, *tmp;

  rut_list_for_each_safe (sub_stack, tmp, &stack->sub_stacks, list_node)
    {
      callback (sub_stack->data, sub_stack->offset, user_data);
      if (sub_stack == stack->sub_stack)
        return;
    }
}

static void
rut_memory_sub_stack_free (RutMemorySubStack *sub_stack)
{
  g_free (sub_stack->data);
  g_slice_free (RutMemorySubStack, sub_stack);
}

void
rut_memory_stack_rewind (RutMemoryStack *stack)
{
  RutMemorySubStack *last_sub_stack = rut_container_of (stack->sub_stacks.prev,
                                                        last_sub_stack,
                                                        list_node);
  RutMemorySubStack *sub_stack;

  for (sub_stack = rut_container_of (stack->sub_stacks.next,
                                     sub_stack, list_node);
       sub_stack != last_sub_stack;
       sub_stack = rut_container_of (stack->sub_stacks.next,
                                     sub_stack, list_node))
    {
      rut_list_remove (&sub_stack->list_node);
      rut_memory_sub_stack_free (sub_stack);
    }

  /* Just keep the largest sub-stack to try and help reduce fragmentation */

  stack->sub_stack = last_sub_stack;
  stack->sub_stack->offset = 0;
}

void
rut_memory_stack_free (RutMemoryStack *stack)
{
  RutMemorySubStack *sub_stack, *tmp;

  rut_list_for_each_safe (sub_stack, tmp, &stack->sub_stacks, list_node)
    rut_memory_sub_stack_free (sub_stack);

  g_slice_free (RutMemoryStack, stack);
}
