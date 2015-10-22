/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2014 Intel Corporation.
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

#include <cglib-config.h>

#include "cg-device-private.h"
#include "cg-pipeline-private.h"
#include "cg-pipeline-state-private.h"
#include "cg-framebuffer-private.h"

#include "cg-device-private.h"
#include "cg-object-private.h"

const cg_pipeline_vertend_t _cg_pipeline_nop_vertend;

static void
_cg_pipeline_vertend_nop_start(cg_device_t *dev,
                               cg_pipeline_t *pipeline,
                               int n_layers,
                               unsigned long pipelines_difference)
{
}

static bool
_cg_pipeline_vertend_nop_add_layer(cg_device_t *dev,
                                   cg_pipeline_t *pipeline,
                                   cg_pipeline_layer_t *layer,
                                   unsigned long layers_difference,
                                   cg_framebuffer_t *framebuffer)
{
    return true;
}

static bool
_cg_pipeline_vertend_nop_end(cg_device_t *dev,
                             cg_pipeline_t *pipeline,
                             unsigned long pipelines_difference)
{
    return true;
}

const cg_pipeline_vertend_t _cg_pipeline_nop_vertend = {
    _cg_pipeline_vertend_nop_start, _cg_pipeline_vertend_nop_add_layer,
    _cg_pipeline_vertend_nop_end,   NULL, /* pipeline_change_notify */
    NULL /* layer_change_notify */
};
