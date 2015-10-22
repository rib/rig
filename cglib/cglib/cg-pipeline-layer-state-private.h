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
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __CG_PIPELINE_LAYER_STATE_PRIVATE_H
#define __CG_PIPELINE_LAYER_STATE_PRIVATE_H

#include "cg-pipeline-layer-state.h"
#include "cg-pipeline-private.h"

cg_pipeline_layer_t *_cg_pipeline_set_layer_unit(cg_pipeline_t *required_owner,
                                                 cg_pipeline_layer_t *layer,
                                                 int unit_index);

cg_pipeline_filter_t _cg_pipeline_get_layer_min_filter(cg_pipeline_t *pipeline,
                                                       int layer_index);

cg_pipeline_filter_t _cg_pipeline_get_layer_mag_filter(cg_pipeline_t *pipeline,
                                                       int layer_index);

bool _cg_pipeline_layer_texture_type_equal(cg_pipeline_layer_t *authority0,
                                           cg_pipeline_layer_t *authority1,
                                           cg_pipeline_eval_flags_t flags);

bool _cg_pipeline_layer_texture_data_equal(cg_pipeline_layer_t *authority0,
                                           cg_pipeline_layer_t *authority1,
                                           cg_pipeline_eval_flags_t flags);

bool _cg_pipeline_layer_sampler_equal(cg_pipeline_layer_t *authority0,
                                      cg_pipeline_layer_t *authority1);

bool
_cg_pipeline_layer_point_sprite_coords_equal(cg_pipeline_layer_t *authority0,
                                             cg_pipeline_layer_t *authority1);

bool _cg_pipeline_layer_vertex_snippets_equal(cg_pipeline_layer_t *authority0,
                                              cg_pipeline_layer_t *authority1);

bool
_cg_pipeline_layer_fragment_snippets_equal(cg_pipeline_layer_t *authority0,
                                           cg_pipeline_layer_t *authority1);

void _cg_pipeline_layer_hash_unit_state(cg_pipeline_layer_t *authority,
                                        cg_pipeline_layer_t **authorities,
                                        cg_pipeline_hash_state_t *state);

void
_cg_pipeline_layer_hash_texture_type_state(cg_pipeline_layer_t *authority,
                                           cg_pipeline_layer_t **authorities,
                                           cg_pipeline_hash_state_t *state);

void
_cg_pipeline_layer_hash_texture_data_state(cg_pipeline_layer_t *authority,
                                           cg_pipeline_layer_t **authorities,
                                           cg_pipeline_hash_state_t *state);

void _cg_pipeline_layer_hash_sampler_state(cg_pipeline_layer_t *authority,
                                           cg_pipeline_layer_t **authorities,
                                           cg_pipeline_hash_state_t *state);

void
_cg_pipeline_layer_hash_point_sprite_state(cg_pipeline_layer_t *authority,
                                           cg_pipeline_layer_t **authorities,
                                           cg_pipeline_hash_state_t *state);

void
_cg_pipeline_layer_hash_vertex_snippets_state(cg_pipeline_layer_t *authority,
                                              cg_pipeline_layer_t **authorities,
                                              cg_pipeline_hash_state_t *state);

void _cg_pipeline_layer_hash_fragment_snippets_state(
    cg_pipeline_layer_t *authority,
    cg_pipeline_layer_t **authorities,
    cg_pipeline_hash_state_t *state);

#endif /* __CG_PIPELINE_LAYER_STATE_PRIVATE_H */
