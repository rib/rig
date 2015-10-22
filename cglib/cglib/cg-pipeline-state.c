/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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

#include <cglib-config.h>

#include "cg-device-private.h"
#include "cg-color-private.h"
#include "cg-blend-string.h"
#include "cg-util.h"
#include "cg-depth-state-private.h"
#include "cg-pipeline-state-private.h"
#include "cg-snippet-private.h"
#include "cg-error-private.h"

#include <test-fixtures/test-cg-fixtures.h>

#include "string.h"

#ifndef GL_FUNC_ADD
#define GL_FUNC_ADD 0x8006
#endif

bool
_cg_pipeline_color_equal(cg_pipeline_t *authority0,
                         cg_pipeline_t *authority1)
{
    return cg_color_equal(&authority0->color, &authority1->color);
}

bool
_cg_pipeline_alpha_func_state_equal(cg_pipeline_t *authority0,
                                    cg_pipeline_t *authority1)
{
    cg_pipeline_alpha_func_state_t *alpha_state0 =
        &authority0->big_state->alpha_state;
    cg_pipeline_alpha_func_state_t *alpha_state1 =
        &authority1->big_state->alpha_state;

    return alpha_state0->alpha_func == alpha_state1->alpha_func;
}

bool
_cg_pipeline_alpha_func_reference_state_equal(cg_pipeline_t *authority0,
                                              cg_pipeline_t *authority1)
{
    cg_pipeline_alpha_func_state_t *alpha_state0 =
        &authority0->big_state->alpha_state;
    cg_pipeline_alpha_func_state_t *alpha_state1 =
        &authority1->big_state->alpha_state;

    return (alpha_state0->alpha_func_reference ==
            alpha_state1->alpha_func_reference);
}

bool
_cg_pipeline_blend_state_equal(cg_pipeline_t *authority0,
                               cg_pipeline_t *authority1)
{
    cg_pipeline_blend_state_t *blend_state0 =
        &authority0->big_state->blend_state;
    cg_pipeline_blend_state_t *blend_state1 =
        &authority1->big_state->blend_state;

    _CG_GET_DEVICE(dev, false);

    if (blend_state0->blend_equation_rgb != blend_state1->blend_equation_rgb)
        return false;

    if (blend_state0->blend_equation_alpha !=
        blend_state1->blend_equation_alpha)
        return false;
    if (blend_state0->blend_src_factor_alpha !=
        blend_state1->blend_src_factor_alpha)
        return false;
    if (blend_state0->blend_dst_factor_alpha !=
        blend_state1->blend_dst_factor_alpha)
        return false;

    if (blend_state0->blend_src_factor_rgb !=
        blend_state1->blend_src_factor_rgb)
        return false;
    if (blend_state0->blend_dst_factor_rgb !=
        blend_state1->blend_dst_factor_rgb)
        return false;

    if (blend_state0->blend_src_factor_rgb == GL_ONE_MINUS_CONSTANT_COLOR ||
        blend_state0->blend_src_factor_rgb == GL_CONSTANT_COLOR ||
        blend_state0->blend_dst_factor_rgb == GL_ONE_MINUS_CONSTANT_COLOR ||
        blend_state0->blend_dst_factor_rgb == GL_CONSTANT_COLOR) {
        if (!cg_color_equal(&blend_state0->blend_constant,
                            &blend_state1->blend_constant))
            return false;
    }

    return true;
}

bool
_cg_pipeline_depth_state_equal(cg_pipeline_t *authority0,
                               cg_pipeline_t *authority1)
{
    if (authority0->big_state->depth_state.test_enabled == false &&
        authority1->big_state->depth_state.test_enabled == false)
        return true;
    else {
        cg_depth_state_t *s0 = &authority0->big_state->depth_state;
        cg_depth_state_t *s1 = &authority1->big_state->depth_state;
        return s0->test_enabled == s1->test_enabled &&
               s0->test_function == s1->test_function &&
               s0->write_enabled == s1->write_enabled &&
               s0->range_near == s1->range_near &&
               s0->range_far == s1->range_far;
    }
}

bool
_cg_pipeline_non_zero_point_size_equal(cg_pipeline_t *authority0,
                                       cg_pipeline_t *authority1)
{
    return (authority0->big_state->non_zero_point_size ==
            authority1->big_state->non_zero_point_size);
}

bool
_cg_pipeline_point_size_equal(cg_pipeline_t *authority0,
                              cg_pipeline_t *authority1)
{
    return authority0->big_state->point_size ==
           authority1->big_state->point_size;
}

bool
_cg_pipeline_per_vertex_point_size_equal(cg_pipeline_t *authority0,
                                         cg_pipeline_t *authority1)
{
    return (authority0->big_state->per_vertex_point_size ==
            authority1->big_state->per_vertex_point_size);
}

bool
_cg_pipeline_logic_ops_state_equal(cg_pipeline_t *authority0,
                                   cg_pipeline_t *authority1)
{
    cg_pipeline_logic_ops_state_t *logic_ops_state0 =
        &authority0->big_state->logic_ops_state;
    cg_pipeline_logic_ops_state_t *logic_ops_state1 =
        &authority1->big_state->logic_ops_state;

    return logic_ops_state0->color_mask == logic_ops_state1->color_mask;
}

bool
_cg_pipeline_cull_face_state_equal(cg_pipeline_t *authority0,
                                   cg_pipeline_t *authority1)
{
    cg_pipeline_cull_face_state_t *cull_face_state0 =
        &authority0->big_state->cull_face_state;
    cg_pipeline_cull_face_state_t *cull_face_state1 =
        &authority1->big_state->cull_face_state;

    /* The cull face state is considered equal if two pipelines are both
       set to no culling. If the front winding property is ever used for
       anything else or the comparison is used not just for drawing then
       this would have to change */

    if (cull_face_state0->mode == CG_PIPELINE_CULL_FACE_MODE_NONE)
        return cull_face_state1->mode == CG_PIPELINE_CULL_FACE_MODE_NONE;

    return (cull_face_state0->mode == cull_face_state1->mode &&
            cull_face_state0->front_winding == cull_face_state1->front_winding);
}

typedef struct {
    const cg_boxed_value_t **dst_values;
    const cg_boxed_value_t *src_values;
    int override_count;
} get_uniforms_closure_t;

static bool
get_uniforms_cb(int uniform_num, void *user_data)
{
    get_uniforms_closure_t *data = user_data;

    if (data->dst_values[uniform_num] == NULL)
        data->dst_values[uniform_num] = data->src_values + data->override_count;

    data->override_count++;

    return true;
}

static void
_cg_pipeline_get_all_uniform_values(cg_pipeline_t *pipeline,
                                    const cg_boxed_value_t **values)
{
    get_uniforms_closure_t data;

    _CG_GET_DEVICE(dev, NO_RETVAL);

    memset(values, 0, sizeof(const cg_boxed_value_t *) * dev->n_uniform_names);

    data.dst_values = values;

    do {
        if ((pipeline->differences & CG_PIPELINE_STATE_UNIFORMS)) {
            const cg_pipeline_uniforms_state_t *uniforms_state =
                &pipeline->big_state->uniforms_state;

            data.override_count = 0;
            data.src_values = uniforms_state->override_values;

            _cg_bitmask_foreach(
                &uniforms_state->override_mask, get_uniforms_cb, &data);
        }
        pipeline = _cg_pipeline_get_parent(pipeline);
    } while (pipeline);
}

bool
_cg_pipeline_uniforms_state_equal(cg_pipeline_t *authority0,
                                  cg_pipeline_t *authority1)
{
    unsigned long *differences;
    const cg_boxed_value_t **values0, **values1;
    int n_longs;
    int i;

    _CG_GET_DEVICE(dev, false);

    if (authority0 == authority1)
        return true;

    values0 = c_alloca(sizeof(const cg_boxed_value_t *) * dev->n_uniform_names);
    values1 = c_alloca(sizeof(const cg_boxed_value_t *) * dev->n_uniform_names);

    n_longs = CG_FLAGS_N_LONGS_FOR_SIZE(dev->n_uniform_names);
    differences = c_alloca(n_longs * sizeof(unsigned long));
    memset(differences, 0, sizeof(unsigned long) * n_longs);
    _cg_pipeline_compare_uniform_differences(
        differences, authority0, authority1);

    _cg_pipeline_get_all_uniform_values(authority0, values0);
    _cg_pipeline_get_all_uniform_values(authority1, values1);

    CG_FLAGS_FOREACH_START(differences, n_longs, i)
    {
        const cg_boxed_value_t *value0 = values0[i];
        const cg_boxed_value_t *value1 = values1[i];

        if (value0 == NULL) {
            if (value1 != NULL && value1->type != CG_BOXED_NONE)
                return false;
        } else if (value1 == NULL) {
            if (value0 != NULL && value0->type != CG_BOXED_NONE)
                return false;
        } else if (!_cg_boxed_value_equal(value0, value1))
            return false;
    }
    CG_FLAGS_FOREACH_END;

    return true;
}

bool
_cg_pipeline_vertex_snippets_state_equal(cg_pipeline_t *authority0,
                                         cg_pipeline_t *authority1)
{
    return _cg_pipeline_snippet_list_equal(
        &authority0->big_state->vertex_snippets,
        &authority1->big_state->vertex_snippets);
}

bool
_cg_pipeline_fragment_snippets_state_equal(cg_pipeline_t *authority0,
                                           cg_pipeline_t *authority1)
{
    return _cg_pipeline_snippet_list_equal(
        &authority0->big_state->fragment_snippets,
        &authority1->big_state->fragment_snippets);
}

void
cg_pipeline_get_color(cg_pipeline_t *pipeline, cg_color_t *color)
{
    cg_pipeline_t *authority;

    c_return_if_fail(cg_is_pipeline(pipeline));

    authority = _cg_pipeline_get_authority(pipeline, CG_PIPELINE_STATE_COLOR);

    *color = authority->color;
}

void
cg_pipeline_set_color(cg_pipeline_t *pipeline, const cg_color_t *color)
{
    cg_pipeline_state_t state = CG_PIPELINE_STATE_COLOR;
    cg_pipeline_t *authority;

    c_return_if_fail(cg_is_pipeline(pipeline));

    authority = _cg_pipeline_get_authority(pipeline, state);

    if (cg_color_equal(color, &authority->color))
        return;

    /* - Make sure the pipeline has no dependants so it may be modified.
     * - If the pipeline isn't currently an authority for the state being
     *   changed, then initialize that state from the current authority.
     */
    _cg_pipeline_pre_change_notify(pipeline, state, color, false);

    pipeline->color = *color;

    _cg_pipeline_update_authority(
        pipeline, authority, state, _cg_pipeline_color_equal);

    pipeline->dirty_real_blend_enable = true;
}

void
cg_pipeline_set_color4ub(cg_pipeline_t *pipeline,
                         uint8_t red,
                         uint8_t green,
                         uint8_t blue,
                         uint8_t alpha)
{
    cg_color_t color;
    cg_color_init_from_4ub(&color, red, green, blue, alpha);
    cg_pipeline_set_color(pipeline, &color);
}

void
cg_pipeline_set_color4f(
    cg_pipeline_t *pipeline, float red, float green, float blue, float alpha)
{
    cg_color_t color;
    cg_color_init_from_4f(&color, red, green, blue, alpha);
    cg_pipeline_set_color(pipeline, &color);
}

cg_pipeline_blend_enable_t
_cg_pipeline_get_blend_enabled(cg_pipeline_t *pipeline)
{
    cg_pipeline_t *authority;

    c_return_val_if_fail(cg_is_pipeline(pipeline), false);

    authority =
        _cg_pipeline_get_authority(pipeline, CG_PIPELINE_STATE_BLEND_ENABLE);
    return authority->blend_enable;
}

static bool
_cg_pipeline_blend_enable_equal(cg_pipeline_t *authority0,
                                cg_pipeline_t *authority1)
{
    return authority0->blend_enable == authority1->blend_enable ? true : false;
}

void
_cg_pipeline_set_blend_enabled(cg_pipeline_t *pipeline,
                               cg_pipeline_blend_enable_t enable)
{
    cg_pipeline_state_t state = CG_PIPELINE_STATE_BLEND_ENABLE;
    cg_pipeline_t *authority;

    c_return_if_fail(cg_is_pipeline(pipeline));
    c_return_if_fail(enable > 1 &&
                       "don't pass true or false to _set_blend_enabled!");

    authority = _cg_pipeline_get_authority(pipeline, state);

    if (authority->blend_enable == enable)
        return;

    /* - Make sure the pipeline has no dependants so it may be modified.
     * - If the pipeline isn't currently an authority for the state being
     *   changed, then initialize that state from the current authority.
     */
    _cg_pipeline_pre_change_notify(pipeline, state, NULL, false);

    pipeline->blend_enable = enable;

    _cg_pipeline_update_authority(
        pipeline, authority, state, _cg_pipeline_blend_enable_equal);

    pipeline->dirty_real_blend_enable = true;
}

static void
_cg_pipeline_set_alpha_test_function(cg_pipeline_t *pipeline,
                                     cg_pipeline_alpha_func_t alpha_func)
{
    cg_pipeline_state_t state = CG_PIPELINE_STATE_ALPHA_FUNC;
    cg_pipeline_t *authority;
    cg_pipeline_alpha_func_state_t *alpha_state;

    c_return_if_fail(cg_is_pipeline(pipeline));

    authority = _cg_pipeline_get_authority(pipeline, state);

    alpha_state = &authority->big_state->alpha_state;
    if (alpha_state->alpha_func == alpha_func)
        return;

    /* - Make sure the pipeline has no dependants so it may be modified.
     * - If the pipeline isn't currently an authority for the state being
     *   changed, then initialize that state from the current authority.
     */
    _cg_pipeline_pre_change_notify(pipeline, state, NULL, false);

    alpha_state = &pipeline->big_state->alpha_state;
    alpha_state->alpha_func = alpha_func;

    _cg_pipeline_update_authority(
        pipeline, authority, state, _cg_pipeline_alpha_func_state_equal);
}

static void
_cg_pipeline_set_alpha_test_function_reference(cg_pipeline_t *pipeline,
                                               float alpha_reference)
{
    cg_pipeline_state_t state = CG_PIPELINE_STATE_ALPHA_FUNC_REFERENCE;
    cg_pipeline_t *authority;
    cg_pipeline_alpha_func_state_t *alpha_state;

    c_return_if_fail(cg_is_pipeline(pipeline));

    authority = _cg_pipeline_get_authority(pipeline, state);

    alpha_state = &authority->big_state->alpha_state;
    if (alpha_state->alpha_func_reference == alpha_reference)
        return;

    /* - Make sure the pipeline has no dependants so it may be modified.
     * - If the pipeline isn't currently an authority for the state being
     *   changed, then initialize that state from the current authority.
     */
    _cg_pipeline_pre_change_notify(pipeline, state, NULL, false);

    alpha_state = &pipeline->big_state->alpha_state;
    alpha_state->alpha_func_reference = alpha_reference;

    _cg_pipeline_update_authority(
        pipeline,
        authority,
        state,
        _cg_pipeline_alpha_func_reference_state_equal);
}

void
cg_pipeline_set_alpha_test_function(cg_pipeline_t *pipeline,
                                    cg_pipeline_alpha_func_t alpha_func,
                                    float alpha_reference)
{
    _cg_pipeline_set_alpha_test_function(pipeline, alpha_func);
    _cg_pipeline_set_alpha_test_function_reference(pipeline, alpha_reference);
}

cg_pipeline_alpha_func_t
cg_pipeline_get_alpha_test_function(cg_pipeline_t *pipeline)
{
    cg_pipeline_t *authority;

    c_return_val_if_fail(cg_is_pipeline(pipeline), 0);

    authority =
        _cg_pipeline_get_authority(pipeline, CG_PIPELINE_STATE_ALPHA_FUNC);

    return authority->big_state->alpha_state.alpha_func;
}

float
cg_pipeline_get_alpha_test_reference(cg_pipeline_t *pipeline)
{
    cg_pipeline_t *authority;

    c_return_val_if_fail(cg_is_pipeline(pipeline), 0.0f);

    authority = _cg_pipeline_get_authority(
        pipeline, CG_PIPELINE_STATE_ALPHA_FUNC_REFERENCE);

    return authority->big_state->alpha_state.alpha_func_reference;
}

static GLenum
arg_to_gl_blend_factor(cg_blend_string_argument_t *arg)
{
    if (arg->source.is_zero)
        return GL_ZERO;
    if (arg->factor.is_one)
        return GL_ONE;
    else if (arg->factor.is_src_alpha_saturate)
        return GL_SRC_ALPHA_SATURATE;
    else if (arg->factor.source.info->type ==
             CG_BLEND_STRING_COLOR_SOURCE_SRC_COLOR) {
        if (arg->factor.source.mask != CG_BLEND_STRING_CHANNEL_MASK_ALPHA) {
            if (arg->factor.source.one_minus)
                return GL_ONE_MINUS_SRC_COLOR;
            else
                return GL_SRC_COLOR;
        } else {
            if (arg->factor.source.one_minus)
                return GL_ONE_MINUS_SRC_ALPHA;
            else
                return GL_SRC_ALPHA;
        }
    } else if (arg->factor.source.info->type ==
               CG_BLEND_STRING_COLOR_SOURCE_DST_COLOR) {
        if (arg->factor.source.mask != CG_BLEND_STRING_CHANNEL_MASK_ALPHA) {
            if (arg->factor.source.one_minus)
                return GL_ONE_MINUS_DST_COLOR;
            else
                return GL_DST_COLOR;
        } else {
            if (arg->factor.source.one_minus)
                return GL_ONE_MINUS_DST_ALPHA;
            else
                return GL_DST_ALPHA;
        }
    }
#if defined(CG_HAS_GLES2_SUPPORT) || defined(CG_HAS_GL_SUPPORT)
    else if (arg->factor.source.info->type ==
             CG_BLEND_STRING_COLOR_SOURCE_CONSTANT) {
        if (arg->factor.source.mask != CG_BLEND_STRING_CHANNEL_MASK_ALPHA) {
            if (arg->factor.source.one_minus)
                return GL_ONE_MINUS_CONSTANT_COLOR;
            else
                return GL_CONSTANT_COLOR;
        } else {
            if (arg->factor.source.one_minus)
                return GL_ONE_MINUS_CONSTANT_ALPHA;
            else
                return GL_CONSTANT_ALPHA;
        }
    }
#endif

    c_warning("Unable to determine valid blend factor from blend string\n");
    return GL_ONE;
}

static void
setup_blend_state(cg_blend_string_statement_t *statement,
                  GLenum *blend_equation,
                  GLint *blend_src_factor,
                  GLint *blend_dst_factor)
{
    switch (statement->function->type) {
    case CG_BLEND_STRING_FUNCTION_ADD:
        *blend_equation = GL_FUNC_ADD;
        break;
    /* TODO - add more */
    default:
        c_warning("Unsupported blend function given");
        *blend_equation = GL_FUNC_ADD;
    }

    *blend_src_factor = arg_to_gl_blend_factor(&statement->args[0]);
    *blend_dst_factor = arg_to_gl_blend_factor(&statement->args[1]);
}

bool
cg_pipeline_set_blend(cg_pipeline_t *pipeline,
                      const char *blend_description,
                      cg_error_t **error)
{
    cg_pipeline_state_t state = CG_PIPELINE_STATE_BLEND;
    cg_pipeline_t *authority;
    cg_blend_string_statement_t statements[2];
    cg_blend_string_statement_t *rgb;
    cg_blend_string_statement_t *a;
    int count;
    cg_pipeline_blend_state_t *blend_state;

    _CG_GET_DEVICE(dev, false);

    c_return_val_if_fail(cg_is_pipeline(pipeline), false);

    count = _cg_blend_string_compile(dev,
                                     blend_description,
                                     statements,
                                     error);
    if (!count)
        return false;

    if (count == 1)
        rgb = a = statements;
    else {
        rgb = &statements[0];
        a = &statements[1];
    }

    authority = _cg_pipeline_get_authority(pipeline, state);

    /* - Make sure the pipeline has no dependants so it may be modified.
     * - If the pipeline isn't currently an authority for the state being
     *   changed, then initialize that state from the current authority.
     */
    _cg_pipeline_pre_change_notify(pipeline, state, NULL, false);

    blend_state = &pipeline->big_state->blend_state;

    setup_blend_state(rgb,
                      &blend_state->blend_equation_rgb,
                      &blend_state->blend_src_factor_rgb,
                      &blend_state->blend_dst_factor_rgb);
    setup_blend_state(a,
                      &blend_state->blend_equation_alpha,
                      &blend_state->blend_src_factor_alpha,
                      &blend_state->blend_dst_factor_alpha);

    /* If we are the current authority see if we can revert to one of our
     * ancestors being the authority */
    if (pipeline == authority && _cg_pipeline_get_parent(authority) != NULL) {
        cg_pipeline_t *parent = _cg_pipeline_get_parent(authority);
        cg_pipeline_t *old_authority =
            _cg_pipeline_get_authority(parent, state);

        if (_cg_pipeline_blend_state_equal(authority, old_authority))
            pipeline->differences &= ~state;
    }

    /* If we weren't previously the authority on this state then we need
     * to extended our differences mask and so it's possible that some
     * of our ancestry will now become redundant, so we aim to reparent
     * ourselves if that's true... */
    if (pipeline != authority) {
        pipeline->differences |= state;
        _cg_pipeline_prune_redundant_ancestry(pipeline);
    }

    pipeline->dirty_real_blend_enable = true;

    return true;
}

void
cg_pipeline_set_blend_constant(cg_pipeline_t *pipeline,
                               const cg_color_t *constant_color)
{
    cg_pipeline_state_t state = CG_PIPELINE_STATE_BLEND;
    cg_pipeline_t *authority;
    cg_pipeline_blend_state_t *blend_state;

    _CG_GET_DEVICE(dev, NO_RETVAL);

    c_return_if_fail(cg_is_pipeline(pipeline));

    if (!_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_BLEND_CONSTANT))
        return;

    authority = _cg_pipeline_get_authority(pipeline, state);

    blend_state = &authority->big_state->blend_state;
    if (cg_color_equal(constant_color, &blend_state->blend_constant))
        return;

    /* - Make sure the pipeline has no dependants so it may be modified.
     * - If the pipeline isn't currently an authority for the state being
     *   changed, then initialize that state from the current authority.
     */
    _cg_pipeline_pre_change_notify(pipeline, state, NULL, false);

    blend_state = &pipeline->big_state->blend_state;
    blend_state->blend_constant = *constant_color;

    _cg_pipeline_update_authority(
        pipeline, authority, state, _cg_pipeline_blend_state_equal);

    pipeline->dirty_real_blend_enable = true;
}

bool
cg_pipeline_set_depth_state(cg_pipeline_t *pipeline,
                            const cg_depth_state_t *depth_state,
                            cg_error_t **error)
{
    cg_pipeline_state_t state = CG_PIPELINE_STATE_DEPTH;
    cg_pipeline_t *authority;
    cg_depth_state_t *orig_state;

    _CG_GET_DEVICE(dev, false);

    c_return_val_if_fail(cg_is_pipeline(pipeline), false);
    c_return_val_if_fail(depth_state->magic == CG_DEPTH_STATE_MAGIC, false);

    authority = _cg_pipeline_get_authority(pipeline, state);

    orig_state = &authority->big_state->depth_state;
    if (orig_state->test_enabled == depth_state->test_enabled &&
        orig_state->write_enabled == depth_state->write_enabled &&
        orig_state->test_function == depth_state->test_function &&
        orig_state->range_near == depth_state->range_near &&
        orig_state->range_far == depth_state->range_far)
        return true;

    /* - Make sure the pipeline has no dependants so it may be modified.
     * - If the pipeline isn't currently an authority for the state being
     *   changed, then initialize that state from the current authority.
     */
    _cg_pipeline_pre_change_notify(pipeline, state, NULL, false);

    pipeline->big_state->depth_state = *depth_state;

    _cg_pipeline_update_authority(
        pipeline, authority, state, _cg_pipeline_depth_state_equal);

    return true;
}

void
cg_pipeline_get_depth_state(cg_pipeline_t *pipeline,
                            cg_depth_state_t *state)
{
    cg_pipeline_t *authority;

    c_return_if_fail(cg_is_pipeline(pipeline));

    authority = _cg_pipeline_get_authority(pipeline, CG_PIPELINE_STATE_DEPTH);
    *state = authority->big_state->depth_state;
}

cg_color_mask_t
cg_pipeline_get_color_mask(cg_pipeline_t *pipeline)
{
    cg_pipeline_t *authority;

    c_return_val_if_fail(cg_is_pipeline(pipeline), 0);

    authority =
        _cg_pipeline_get_authority(pipeline, CG_PIPELINE_STATE_LOGIC_OPS);

    return authority->big_state->logic_ops_state.color_mask;
}

void
cg_pipeline_set_color_mask(cg_pipeline_t *pipeline,
                           cg_color_mask_t color_mask)
{
    cg_pipeline_state_t state = CG_PIPELINE_STATE_LOGIC_OPS;
    cg_pipeline_t *authority;
    cg_pipeline_logic_ops_state_t *logic_ops_state;

    c_return_if_fail(cg_is_pipeline(pipeline));

    authority = _cg_pipeline_get_authority(pipeline, state);

    logic_ops_state = &authority->big_state->logic_ops_state;
    if (logic_ops_state->color_mask == color_mask)
        return;

    /* - Make sure the pipeline has no dependants so it may be modified.
     * - If the pipeline isn't currently an authority for the state being
     *   changed, then initialize that state from the current authority.
     */
    _cg_pipeline_pre_change_notify(pipeline, state, NULL, false);

    logic_ops_state = &pipeline->big_state->logic_ops_state;
    logic_ops_state->color_mask = color_mask;

    _cg_pipeline_update_authority(
        pipeline, authority, state, _cg_pipeline_logic_ops_state_equal);
}

void
cg_pipeline_set_cull_face_mode(cg_pipeline_t *pipeline,
                               cg_pipeline_cull_face_mode_t cull_face_mode)
{
    cg_pipeline_state_t state = CG_PIPELINE_STATE_CULL_FACE;
    cg_pipeline_t *authority;
    cg_pipeline_cull_face_state_t *cull_face_state;

    c_return_if_fail(cg_is_pipeline(pipeline));

    authority = _cg_pipeline_get_authority(pipeline, state);

    cull_face_state = &authority->big_state->cull_face_state;

    if (cull_face_state->mode == cull_face_mode)
        return;

    /* - Make sure the pipeline has no dependants so it may be modified.
     * - If the pipeline isn't currently an authority for the state being
     *   changed, then initialize that state from the current authority.
     */
    _cg_pipeline_pre_change_notify(pipeline, state, NULL, false);

    pipeline->big_state->cull_face_state.mode = cull_face_mode;

    _cg_pipeline_update_authority(
        pipeline, authority, state, _cg_pipeline_cull_face_state_equal);
}

void
cg_pipeline_set_front_face_winding(cg_pipeline_t *pipeline,
                                   cg_winding_t front_winding)
{
    cg_pipeline_state_t state = CG_PIPELINE_STATE_CULL_FACE;
    cg_pipeline_t *authority;
    cg_pipeline_cull_face_state_t *cull_face_state;

    c_return_if_fail(cg_is_pipeline(pipeline));

    authority = _cg_pipeline_get_authority(pipeline, state);

    cull_face_state = &authority->big_state->cull_face_state;

    if (cull_face_state->front_winding == front_winding)
        return;

    /* - Make sure the pipeline has no dependants so it may be modified.
     * - If the pipeline isn't currently an authority for the state being
     *   changed, then initialize that state from the current authority.
     */
    _cg_pipeline_pre_change_notify(pipeline, state, NULL, false);

    pipeline->big_state->cull_face_state.front_winding = front_winding;

    _cg_pipeline_update_authority(
        pipeline, authority, state, _cg_pipeline_cull_face_state_equal);
}

cg_pipeline_cull_face_mode_t
cg_pipeline_get_cull_face_mode(cg_pipeline_t *pipeline)
{
    cg_pipeline_state_t state = CG_PIPELINE_STATE_CULL_FACE;
    cg_pipeline_t *authority;

    c_return_val_if_fail(cg_is_pipeline(pipeline),
                           CG_PIPELINE_CULL_FACE_MODE_NONE);

    authority = _cg_pipeline_get_authority(pipeline, state);

    return authority->big_state->cull_face_state.mode;
}

cg_winding_t
cg_pipeline_get_front_face_winding(cg_pipeline_t *pipeline)
{
    cg_pipeline_state_t state = CG_PIPELINE_STATE_CULL_FACE;
    cg_pipeline_t *authority;

    c_return_val_if_fail(cg_is_pipeline(pipeline),
                           CG_PIPELINE_CULL_FACE_MODE_NONE);

    authority = _cg_pipeline_get_authority(pipeline, state);

    return authority->big_state->cull_face_state.front_winding;
}

float
cg_pipeline_get_point_size(cg_pipeline_t *pipeline)
{
    cg_pipeline_t *authority;

    c_return_val_if_fail(cg_is_pipeline(pipeline), false);

    authority =
        _cg_pipeline_get_authority(pipeline, CG_PIPELINE_STATE_POINT_SIZE);

    return authority->big_state->point_size;
}

static void
_cg_pipeline_set_non_zero_point_size(cg_pipeline_t *pipeline,
                                     bool value)
{
    cg_pipeline_state_t state = CG_PIPELINE_STATE_NON_ZERO_POINT_SIZE;
    cg_pipeline_t *authority;

    c_return_if_fail(cg_is_pipeline(pipeline));

    authority = _cg_pipeline_get_authority(pipeline, state);

    /* - Make sure the pipeline has no dependants so it may be modified.
     * - If the pipeline isn't currently an authority for the state being
     *   changed, then initialize that state from the current authority.
     */
    _cg_pipeline_pre_change_notify(pipeline, state, NULL, false);

    pipeline->big_state->non_zero_point_size = !!value;

    _cg_pipeline_update_authority(
        pipeline, authority, state, _cg_pipeline_non_zero_point_size_equal);
}

void
cg_pipeline_set_point_size(cg_pipeline_t *pipeline, float point_size)
{
    cg_pipeline_state_t state = CG_PIPELINE_STATE_POINT_SIZE;
    cg_pipeline_t *authority;

    c_return_if_fail(cg_is_pipeline(pipeline));

    authority = _cg_pipeline_get_authority(pipeline, state);

    if (authority->big_state->point_size == point_size)
        return;

    /* Changing the point size may additionally modify
     * CG_PIPELINE_STATE_NON_ZERO_POINT_SIZE. */

    if ((authority->big_state->point_size > 0.0f) != (point_size > 0.0f))
        _cg_pipeline_set_non_zero_point_size(pipeline, point_size > 0.0f);

    /* - Make sure the pipeline has no dependants so it may be modified.
     * - If the pipeline isn't currently an authority for the state being
     *   changed, then initialize that state from the current authority.
     */
    _cg_pipeline_pre_change_notify(pipeline, state, NULL, false);

    pipeline->big_state->point_size = point_size;

    _cg_pipeline_update_authority(
        pipeline, authority, state, _cg_pipeline_point_size_equal);
}

bool
cg_pipeline_set_per_vertex_point_size(cg_pipeline_t *pipeline,
                                      bool enable,
                                      cg_error_t **error)
{
    cg_pipeline_state_t state = CG_PIPELINE_STATE_PER_VERTEX_POINT_SIZE;
    cg_pipeline_t *authority;

    _CG_GET_DEVICE(dev, false);
    c_return_val_if_fail(cg_is_pipeline(pipeline), false);

    authority = _cg_pipeline_get_authority(pipeline, state);

    enable = !!enable;

    if (authority->big_state->per_vertex_point_size == enable)
        return true;

    if (enable && !cg_has_feature(dev, CG_FEATURE_ID_PER_VERTEX_POINT_SIZE)) {
        _cg_set_error(error,
                      CG_SYSTEM_ERROR,
                      CG_SYSTEM_ERROR_UNSUPPORTED,
                      "Per-vertex point size is not supported");

        return false;
    }

    /* - Make sure the pipeline has no dependants so it may be modified.
     * - If the pipeline isn't currently an authority for the state being
     *   changed, then initialize that state from the current authority.
     */
    _cg_pipeline_pre_change_notify(pipeline, state, NULL, false);

    pipeline->big_state->per_vertex_point_size = enable;

    _cg_pipeline_update_authority(
        pipeline, authority, state, _cg_pipeline_point_size_equal);

    return true;
}

bool
cg_pipeline_get_per_vertex_point_size(cg_pipeline_t *pipeline)
{
    cg_pipeline_t *authority;

    c_return_val_if_fail(cg_is_pipeline(pipeline), false);

    authority = _cg_pipeline_get_authority(
        pipeline, CG_PIPELINE_STATE_PER_VERTEX_POINT_SIZE);

    return authority->big_state->per_vertex_point_size;
}

static cg_boxed_value_t *
_cg_pipeline_override_uniform(cg_pipeline_t *pipeline,
                              int location)
{
    cg_pipeline_state_t state = CG_PIPELINE_STATE_UNIFORMS;
    cg_pipeline_uniforms_state_t *uniforms_state;
    int override_index;

    _CG_GET_DEVICE(dev, NULL);

    c_return_val_if_fail(cg_is_pipeline(pipeline), NULL);
    c_return_val_if_fail(location >= 0, NULL);
    c_return_val_if_fail(location < dev->n_uniform_names, NULL);

    /* - Make sure the pipeline has no dependants so it may be modified.
     * - If the pipeline isn't currently an authority for the state being
     *   changed, then initialize that state from the current authority.
     */
    _cg_pipeline_pre_change_notify(pipeline, state, NULL, false);

    uniforms_state = &pipeline->big_state->uniforms_state;

    /* Count the number of bits that are set below this location. That
       should give us the position where our new value should lie */
    override_index =
        _cg_bitmask_popcount_upto(&uniforms_state->override_mask, location);

    _cg_bitmask_set(&uniforms_state->changed_mask, location, true);

    /* If this pipeline already has an override for this value then we
       can just use it directly */
    if (_cg_bitmask_get(&uniforms_state->override_mask, location))
        return uniforms_state->override_values + override_index;

    /* We need to create a new override value in the right position
       within the array. This is pretty inefficient but the hope is that
       it will be much more common to modify an existing uniform rather
       than modify a new one so it is more important to optimise the
       former case. */

    if (uniforms_state->override_values == NULL) {
        c_assert(override_index == 0);
        uniforms_state->override_values = c_new(cg_boxed_value_t, 1);
    } else {
        /* We need to grow the array and copy in the old values */
        cg_boxed_value_t *old_values = uniforms_state->override_values;
        int old_size = _cg_bitmask_popcount(&uniforms_state->override_mask);

        uniforms_state->override_values = c_new(cg_boxed_value_t, old_size + 1);

        /* Copy in the old values leaving a gap for the new value */
        memcpy(uniforms_state->override_values,
               old_values,
               sizeof(cg_boxed_value_t) * override_index);
        memcpy(uniforms_state->override_values + override_index + 1,
               old_values + override_index,
               sizeof(cg_boxed_value_t) * (old_size - override_index));

        c_free(old_values);
    }

    _cg_boxed_value_init(uniforms_state->override_values + override_index);

    _cg_bitmask_set(&uniforms_state->override_mask, location, true);

    return uniforms_state->override_values + override_index;
}

void
cg_pipeline_set_uniform_1f(cg_pipeline_t *pipeline,
                           int uniform_location,
                           float value)
{
    cg_boxed_value_t *boxed_value;

    boxed_value = _cg_pipeline_override_uniform(pipeline, uniform_location);

    _cg_boxed_value_set_1f(boxed_value, value);
}

void
cg_pipeline_set_uniform_1i(cg_pipeline_t *pipeline,
                           int uniform_location,
                           int value)
{
    cg_boxed_value_t *boxed_value;

    boxed_value = _cg_pipeline_override_uniform(pipeline, uniform_location);

    _cg_boxed_value_set_1i(boxed_value, value);
}

void
cg_pipeline_set_uniform_float(cg_pipeline_t *pipeline,
                              int uniform_location,
                              int n_components,
                              int count,
                              const float *value)
{
    cg_boxed_value_t *boxed_value;

    boxed_value = _cg_pipeline_override_uniform(pipeline, uniform_location);

    _cg_boxed_value_set_float(boxed_value, n_components, count, value);
}

void
cg_pipeline_set_uniform_int(cg_pipeline_t *pipeline,
                            int uniform_location,
                            int n_components,
                            int count,
                            const int *value)
{
    cg_boxed_value_t *boxed_value;

    boxed_value = _cg_pipeline_override_uniform(pipeline, uniform_location);

    _cg_boxed_value_set_int(boxed_value, n_components, count, value);
}

void
cg_pipeline_set_uniform_matrix(cg_pipeline_t *pipeline,
                               int uniform_location,
                               int dimensions,
                               int count,
                               bool transpose,
                               const float *value)
{
    cg_boxed_value_t *boxed_value;

    boxed_value = _cg_pipeline_override_uniform(pipeline, uniform_location);

    _cg_boxed_value_set_matrix(
        boxed_value, dimensions, count, transpose, value);
}

static void
_cg_pipeline_add_vertex_snippet(cg_pipeline_t *pipeline,
                                cg_snippet_t *snippet)
{
    cg_pipeline_state_t state = CG_PIPELINE_STATE_VERTEX_SNIPPETS;

    /* - Make sure the pipeline has no dependants so it may be modified.
     * - If the pipeline isn't currently an authority for the state being
     *   changed, then initialize that state from the current authority.
     */
    _cg_pipeline_pre_change_notify(pipeline, state, NULL, false);

    _cg_pipeline_snippet_list_add(&pipeline->big_state->vertex_snippets,
                                  snippet);
}

static void
_cg_pipeline_add_fragment_snippet(cg_pipeline_t *pipeline,
                                  cg_snippet_t *snippet)
{
    cg_pipeline_state_t state = CG_PIPELINE_STATE_FRAGMENT_SNIPPETS;

    /* - Make sure the pipeline has no dependants so it may be modified.
     * - If the pipeline isn't currently an authority for the state being
     *   changed, then initialize that state from the current authority.
     */
    _cg_pipeline_pre_change_notify(pipeline, state, NULL, false);

    _cg_pipeline_snippet_list_add(&pipeline->big_state->fragment_snippets,
                                  snippet);
}

void
cg_pipeline_add_snippet(cg_pipeline_t *pipeline, cg_snippet_t *snippet)
{
    c_return_if_fail(cg_is_pipeline(pipeline));
    c_return_if_fail(cg_is_snippet(snippet));
    c_return_if_fail(snippet->hook < CG_SNIPPET_FIRST_LAYER_HOOK);

    if (snippet->hook < CG_SNIPPET_FIRST_PIPELINE_FRAGMENT_HOOK)
        _cg_pipeline_add_vertex_snippet(pipeline, snippet);
    else
        _cg_pipeline_add_fragment_snippet(pipeline, snippet);
}

bool
_cg_pipeline_has_non_layer_vertex_snippets(cg_pipeline_t *pipeline)
{
    cg_pipeline_t *authority =
        _cg_pipeline_get_authority(pipeline, CG_PIPELINE_STATE_VERTEX_SNIPPETS);

    return authority->big_state->vertex_snippets.entries != NULL;
}

static bool
check_layer_has_vertex_snippet(cg_pipeline_layer_t *layer,
                               void *user_data)
{
    unsigned long state = CG_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS;
    cg_pipeline_layer_t *authority =
        _cg_pipeline_layer_get_authority(layer, state);
    bool *found_vertex_snippet = user_data;

    if (authority->big_state->vertex_snippets.entries) {
        *found_vertex_snippet = true;
        return false;
    }

    return true;
}

bool
_cg_pipeline_has_vertex_snippets(cg_pipeline_t *pipeline)
{
    bool found_vertex_snippet = false;

    if (_cg_pipeline_has_non_layer_vertex_snippets(pipeline))
        return true;

    _cg_pipeline_foreach_layer_internal(
        pipeline, check_layer_has_vertex_snippet, &found_vertex_snippet);

    return found_vertex_snippet;
}

bool
_cg_pipeline_has_non_layer_fragment_snippets(cg_pipeline_t *pipeline)
{
    cg_pipeline_t *authority = _cg_pipeline_get_authority(
        pipeline, CG_PIPELINE_STATE_FRAGMENT_SNIPPETS);

    return authority->big_state->fragment_snippets.entries != NULL;
}

static bool
check_layer_has_fragment_snippet(cg_pipeline_layer_t *layer,
                                 void *user_data)
{
    unsigned long state = CG_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS;
    cg_pipeline_layer_t *authority =
        _cg_pipeline_layer_get_authority(layer, state);
    bool *found_fragment_snippet = user_data;

    if (authority->big_state->fragment_snippets.entries) {
        *found_fragment_snippet = true;
        return false;
    }

    return true;
}

bool
_cg_pipeline_has_fragment_snippets(cg_pipeline_t *pipeline)
{
    bool found_fragment_snippet = false;

    if (_cg_pipeline_has_non_layer_fragment_snippets(pipeline))
        return true;

    _cg_pipeline_foreach_layer_internal(
        pipeline, check_layer_has_fragment_snippet, &found_fragment_snippet);

    return found_fragment_snippet;
}

void
_cg_pipeline_hash_color_state(cg_pipeline_t *authority,
                              cg_pipeline_hash_state_t *state)
{
    state->hash = _cg_util_one_at_a_time_hash(
        state->hash, &authority->color, _CG_COLOR_DATA_SIZE);
}

void
_cg_pipeline_hash_blend_enable_state(cg_pipeline_t *authority,
                                     cg_pipeline_hash_state_t *state)
{
    uint8_t blend_enable = authority->blend_enable;
    state->hash = _cg_util_one_at_a_time_hash(state->hash, &blend_enable, 1);
}

void
_cg_pipeline_hash_alpha_func_state(cg_pipeline_t *authority,
                                   cg_pipeline_hash_state_t *state)
{
    cg_pipeline_alpha_func_state_t *alpha_state =
        &authority->big_state->alpha_state;
    state->hash = _cg_util_one_at_a_time_hash(
        state->hash, &alpha_state->alpha_func, sizeof(alpha_state->alpha_func));
}

void
_cg_pipeline_hash_alpha_func_reference_state(cg_pipeline_t *authority,
                                             cg_pipeline_hash_state_t *state)
{
    cg_pipeline_alpha_func_state_t *alpha_state =
        &authority->big_state->alpha_state;
    float ref = alpha_state->alpha_func_reference;
    state->hash = _cg_util_one_at_a_time_hash(state->hash, &ref, sizeof(float));
}

void
_cg_pipeline_hash_blend_state(cg_pipeline_t *authority,
                              cg_pipeline_hash_state_t *state)
{
    cg_pipeline_blend_state_t *blend_state = &authority->big_state->blend_state;
    unsigned int hash;

    _CG_GET_DEVICE(dev, NO_RETVAL);

    if (!authority->real_blend_enable)
        return;

    hash = state->hash;

    hash = _cg_util_one_at_a_time_hash(hash,
                                       &blend_state->blend_equation_rgb,
                                       sizeof(blend_state->blend_equation_rgb));
    hash =
        _cg_util_one_at_a_time_hash(hash,
                                    &blend_state->blend_equation_alpha,
                                    sizeof(blend_state->blend_equation_alpha));
    hash = _cg_util_one_at_a_time_hash(
        hash,
        &blend_state->blend_src_factor_alpha,
        sizeof(blend_state->blend_src_factor_alpha));
    hash = _cg_util_one_at_a_time_hash(
        hash,
        &blend_state->blend_dst_factor_alpha,
        sizeof(blend_state->blend_dst_factor_alpha));

    if (blend_state->blend_src_factor_rgb == GL_ONE_MINUS_CONSTANT_COLOR ||
        blend_state->blend_src_factor_rgb == GL_CONSTANT_COLOR ||
        blend_state->blend_dst_factor_rgb == GL_ONE_MINUS_CONSTANT_COLOR ||
        blend_state->blend_dst_factor_rgb == GL_CONSTANT_COLOR) {
        hash = _cg_util_one_at_a_time_hash(hash,
                                           &blend_state->blend_constant,
                                           sizeof(blend_state->blend_constant));
    }

    hash =
        _cg_util_one_at_a_time_hash(hash,
                                    &blend_state->blend_src_factor_rgb,
                                    sizeof(blend_state->blend_src_factor_rgb));
    hash =
        _cg_util_one_at_a_time_hash(hash,
                                    &blend_state->blend_dst_factor_rgb,
                                    sizeof(blend_state->blend_dst_factor_rgb));

    state->hash = hash;
}

void
_cg_pipeline_hash_depth_state(cg_pipeline_t *authority,
                              cg_pipeline_hash_state_t *state)
{
    cg_depth_state_t *depth_state = &authority->big_state->depth_state;
    unsigned int hash = state->hash;

    if (depth_state->test_enabled) {
        uint8_t enabled = depth_state->test_enabled;
        cg_depth_test_function_t function = depth_state->test_function;
        hash = _cg_util_one_at_a_time_hash(hash, &enabled, sizeof(enabled));
        hash = _cg_util_one_at_a_time_hash(hash, &function, sizeof(function));
    }

    if (depth_state->write_enabled) {
        uint8_t enabled = depth_state->write_enabled;
        float near_val = depth_state->range_near;
        float far_val = depth_state->range_far;
        hash = _cg_util_one_at_a_time_hash(hash, &enabled, sizeof(enabled));
        hash = _cg_util_one_at_a_time_hash(hash, &near_val, sizeof(near_val));
        hash = _cg_util_one_at_a_time_hash(hash, &far_val, sizeof(far_val));
    }

    state->hash = hash;
}

void
_cg_pipeline_hash_non_zero_point_size_state(cg_pipeline_t *authority,
                                            cg_pipeline_hash_state_t *state)
{
    bool non_zero_point_size = authority->big_state->non_zero_point_size;

    state->hash = _cg_util_one_at_a_time_hash(
        state->hash, &non_zero_point_size, sizeof(non_zero_point_size));
}

void
_cg_pipeline_hash_point_size_state(cg_pipeline_t *authority,
                                   cg_pipeline_hash_state_t *state)
{
    float point_size = authority->big_state->point_size;
    state->hash = _cg_util_one_at_a_time_hash(
        state->hash, &point_size, sizeof(point_size));
}

void
_cg_pipeline_hash_per_vertex_point_size_state(cg_pipeline_t *authority,
                                              cg_pipeline_hash_state_t *state)
{
    bool per_vertex_point_size = authority->big_state->per_vertex_point_size;
    state->hash = _cg_util_one_at_a_time_hash(
        state->hash, &per_vertex_point_size, sizeof(per_vertex_point_size));
}

void
_cg_pipeline_hash_logic_ops_state(cg_pipeline_t *authority,
                                  cg_pipeline_hash_state_t *state)
{
    cg_pipeline_logic_ops_state_t *logic_ops_state =
        &authority->big_state->logic_ops_state;
    state->hash = _cg_util_one_at_a_time_hash(
        state->hash, &logic_ops_state->color_mask, sizeof(cg_color_mask_t));
}

void
_cg_pipeline_hash_cull_face_state(cg_pipeline_t *authority,
                                  cg_pipeline_hash_state_t *state)
{
    cg_pipeline_cull_face_state_t *cull_face_state =
        &authority->big_state->cull_face_state;

    /* The cull face state is considered equal if two pipelines are both
       set to no culling. If the front winding property is ever used for
       anything else or the hashing is used not just for drawing then
       this would have to change */
    if (cull_face_state->mode == CG_PIPELINE_CULL_FACE_MODE_NONE)
        state->hash =
            _cg_util_one_at_a_time_hash(state->hash,
                                        &cull_face_state->mode,
                                        sizeof(cg_pipeline_cull_face_mode_t));
    else
        state->hash =
            _cg_util_one_at_a_time_hash(state->hash,
                                        cull_face_state,
                                        sizeof(cg_pipeline_cull_face_state_t));
}

void
_cg_pipeline_hash_uniforms_state(cg_pipeline_t *authority,
                                 cg_pipeline_hash_state_t *state)
{
    /* This isn't used anywhere yet because the uniform state doesn't
       affect program generation. It's quite a hassle to implement so
       let's just leave it until something actually needs it */
    c_warn_if_reached();
}

void
_cg_pipeline_compare_uniform_differences(unsigned long *differences,
                                         cg_pipeline_t *pipeline0,
                                         cg_pipeline_t *pipeline1)
{
    c_sllist_t *head0 = NULL;
    c_sllist_t *head1 = NULL;
    cg_pipeline_t *node0;
    cg_pipeline_t *node1;
    int len0 = 0;
    int len1 = 0;
    int count;
    c_sllist_t *common_ancestor0;
    c_sllist_t *common_ancestor1;

    /* This algorithm is copied from
       _cg_pipeline_compare_differences(). It might be nice to share
       the code more */

    for (node0 = pipeline0; node0; node0 = _cg_pipeline_get_parent(node0)) {
        c_sllist_t *link = alloca(sizeof(c_sllist_t));
        link->next = head0;
        link->data = node0;
        head0 = link;
        len0++;
    }
    for (node1 = pipeline1; node1; node1 = _cg_pipeline_get_parent(node1)) {
        c_sllist_t *link = alloca(sizeof(c_sllist_t));
        link->next = head1;
        link->data = node1;
        head1 = link;
        len1++;
    }

    /* NB: There's no point looking at the head entries since we know both
    * pipelines must have the same default pipeline as their root node. */
    common_ancestor0 = head0;
    common_ancestor1 = head1;
    head0 = head0->next;
    head1 = head1->next;
    count = MIN(len0, len1) - 1;
    while (count--) {
        if (head0->data != head1->data)
            break;
        common_ancestor0 = head0;
        common_ancestor1 = head1;
        head0 = head0->next;
        head1 = head1->next;
    }

    for (head0 = common_ancestor0->next; head0; head0 = head0->next) {
        node0 = head0->data;
        if ((node0->differences & CG_PIPELINE_STATE_UNIFORMS)) {
            const cg_pipeline_uniforms_state_t *uniforms_state =
                &node0->big_state->uniforms_state;
            _cg_bitmask_set_flags(&uniforms_state->override_mask, differences);
        }
    }
    for (head1 = common_ancestor1->next; head1; head1 = head1->next) {
        node1 = head1->data;
        if ((node1->differences & CG_PIPELINE_STATE_UNIFORMS)) {
            const cg_pipeline_uniforms_state_t *uniforms_state =
                &node1->big_state->uniforms_state;
            _cg_bitmask_set_flags(&uniforms_state->override_mask, differences);
        }
    }
}

void
_cg_pipeline_hash_vertex_snippets_state(cg_pipeline_t *authority,
                                        cg_pipeline_hash_state_t *state)
{
    _cg_pipeline_snippet_list_hash(&authority->big_state->vertex_snippets,
                                   &state->hash);
}

void
_cg_pipeline_hash_fragment_snippets_state(cg_pipeline_t *authority,
                                          cg_pipeline_hash_state_t *state)
{
    _cg_pipeline_snippet_list_hash(&authority->big_state->fragment_snippets,
                                   &state->hash);
}

TEST(check_blend_constant_ancestry)
{
    cg_pipeline_t *pipeline;
    cg_node_t *node;
    int pipeline_length = 0;
    int i;

    test_cg_init();

    pipeline = cg_pipeline_new(test_dev);

    /* Repeatedly making a copy of a pipeline and changing the same
     * state (in this case the blend constant) shouldn't cause a long
     * chain of pipelines to be created because the redundant ancestry
     * should be pruned. */

    for (i = 0; i < 20; i++) {
        cg_color_t color = { i / 20.0f, 0.0f, 0.0f, 1.0f };
        cg_pipeline_t *tmp_pipeline;

        tmp_pipeline = cg_pipeline_copy(pipeline);
        cg_object_unref(pipeline);
        pipeline = tmp_pipeline;

        cg_pipeline_set_blend_constant(pipeline, &color);
    }

    for (node = (cg_node_t *)pipeline; node; node = node->parent)
        pipeline_length++;

    c_assert_cmpint(pipeline_length, <=, 2);

    cg_object_unref(pipeline);

    test_cg_fini();
}

TEST(check_uniform_ancestry)
{
    cg_pipeline_t *pipeline;
    cg_node_t *node;
    int pipeline_length = 0;
    int i;

    test_cg_init();
    test_allow_failure();

    pipeline = cg_pipeline_new(test_dev);

    /* Repeatedly making a copy of a pipeline and changing a uniform
     * shouldn't cause a long chain of pipelines to be created */

    for (i = 0; i < 20; i++) {
        cg_pipeline_t *tmp_pipeline;
        int uniform_location;

        tmp_pipeline = cg_pipeline_copy(pipeline);
        cg_object_unref(pipeline);
        pipeline = tmp_pipeline;

        uniform_location =
            cg_pipeline_get_uniform_location(pipeline, "a_uniform");

        cg_pipeline_set_uniform_1i(pipeline, uniform_location, i);
    }

    for (node = (cg_node_t *)pipeline; node; node = node->parent)
        pipeline_length++;

    c_assert_cmpint(pipeline_length, <=, 2);

    cg_object_unref(pipeline);

    test_cg_fini();
}
