/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Intel Corporation.
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

#ifndef __CG_SAMPLER_CACHE_PRIVATE_H
#define __CG_SAMPLER_CACHE_PRIVATE_H

#include "cg-device.h"
#include "cg-gl-header.h"

/* These aren't defined in the GLES headers */
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812d
#endif
#ifndef GL_MIRRORED_REPEAT
#define GL_MIRRORED_REPEAT 0x8370
#endif

/*
 * XXX: keep the values in sync with the cg_pipeline_wrap_mode_t enum
 * so no conversion is actually needed.
 */
typedef enum _cg_sampler_cache_wrap_mode_t {
    CG_SAMPLER_CACHE_WRAP_MODE_REPEAT = GL_REPEAT,
    CG_SAMPLER_CACHE_WRAP_MODE_MIRRORED_REPEAT = GL_MIRRORED_REPEAT,
    CG_SAMPLER_CACHE_WRAP_MODE_CLAMP_TO_EDGE = GL_CLAMP_TO_EDGE,
    CG_SAMPLER_CACHE_WRAP_MODE_CLAMP_TO_BORDER = GL_CLAMP_TO_BORDER,
} cg_sampler_cache_wrap_mode_t;

typedef struct _cg_sampler_cache_t cg_sampler_cache_t;

typedef struct _cg_sampler_cache_entry_t {
    GLuint sampler_object;

    GLenum min_filter;
    GLenum mag_filter;

    cg_sampler_cache_wrap_mode_t wrap_mode_s;
    cg_sampler_cache_wrap_mode_t wrap_mode_t;
    cg_sampler_cache_wrap_mode_t wrap_mode_p;
} cg_sampler_cache_entry_t;

cg_sampler_cache_t *_cg_sampler_cache_new(cg_device_t *dev);

const cg_sampler_cache_entry_t *
_cg_sampler_cache_get_default_entry(cg_sampler_cache_t *cache);

const cg_sampler_cache_entry_t *
_cg_sampler_cache_update_wrap_modes(cg_sampler_cache_t *cache,
                                    const cg_sampler_cache_entry_t *old_entry,
                                    cg_sampler_cache_wrap_mode_t wrap_mode_s,
                                    cg_sampler_cache_wrap_mode_t wrap_mode_t,
                                    cg_sampler_cache_wrap_mode_t wrap_mode_p);

const cg_sampler_cache_entry_t *
_cg_sampler_cache_update_filters(cg_sampler_cache_t *cache,
                                 const cg_sampler_cache_entry_t *old_entry,
                                 GLenum min_filter,
                                 GLenum mag_filter);

void _cg_sampler_cache_free(cg_sampler_cache_t *cache);

#endif /* __CG_SAMPLER_CACHE_PRIVATE_H */
