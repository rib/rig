/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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

#ifndef __CG_SPANS_PRIVATE_H
#define __CG_SPANS_PRIVATE_H

#include "cg-object-private.h"
#include "cg-pipeline-layer-state.h"

typedef struct _cg_span_t {
    float start;
    float size;
    float waste;
} cg_span_t;

typedef struct _cg_span_iter_t {
    int index;
    const cg_span_t *spans;
    int n_spans;
    const cg_span_t *span;
    float pos;
    float next_pos;
    float origin;
    float cover_start;
    float cover_end;
    float intersect_start;
    float intersect_end;
    bool intersects;
    bool flipped;
    cg_pipeline_wrap_mode_t wrap_mode;
    int mirror_direction;
} cg_span_iter_t;

void _cg_span_iter_update(cg_span_iter_t *iter);

void _cg_span_iter_begin(cg_span_iter_t *iter,
                         const cg_span_t *spans,
                         int n_spans,
                         float normalize_factor,
                         float cover_start,
                         float cover_end,
                         cg_pipeline_wrap_mode_t wrap_mode);

void _cg_span_iter_next(cg_span_iter_t *iter);

bool _cg_span_iter_end(cg_span_iter_t *iter);

#endif /* __CG_SPANS_PRIVATE_H */
