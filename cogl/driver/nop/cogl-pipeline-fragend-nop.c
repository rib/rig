/*
 * Cogl
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

#include <config.h>

#include "cogl-pipeline-private.h"

#include <ulib.h>

const CoglPipelineFragend _cogl_pipeline_nop_fragend;

static void
_cogl_pipeline_fragend_nop_start (CoglPipeline *pipeline,
                                  int n_layers,
                                  unsigned long pipelines_difference)
{
}

static CoglBool
_cogl_pipeline_fragend_nop_add_layer (CoglPipeline *pipeline,
                                      CoglPipelineLayer *layer,
                                      unsigned long layers_difference)
{
  return TRUE;
}

static CoglBool
_cogl_pipeline_fragend_nop_end (CoglPipeline *pipeline,
                                unsigned long pipelines_difference)
{
  return TRUE;
}

const CoglPipelineFragend _cogl_pipeline_nop_fragend =
{
  _cogl_pipeline_fragend_nop_start,
  _cogl_pipeline_fragend_nop_add_layer,
  NULL, /* passthrough */
  _cogl_pipeline_fragend_nop_end,
  NULL, /* pipeline_change_notify */
  NULL, /* pipeline_set_parent_notify */
  NULL, /* layer_change_notify */
};

