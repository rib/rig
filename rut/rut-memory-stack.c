/*
 * Rut
 *
 * Copyright (C) 2011 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
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

typedef struct _RutMemorySubStack RutMemorySubStack;

struct _RutMemorySubStack
{
  RutList list_node;
  size_t bytes;
  uint8_t *data;
};

struct _RutMemoryStack
{
  RutList sub_stacks;

  RutMemorySubStack *sub_stack;
  size_t sub_stack_offset;
};

static RutMemorySubStack *
rut_memory_sub_stack_alloc (size_t bytes)
{
  RutMemorySubStack *sub_stack = g_slice_new (RutMemorySubStack);
  sub_stack->bytes = bytes;
  sub_stack->data = g_malloc (bytes);
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
  stack->sub_stack_offset = 0;
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
rut_memory_stack_alloc (RutMemoryStack *stack, size_t bytes)
{
  RutMemorySubStack *sub_stack;
  void *ret;

  sub_stack = stack->sub_stack;
  if (G_LIKELY (sub_stack->bytes - stack->sub_stack_offset >= bytes))
    {
      ret = sub_stack->data + stack->sub_stack_offset;
      stack->sub_stack_offset += bytes;
      return ret;
    }

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

  sub_stack = rut_container_of (stack->sub_stacks.prev,
                                sub_stack,
                                list_node);

  rut_memory_stack_add_sub_stack (stack, MAX (sub_stack->bytes, bytes) * 2);

  sub_stack = rut_container_of (stack->sub_stacks.prev,
                                sub_stack,
                                list_node);

  stack->sub_stack_offset += bytes;

  return sub_stack->data;
}

void
rut_memory_stack_rewind (RutMemoryStack *stack)
{
  stack->sub_stack = rut_container_of (stack->sub_stacks.next,
                                       stack->sub_stack,
                                       list_node);
  stack->sub_stack_offset = 0;
}

static void
rut_memory_sub_stack_free (RutMemorySubStack *sub_stack)
{
  g_free (sub_stack->data);
  g_slice_free (RutMemorySubStack, sub_stack);
}

void
rut_memory_stack_free (RutMemoryStack *stack)
{
  RutMemorySubStack *sub_stack, *tmp;

  rut_list_for_each_safe (sub_stack, tmp, &stack->sub_stacks, list_node)
    rut_memory_sub_stack_free (sub_stack);

  g_slice_free (RutMemoryStack, stack);
}
