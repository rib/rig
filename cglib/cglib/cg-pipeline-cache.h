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
 */

#ifndef __CG_PIPELINE_CACHE_H__
#define __CG_PIPELINE_CACHE_H__

#include "cg-pipeline.h"

typedef struct _cg_pipeline_cache_t cg_pipeline_cache_t;

typedef struct {
    cg_pipeline_t *pipeline;

    /* Number of usages of this template. If this drops to zero then it
     * will be a candidate for removal from the cache */
    int usage_count;
} cg_pipeline_cache_entry_t;

cg_pipeline_cache_t *_cg_pipeline_cache_new(cg_device_t *dev);

void _cg_pipeline_cache_free(cg_pipeline_cache_t *cache);

/*
 * Gets a pipeline from the cache that has the same state as
 * @key_pipeline for the state in
 * CG_PIPELINE_STATE_AFFECTS_FRAGMENT_CODEGEN. If there is no
 * matching pipline already then a copy of key_pipeline is stored in
 * the cache so that it will be used next time the function is called
 * with a similar pipeline. In that case the copy itself will be
 * returned
 */
cg_pipeline_cache_entry_t *
_cg_pipeline_cache_get_fragment_template(cg_pipeline_cache_t *cache,
                                         cg_pipeline_t *key_pipeline);

/*
 * Gets a pipeline from the cache that has the same state as
 * @key_pipeline for the state in
 * CG_PIPELINE_STATE_AFFECTS_VERTEX_CODEGEN. If there is no
 * matching pipline already then a copy of key_pipeline is stored in
 * the cache so that it will be used next time the function is called
 * with a similar pipeline. In that case the copy itself will be
 * returned
 */
cg_pipeline_cache_entry_t *
_cg_pipeline_cache_get_vertex_template(cg_pipeline_cache_t *cache,
                                       cg_pipeline_t *key_pipeline);

/*
 * Gets a pipeline from the cache that has the same state as
 * @key_pipeline for the combination of the state state in
 * CG_PIPELINE_STATE_AFFECTS_VERTEX_CODEGEN and
 * CG_PIPELINE_STATE_AFFECTS_FRAGMENT_CODEGEN. If there is no
 * matching pipline already then a copy of key_pipeline is stored in
 * the cache so that it will be used next time the function is called
 * with a similar pipeline. In that case the copy itself will be
 * returned
 */
cg_pipeline_cache_entry_t *
_cg_pipeline_cache_get_combined_template(cg_pipeline_cache_t *cache,
                                         cg_pipeline_t *key_pipeline);

#endif /* __CG_PIPELINE_CACHE_H__ */
