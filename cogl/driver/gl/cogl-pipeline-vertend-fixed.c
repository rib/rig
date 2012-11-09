/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-context-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-state-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-framebuffer-private.h"

#ifdef COGL_PIPELINE_VERTEND_FIXED

#include "cogl-internal.h"
#include "cogl-context-private.h"
#include "cogl-object-private.h"

const CoglPipelineVertend _cogl_pipeline_fixed_vertend;

static void
_cogl_pipeline_vertend_fixed_start (CoglPipeline *pipeline,
                                    int n_layers,
                                    unsigned long pipelines_difference)
{
  _cogl_use_vertex_program (0, COGL_PIPELINE_PROGRAM_TYPE_FIXED);
}

static CoglBool
_cogl_pipeline_vertend_fixed_add_layer (CoglPipeline *pipeline,
                                        CoglPipelineLayer *layer,
                                        unsigned long layers_difference,
                                        CoglFramebuffer *framebuffer)
{
  return TRUE;
}

static CoglBool
_cogl_pipeline_vertend_fixed_end (CoglPipeline *pipeline,
                                  unsigned long pipelines_difference)
{
  _COGL_GET_CONTEXT (ctx, FALSE);

  if (pipelines_difference & COGL_PIPELINE_STATE_POINT_SIZE)
    {
      CoglPipeline *authority =
        _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_POINT_SIZE);

      GE( ctx, glPointSize (authority->big_state->point_size) );
    }

  return TRUE;
}

const CoglPipelineVertend _cogl_pipeline_fixed_vertend =
{
  _cogl_pipeline_vertend_fixed_start,
  _cogl_pipeline_vertend_fixed_add_layer,
  _cogl_pipeline_vertend_fixed_end,
  NULL, /* pipeline_change_notify */
  NULL /* layer_change_notify */
};

#endif /* COGL_PIPELINE_VERTEND_FIXED */

