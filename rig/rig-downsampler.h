/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012  Intel Corporation
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
 */

#ifndef __RIG_DOWNSAMPLER_H__
#define __RIG_DOWNSAMPLER_H__

#include <cglib/cglib.h>

#include <rut.h>

#include "rig-engine.h"

typedef struct {
    rig_engine_t *engine;
    cg_pipeline_t *pipeline;
    cg_texture_t *dest;
    cg_framebuffer_t *fb;
    rut_object_t *camera;
} rig_downsampler_t;

rig_downsampler_t *rig_downsampler_new(rig_engine_t *engine);

void rig_downsampler_free(rig_downsampler_t *downsampler);

cg_texture_t *rig_downsampler_downsample(rig_downsampler_t *downsampler,
                                         cg_texture_t *source,
                                         int scale_factor_x,
                                         int scale_factor_y);

#endif /* __RIG_DOWNSAMPLER_H__ */
