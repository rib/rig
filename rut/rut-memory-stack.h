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
 */

#include <clib.h>

#include <stdint.h>

#include "rut-list.h"

#ifndef __RUT_MEMORY_STACK__
#define __RUT_MEMORY_STACK__

G_BEGIN_DECLS


typedef struct _RutMemoryStack RutMemoryStack;
typedef struct _RutMemorySubStack RutMemorySubStack;

struct _RutMemorySubStack
{
  RutList list_node;
  uint8_t *data;
  size_t bytes;
  size_t offset;
};

struct _RutMemoryStack
{
  RutList sub_stacks;

  RutMemorySubStack *sub_stack;
};


RutMemoryStack *
rut_memory_stack_new (size_t initial_size_bytes);

void *
_rut_memory_stack_alloc_in_next_sub_stack (RutMemoryStack *stack,
                                           size_t bytes);

static inline size_t
_rut_memory_stack_align (size_t base, int alignment)
{
  return (base + alignment - 1) & ~(alignment - 1);
}

static inline void *
rut_memory_stack_memalign (RutMemoryStack *stack,
                           size_t bytes,
                           size_t alignment)
{
  RutMemorySubStack *sub_stack = stack->sub_stack;
  size_t offset = _rut_memory_stack_align (stack->sub_stack->offset, alignment);

  if (G_LIKELY (sub_stack->bytes - offset >= bytes))
    {
      void *ret = sub_stack->data + offset;
      stack->sub_stack->offset = offset + bytes;
      return ret;
    }
  else
    return _rut_memory_stack_alloc_in_next_sub_stack (stack, bytes);
}

static inline void *
rut_memory_stack_alloc (RutMemoryStack *stack,
                        size_t bytes)
{
  RutMemorySubStack *sub_stack = stack->sub_stack;

  if (G_LIKELY (sub_stack->bytes - stack->sub_stack->offset >= bytes))
    {
      void *ret = sub_stack->data + stack->sub_stack->offset;
      stack->sub_stack->offset += bytes;
      return ret;
    }
  else
    return _rut_memory_stack_alloc_in_next_sub_stack (stack, bytes);
}


typedef void (*RutMemoryStackRegionCallback) (uint8_t *region,
                                              size_t bytes,
                                              void *user_data);

void
rut_memory_stack_foreach_region (RutMemoryStack *stack,
                                 RutMemoryStackRegionCallback callback,
                                 void *user_data);

void
rut_memory_stack_rewind (RutMemoryStack *stack);

void
rut_memory_stack_free (RutMemoryStack *stack);


G_END_DECLS

#endif /* __RUT_MEMORY_STACK__ */
