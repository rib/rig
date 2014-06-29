/*
 * Cogl
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
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifndef __CG_PANGO_PIPELINE_CACHE_H__
#define __CG_PANGO_PIPELINE_CACHE_H__

#include <glib.h>

#include "cogl/cogl-context-private.h"

CG_BEGIN_DECLS

typedef struct _cg_pango_pipeline_cache_t {
    cg_context_t *ctx;

    GHashTable *hash_table;

    cg_pipeline_t *base_texture_alpha_pipeline;
    cg_pipeline_t *base_texture_rgba_pipeline;

    bool use_mipmapping;
} cg_pango_pipeline_cache_t;

cg_pango_pipeline_cache_t *_cg_pango_pipeline_cache_new(cg_context_t *ctx,
                                                        bool use_mipmapping);

/* Returns a pipeline that can be used to render glyphs in the given
   texture. The pipeline has a new reference so it is up to the caller
   to unref it */
cg_pipeline_t *_cg_pango_pipeline_cache_get(cg_pango_pipeline_cache_t *cache,
                                            cg_texture_t *texture);

void _cg_pango_pipeline_cache_free(cg_pango_pipeline_cache_t *cache);

CG_END_DECLS

#endif /* __CG_PANGO_PIPELINE_CACHE_H__ */
