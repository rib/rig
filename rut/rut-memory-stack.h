/*
 * Rut
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#include <glib.h>

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
