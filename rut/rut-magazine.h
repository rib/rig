/*
 * Copyright (c) 2012,2014 Intel Corporation.
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
 */

#ifndef __RUT_MAGAZINE_H__
#define __RUT_MAGAZINE_H__

#include <clib.h>

#include "rut-memory-stack.h"

C_BEGIN_DECLS

typedef struct _rut_magazine_chunk_t rut_magazine_chunk_t;

struct _rut_magazine_chunk_t {
    rut_magazine_chunk_t *next;
};

typedef struct _rut_magazine_t {
    size_t chunk_size;

    rut_memory_stack_t *stack;
    rut_magazine_chunk_t *head;
} rut_magazine_t;

rut_magazine_t *rut_magazine_new(size_t chunk_size, int initial_chunk_count);

static inline void *
rut_magazine_chunk_alloc(rut_magazine_t *magazine)
{
    if (C_LIKELY(magazine->head)) {
        rut_magazine_chunk_t *chunk = magazine->head;
        magazine->head = chunk->next;
        return chunk;
    } else
        return rut_memory_stack_alloc(magazine->stack, magazine->chunk_size);
}

static inline void
rut_magazine_chunk_free(rut_magazine_t *magazine, void *data)
{
    rut_magazine_chunk_t *chunk = (rut_magazine_chunk_t *)data;

    chunk->next = magazine->head;
    magazine->head = chunk;
}

void rut_magazine_free(rut_magazine_t *magazine);

C_END_DECLS

#endif /* __RUT_MAGAZINE_H__ */
