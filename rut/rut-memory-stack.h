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

#include <stdint.h>

#include <clib.h>

#ifndef __RUT_MEMORY_STACK__
#define __RUT_MEMORY_STACK__

C_BEGIN_DECLS

typedef struct _rut_memory_stack_t rut_memory_stack_t;
typedef struct _rut_memory_sub_stack_t rut_memory_sub_stack_t;

struct _rut_memory_sub_stack_t {
    c_list_t link;
    uint8_t *data;
    size_t bytes;
    size_t offset;
};

struct _rut_memory_stack_t {
    c_list_t sub_stacks;

    rut_memory_sub_stack_t *sub_stack;
};

rut_memory_stack_t *rut_memory_stack_new(size_t initial_size_bytes);

void *_rut_memory_stack_alloc_in_next_sub_stack(rut_memory_stack_t *stack,
                                                size_t bytes);

static inline size_t
_rut_memory_stack_align(size_t base, int alignment)
{
    return (base + alignment - 1) & ~(alignment - 1);
}

static inline void *
rut_memory_stack_memalign(rut_memory_stack_t *stack,
                          size_t bytes,
                          size_t alignment)
{
    rut_memory_sub_stack_t *sub_stack = stack->sub_stack;
    size_t offset =
        _rut_memory_stack_align(stack->sub_stack->offset, alignment);

    if (C_LIKELY(sub_stack->bytes - offset >= bytes)) {
        void *ret = sub_stack->data + offset;
        stack->sub_stack->offset = offset + bytes;
        return ret;
    } else
        return _rut_memory_stack_alloc_in_next_sub_stack(stack, bytes);
}

static inline void *
rut_memory_stack_alloc(rut_memory_stack_t *stack,
                       size_t bytes)
{
    rut_memory_sub_stack_t *sub_stack = stack->sub_stack;

    if (C_LIKELY(sub_stack->bytes - stack->sub_stack->offset >= bytes)) {
        void *ret = sub_stack->data + stack->sub_stack->offset;
        stack->sub_stack->offset += bytes;
        return ret;
    } else
        return _rut_memory_stack_alloc_in_next_sub_stack(stack, bytes);
}

typedef void (*rut_memory_stack_region_callback_t)(uint8_t *region,
                                                   size_t bytes,
                                                   void *user_data);

/* C++ will gets upset with how c_list_for_each_safe is implemented */
#ifndef __cplusplus

static inline void
rut_memory_stack_foreach_region(rut_memory_stack_t *stack,
                                rut_memory_stack_region_callback_t callback,
                                void *user_data)
{
    rut_memory_sub_stack_t *sub_stack, *tmp;

    c_list_for_each_safe(sub_stack, tmp, &stack->sub_stacks, link) {
        callback(sub_stack->data, sub_stack->offset, user_data);
        if (sub_stack == stack->sub_stack)
            return;
    }
}
#endif /* __cplusplus */

void rut_memory_stack_rewind(rut_memory_stack_t *stack);

void rut_memory_stack_free(rut_memory_stack_t *stack);

C_END_DECLS

#endif /* __RUT_MEMORY_STACK__ */
