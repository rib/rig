/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009,2010,2013 Intel Corporation.
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

#include "cg-debug.h"
#include "cg-device-private.h"
#include "cg-object.h"

#include "cg-pipeline-private.h"
#include "cg-pipeline-opengl-private.h"
#include "cg-pipeline-state-private.h"
#include "cg-pipeline-layer-state-private.h"
#include "cg-texture-private.h"
#include "cg-blend-string.h"
#include "cg-color-private.h"
#include "cg-util.h"
#include "cg-profile.h"
#include "cg-depth-state-private.h"
#include "cg-private.h"

#include <clib.h>
#include <string.h>

static void _cg_pipeline_free(cg_pipeline_t *tex);
static void recursively_free_layer_caches(cg_pipeline_t *pipeline);

const cg_pipeline_fragend_t *_cg_pipeline_fragends[CG_PIPELINE_N_FRAGENDS];
const cg_pipeline_vertend_t *_cg_pipeline_vertends[CG_PIPELINE_N_VERTENDS];
/* The 'MAX' here is so that we don't define an empty array when there
   are no progends */
const cg_pipeline_progend_t *_cg_pipeline_progends
[MAX(CG_PIPELINE_N_PROGENDS, 1)];

#ifdef CG_PIPELINE_PROGEND_GLSL
#include "cg-pipeline-progend-glsl-private.h"
#endif
#ifdef CG_PIPELINE_PROGEND_NOP
#include "cg-pipeline-progend-nop-private.h"
#endif

#ifdef CG_PIPELINE_VERTEND_GLSL
#include "cg-pipeline-vertend-glsl-private.h"
#endif
#ifdef CG_PIPELINE_VERTEND_NOP
#include "cg-pipeline-vertend-nop-private.h"
#endif

#ifdef CG_PIPELINE_FRAGEND_GLSL
#include "cg-pipeline-fragend-glsl-private.h"
#endif
#ifdef CG_PIPELINE_FRAGEND_NOP
#include "cg-pipeline-fragend-nop-private.h"
#endif

CG_OBJECT_DEFINE(Pipeline, pipeline);

/*
 * This initializes the first pipeline owned by the CGlib context. All
 * subsequently instantiated pipelines created via the cg_pipeline_new()
 * API will initially be a copy of this pipeline.
 *
 * The default pipeline is the topmost ancester for all pipelines.
 */
void
_cg_pipeline_init_default_pipeline(cg_device_t *dev)
{
    /* Create new - blank - pipeline */
    cg_pipeline_t *pipeline = c_slice_new0(cg_pipeline_t);
    /* XXX: NB: It's important that we zero this to avoid polluting
     * pipeline hash values with un-initialized data */
    cg_pipeline_big_state_t *big_state = c_slice_new0(cg_pipeline_big_state_t);
    cg_pipeline_alpha_func_state_t *alpha_state = &big_state->alpha_state;
    cg_pipeline_blend_state_t *blend_state = &big_state->blend_state;
    cg_pipeline_logic_ops_state_t *logic_ops_state =
        &big_state->logic_ops_state;
    cg_pipeline_cull_face_state_t *cull_face_state =
        &big_state->cull_face_state;
    cg_pipeline_uniforms_state_t *uniforms_state = &big_state->uniforms_state;

/* Take this opportunity to setup the backends... */
#ifdef CG_PIPELINE_PROGEND_GLSL
    _cg_pipeline_progends[CG_PIPELINE_PROGEND_GLSL] =
        &_cg_pipeline_glsl_progend;
#endif
#ifdef CG_PIPELINE_PROGEND_NOP
    _cg_pipeline_progends[CG_PIPELINE_PROGEND_NOP] = &_cg_pipeline_nop_progend;
#endif

#ifdef CG_PIPELINE_VERTEND_GLSL
    _cg_pipeline_vertends[CG_PIPELINE_VERTEND_GLSL] =
        &_cg_pipeline_glsl_vertend;
#endif
#ifdef CG_PIPELINE_VERTEND_NOP
    _cg_pipeline_vertends[CG_PIPELINE_VERTEND_NOP] = &_cg_pipeline_nop_vertend;
#endif

#ifdef CG_PIPELINE_FRAGEND_GLSL
    _cg_pipeline_fragends[CG_PIPELINE_FRAGEND_GLSL] =
        &_cg_pipeline_glsl_fragend;
#endif
#ifdef CG_PIPELINE_FRAGEND_NOP
    _cg_pipeline_fragends[CG_PIPELINE_FRAGEND_NOP] = &_cg_pipeline_nop_fragend;
#endif

    _cg_pipeline_node_init(CG_NODE(pipeline));

    pipeline->immutable = false;
    pipeline->progend = CG_PIPELINE_PROGEND_UNDEFINED;
    pipeline->differences = CG_PIPELINE_STATE_ALL_SPARSE;

    pipeline->real_blend_enable = false;

    pipeline->blend_enable = CG_PIPELINE_BLEND_ENABLE_AUTOMATIC;
    pipeline->layer_differences = NULL;
    pipeline->n_layers = 0;

    pipeline->big_state = big_state;
    pipeline->has_big_state = true;

    pipeline->static_breadcrumb = "default pipeline";
    pipeline->has_static_breadcrumb = true;

    pipeline->age = 0;

    /* Use the same defaults as the GL spec... */
    cg_color_init_from_4ub(&pipeline->color, 0xff, 0xff, 0xff, 0xff);

    /* Use the same defaults as the GL spec... */
    alpha_state->alpha_func = CG_PIPELINE_ALPHA_FUNC_ALWAYS;
    alpha_state->alpha_func_reference = 0.0;

/* Not the same as the GL default, but seems saner... */
#if defined(CG_HAS_GLES2_SUPPORT) || defined(CG_HAS_GL_SUPPORT)
    blend_state->blend_equation_rgb = GL_FUNC_ADD;
    blend_state->blend_equation_alpha = GL_FUNC_ADD;
    blend_state->blend_src_factor_alpha = GL_ONE;
    blend_state->blend_dst_factor_alpha = GL_ONE_MINUS_SRC_ALPHA;
    cg_color_init_from_4ub(
        &blend_state->blend_constant, 0x00, 0x00, 0x00, 0x00);
#endif
    blend_state->blend_src_factor_rgb = GL_ONE;
    blend_state->blend_dst_factor_rgb = GL_ONE_MINUS_SRC_ALPHA;

    cg_depth_state_init(&big_state->depth_state);

    big_state->point_size = 0.0f;

    logic_ops_state->color_mask = CG_COLOR_MASK_ALL;

    cull_face_state->mode = CG_PIPELINE_CULL_FACE_MODE_NONE;
    cull_face_state->front_winding = CG_WINDING_COUNTER_CLOCKWISE;

    _cg_bitmask_init(&uniforms_state->override_mask);
    _cg_bitmask_init(&uniforms_state->changed_mask);
    uniforms_state->override_values = NULL;

    dev->default_pipeline = _cg_pipeline_object_new(pipeline);
}

static void
_cg_pipeline_unparent(cg_node_t *pipeline)
{
    /* Chain up */
    _cg_pipeline_node_unparent_real(pipeline);
}

static bool
recursively_free_layer_caches_cb(cg_node_t *node, void *user_data)
{
    recursively_free_layer_caches(CG_PIPELINE(node));
    return true;
}

/* This recursively frees the layers_cache of a pipeline and all of
 * its descendants.
 *
 * For instance if we change a pipelines ->layer_differences list
 * then that pipeline and all of its descendants may now have
 * incorrect layer caches. */
static void
recursively_free_layer_caches(cg_pipeline_t *pipeline)
{
    /* Note: we maintain the invariable that if a pipeline already has a
     * dirty layers_cache then so do all of its descendants. */
    if (pipeline->layers_cache_dirty)
        return;

    if (C_UNLIKELY(pipeline->layers_cache != pipeline->short_layers_cache))
        c_slice_free1(sizeof(cg_pipeline_layer_t *) * pipeline->n_layers,
                      pipeline->layers_cache);
    pipeline->layers_cache_dirty = true;

    _cg_pipeline_node_foreach_child(
        CG_NODE(pipeline), recursively_free_layer_caches_cb, NULL);
}

static void
_cg_pipeline_set_parent(cg_pipeline_t *pipeline,
                        cg_pipeline_t *parent)
{
    /* Chain up */
    _cg_pipeline_node_set_parent_real(CG_NODE(pipeline),
                                      CG_NODE(parent),
                                      _cg_pipeline_unparent);

    /* Since we just changed the ancestry of the pipeline its cache of
     * layers could now be invalid so free it... */
    if (pipeline->differences & CG_PIPELINE_STATE_LAYERS)
        recursively_free_layer_caches(pipeline);
}

/* XXX: Always have an eye out for opportunities to lower the cost of
 * cg_pipeline_copy. */
cg_pipeline_t *
cg_pipeline_copy(cg_pipeline_t *src)
{
    cg_pipeline_t *pipeline = c_slice_new(cg_pipeline_t);

    _cg_pipeline_node_init(CG_NODE(pipeline));

    pipeline->immutable = false;
    src->immutable = true;

    pipeline->differences = 0;

    pipeline->has_big_state = false;

    /* NB: real_blend_enable isn't a sparse property, it's valid for
     * every pipeline node so we have fast access to it. */
    pipeline->real_blend_enable = src->real_blend_enable;
    pipeline->dirty_real_blend_enable = src->dirty_real_blend_enable;
    pipeline->unknown_color_alpha = src->unknown_color_alpha;

    /* XXX:
     * consider generalizing the idea of "cached" properties. These
     * would still have an authority like other sparse properties but
     * you wouldn't have to walk up the ancestry to find the authority
     * because the value would be cached directly in each pipeline.
     */

    pipeline->layers_cache_dirty = true;

    pipeline->progend = src->progend;

    pipeline->has_static_breadcrumb = false;

    pipeline->age = 0;

    _cg_pipeline_set_parent(pipeline, src);

    return _cg_pipeline_object_new(pipeline);
}

cg_pipeline_t *
cg_pipeline_new(cg_device_t *dev)
{
    cg_pipeline_t *new;

    cg_device_connect(dev, NULL);

    new = cg_pipeline_copy(dev->default_pipeline);
#ifdef CG_DEBUG_ENABLED
    _cg_pipeline_set_static_breadcrumb(new, "new");
#endif
    return new;
}

static void
_cg_pipeline_free(cg_pipeline_t *pipeline)
{
    c_return_if_fail(c_list_empty(&CG_NODE(pipeline)->children));

    _cg_pipeline_unparent(CG_NODE(pipeline));

    if (pipeline->differences & CG_PIPELINE_STATE_UNIFORMS) {
        cg_pipeline_uniforms_state_t *uniforms_state =
            &pipeline->big_state->uniforms_state;
        int n_overrides = _cg_bitmask_popcount(&uniforms_state->override_mask);
        int i;

        for (i = 0; i < n_overrides; i++)
            _cg_boxed_value_destroy(uniforms_state->override_values + i);
        c_free(uniforms_state->override_values);

        _cg_bitmask_destroy(&uniforms_state->override_mask);
        _cg_bitmask_destroy(&uniforms_state->changed_mask);
    }

    if (pipeline->differences & CG_PIPELINE_STATE_LAYERS) {
        c_llist_foreach(pipeline->layer_differences,
                       (c_iter_func_t)cg_object_unref, NULL);
        c_llist_free(pipeline->layer_differences);
    }

    if (pipeline->differences & CG_PIPELINE_STATE_VERTEX_SNIPPETS)
        _cg_pipeline_snippet_list_free(&pipeline->big_state->vertex_snippets);

    if (pipeline->differences & CG_PIPELINE_STATE_FRAGMENT_SNIPPETS)
        _cg_pipeline_snippet_list_free(&pipeline->big_state->fragment_snippets);

    recursively_free_layer_caches(pipeline);

    if (pipeline->differences & CG_PIPELINE_STATE_NEEDS_BIG_STATE)
        c_slice_free(cg_pipeline_big_state_t, pipeline->big_state);

    c_slice_free(cg_pipeline_t, pipeline);
}

bool
_cg_pipeline_get_real_blend_enabled(cg_pipeline_t *pipeline)
{
    c_return_val_if_fail(cg_is_pipeline(pipeline), false);

    return pipeline->real_blend_enable;
}

static void
_cg_pipeline_update_layers_cache(cg_pipeline_t *pipeline)
{
    /* Note: we assume this pipeline is a _LAYERS authority */
    int n_layers;
    cg_pipeline_t *current;
    int layers_found;

    if (C_LIKELY(!pipeline->layers_cache_dirty) || pipeline->n_layers == 0)
        return;

    pipeline->layers_cache_dirty = false;

    n_layers = pipeline->n_layers;
    if (C_LIKELY(n_layers < C_N_ELEMENTS(pipeline->short_layers_cache))) {
        pipeline->layers_cache = pipeline->short_layers_cache;
        memset(pipeline->layers_cache,
               0,
               sizeof(cg_pipeline_layer_t *) *
               C_N_ELEMENTS(pipeline->short_layers_cache));
    } else {
        pipeline->layers_cache =
            c_slice_alloc0(sizeof(cg_pipeline_layer_t *) * n_layers);
    }

    /* Notes:
     *
     * Each pipeline doesn't have to contain a complete list of the layers
     * it depends on, some of them are indirectly referenced through the
     * pipeline's ancestors.
     *
     * pipeline->layer_differences only contains a list of layers that
     * have changed in relation to its parent.
     *
     * pipeline->layer_differences is not maintained sorted, but it
     * won't contain multiple layers corresponding to a particular
     * ->unit_index.
     *
     * Some of the ancestor pipelines may reference layers with
     * ->unit_index values >= n_layers so we ignore them.
     *
     * As we ascend through the ancestors we are searching for any
     * cg_pipeline_layer_ts corresponding to the texture ->unit_index
     * values in the range [0,n_layers-1]. As soon as a pointer is found
     * we ignore layers of further ancestors with the same ->unit_index
     * values.
     */

    layers_found = 0;
    for (current = pipeline; _cg_pipeline_get_parent(current);
         current = _cg_pipeline_get_parent(current)) {
        c_llist_t *l;

        if (!(current->differences & CG_PIPELINE_STATE_LAYERS))
            continue;

        for (l = current->layer_differences; l; l = l->next) {
            cg_pipeline_layer_t *layer = l->data;
            int unit_index = _cg_pipeline_layer_get_unit_index(layer);

            if (unit_index < n_layers && !pipeline->layers_cache[unit_index]) {
                pipeline->layers_cache[unit_index] = layer;
                layers_found++;
                if (layers_found == n_layers)
                    return;
            }
        }
    }

    c_warn_if_reached();
}

/* XXX: Be carefull when using this API that the callback given doesn't result
 * in the layer cache being invalidated during the iteration! */
void
_cg_pipeline_foreach_layer_internal(
    cg_pipeline_t *pipeline,
    cg_pipeline_internal_layer_callback_t callback,
    void *user_data)
{
    cg_pipeline_t *authority =
        _cg_pipeline_get_authority(pipeline, CG_PIPELINE_STATE_LAYERS);
    int n_layers;
    int i;
    bool cont;

    n_layers = authority->n_layers;
    if (n_layers == 0)
        return;

    _cg_pipeline_update_layers_cache(authority);

    for (i = 0, cont = true; i < n_layers && cont == true; i++) {
        c_return_if_fail(authority->layers_cache_dirty == false);
        cont = callback(authority->layers_cache[i], user_data);
    }
}

bool
_cg_pipeline_layer_numbers_equal(cg_pipeline_t *pipeline0,
                                 cg_pipeline_t *pipeline1)
{
    cg_pipeline_t *authority0 =
        _cg_pipeline_get_authority(pipeline0, CG_PIPELINE_STATE_LAYERS);
    cg_pipeline_t *authority1 =
        _cg_pipeline_get_authority(pipeline1, CG_PIPELINE_STATE_LAYERS);
    int n_layers = authority0->n_layers;
    int i;

    if (authority1->n_layers != n_layers)
        return false;

    _cg_pipeline_update_layers_cache(authority0);
    _cg_pipeline_update_layers_cache(authority1);

    for (i = 0; i < n_layers; i++) {
        cg_pipeline_layer_t *layer0 = authority0->layers_cache[i];
        cg_pipeline_layer_t *layer1 = authority1->layers_cache[i];

        if (layer0->index != layer1->index)
            return false;
    }

    return true;
}

typedef struct {
    int i;
    int *indices;
} append_layer_index_state_t;

static bool
append_layer_index_cb(cg_pipeline_layer_t *layer, void *user_data)
{
    append_layer_index_state_t *state = user_data;
    state->indices[state->i++] = layer->index;
    return true;
}

void
cg_pipeline_foreach_layer(cg_pipeline_t *pipeline,
                          cg_pipeline_layer_callback_t callback,
                          void *user_data)
{
    cg_pipeline_t *authority =
        _cg_pipeline_get_authority(pipeline, CG_PIPELINE_STATE_LAYERS);
    append_layer_index_state_t state;

    /* XXX: We don't know what the user is going to want to do to the layers
     * but any modification of layers can result in the layer graph changing
     * which could confuse _cg_pipeline_foreach_layer_internal(). We first
     * get a list of layer indices which will remain valid so long as the
     * user doesn't remove layers. */

    state.i = 0;
    state.indices = c_alloca(authority->n_layers * sizeof(int));

    _cg_pipeline_foreach_layer_internal(
        pipeline, append_layer_index_cb, &state);

    for (unsigned i = 0, cont = true; i < authority->n_layers && cont; i++)
        cont = callback(pipeline, state.indices[i], user_data);
}

static bool
layer_has_alpha_cb(cg_pipeline_layer_t *layer, void *data)
{
    bool *has_alpha = data;
    *has_alpha = _cg_pipeline_layer_has_alpha(layer);

    /* return false to stop iterating layers if we find any layer
     * has alpha ...
     *
     * FIXME: actually we should never be bailing out because it's
     * always possible that a later layer could discard any previous
     * alpha!
     */

    return !(*has_alpha);
}

/* NB: If this pipeline returns false that doesn't mean that the
 * pipeline is definitely opaque, it just means that that the
 * given changes dont imply transparency.
 *
 * If you want to find out of the pipeline is opaque then assuming
 * this returns false for a set of changes then you can follow
 * up
 */
static bool
_cg_pipeline_change_implies_transparency(cg_pipeline_t *pipeline,
                                         unsigned int changes,
                                         const cg_color_t *override_color,
                                         bool unknown_color_alpha)
{
    /* In the case of a layer state change we need to check everything
     * else first since they contribute to the has_alpha status of the
     * "PREVIOUS" layer. */
    if (changes & CG_PIPELINE_STATE_LAYERS)
        changes = CG_PIPELINE_STATE_AFFECTS_BLENDING;

    if (unknown_color_alpha)
        return true;

    if ((override_color && cg_color_get_alpha_byte(override_color) != 0xff))
        return true;

    if (changes & CG_PIPELINE_STATE_COLOR) {
        cg_color_t tmp;
        cg_pipeline_get_color(pipeline, &tmp);
        if (cg_color_get_alpha_byte(&tmp) != 0xff)
            return true;
    }

    if (changes & CG_PIPELINE_STATE_FRAGMENT_SNIPPETS) {
        if (_cg_pipeline_has_non_layer_fragment_snippets(pipeline))
            return true;
    }

    if (changes & CG_PIPELINE_STATE_VERTEX_SNIPPETS) {
        if (_cg_pipeline_has_non_layer_vertex_snippets(pipeline))
            return true;
    }

    if (changes & CG_PIPELINE_STATE_LAYERS) {
        /* has_alpha tracks the alpha status of the GL_PREVIOUS layer.
         * To start with that's defined by the pipeline color which
         * must be fully opaque if we got this far. */
        bool has_alpha = false;
        _cg_pipeline_foreach_layer_internal(
            pipeline, layer_has_alpha_cb, &has_alpha);
        if (has_alpha)
            return true;
    }

    return false;
}

static bool
_cg_pipeline_needs_blending_enabled(cg_pipeline_t *pipeline,
                                    unsigned int changes,
                                    const cg_color_t *override_color,
                                    bool unknown_color_alpha)
{
    cg_pipeline_t *enable_authority;
    cg_pipeline_t *blend_authority;
    cg_pipeline_blend_state_t *blend_state;
    cg_pipeline_blend_enable_t enabled;

    if (C_UNLIKELY(CG_DEBUG_ENABLED(CG_DEBUG_DISABLE_BLENDING)))
        return false;

    /* We unconditionally check the _BLEND_ENABLE state first because
     * all the other changes are irrelevent if blend_enable != _AUTOMATIC
     */
    enable_authority =
        _cg_pipeline_get_authority(pipeline, CG_PIPELINE_STATE_BLEND_ENABLE);

    enabled = enable_authority->blend_enable;
    if (enabled != CG_PIPELINE_BLEND_ENABLE_AUTOMATIC)
        return enabled == CG_PIPELINE_BLEND_ENABLE_ENABLED ? true : false;

    blend_authority =
        _cg_pipeline_get_authority(pipeline, CG_PIPELINE_STATE_BLEND);

    blend_state = &blend_authority->big_state->blend_state;

    /* We are trying to identify some cases that are equivalent to
     * blending being disable, where the output is simply GL_SRC_COLOR.
     *
     * Note: we currently only consider a few cases that can be
     * optimized but there could be opportunities to special case more
     * blend functions later.
     */

    /* As the most common way that we currently use to effectively
     * disable blending is to use an equation of
     * "RGBA=ADD(SRC_COLOR, 0)" that's the first thing we check
     * for... */
    if (blend_state->blend_equation_rgb == GL_FUNC_ADD &&
        blend_state->blend_equation_alpha == GL_FUNC_ADD &&
        blend_state->blend_src_factor_alpha == GL_ONE &&
        blend_state->blend_dst_factor_alpha == GL_ZERO) {
        return false;
    }

    /* NB: The default blending equation for CGlib is
     * "RGBA=ADD(SRC_COLOR, DST_COLOR * (1-SRC_COLOR[A]))"
     *
     * Next we check if the default blending equation is being used.  If
     * so then we follow that by looking for cases where SRC_COLOR[A] ==
     * 1 since that simplifies "DST_COLOR * (1-SRC_COLOR[A])" to 0 which
     * also effectively requires no blending.
     */

    if (blend_state->blend_equation_rgb != GL_FUNC_ADD ||
        blend_state->blend_equation_alpha != GL_FUNC_ADD)
        return true;

    if (blend_state->blend_src_factor_alpha != GL_ONE ||
        blend_state->blend_dst_factor_alpha != GL_ONE_MINUS_SRC_ALPHA)
        return true;

    if (blend_state->blend_src_factor_rgb != GL_ONE ||
        blend_state->blend_dst_factor_rgb != GL_ONE_MINUS_SRC_ALPHA)
        return true;

    /* Given the above constraints, it's now a case of finding any
     * SRC_ALPHA that != 1 */

    if (_cg_pipeline_change_implies_transparency(
            pipeline, changes, override_color, unknown_color_alpha))
        return true;

    /* At this point, considering just the state that has changed it
     * looks like blending isn't needed. If blending was previously
     * enabled though it could be that some other state still requires
     * that we have blending enabled because it implies transparency.
     * In this case we still need to go and check the other state...
     *
     * XXX: We could explicitly keep track of the mask of state groups
     * that are currently causing blending to be enabled so that we
     * never have to resort to checking *all* the state and can instead
     * always limit the check to those in the mask.
     */
    if (pipeline->real_blend_enable) {
        unsigned int other_state =
            CG_PIPELINE_STATE_AFFECTS_BLENDING & ~changes;
        if (other_state && _cg_pipeline_change_implies_transparency(
                pipeline, other_state, NULL, false))
            return true;
    }

    return false;
}

void
_cg_pipeline_set_progend(cg_pipeline_t *pipeline, int progend)
{
    pipeline->progend = progend;
}

static void
_cg_pipeline_copy_differences(cg_pipeline_t *dest,
                              cg_pipeline_t *src,
                              unsigned long differences)
{
    cg_pipeline_big_state_t *big_state;

    if (differences & CG_PIPELINE_STATE_COLOR)
        dest->color = src->color;

    if (differences & CG_PIPELINE_STATE_BLEND_ENABLE)
        dest->blend_enable = src->blend_enable;

    if (differences & CG_PIPELINE_STATE_LAYERS) {
        c_llist_t *l;

        if (dest->differences & CG_PIPELINE_STATE_LAYERS &&
            dest->layer_differences) {
            c_llist_foreach(dest->layer_differences,
                           (c_iter_func_t)cg_object_unref, NULL);
            c_llist_free(dest->layer_differences);
        }

        for (l = src->layer_differences; l; l = l->next) {
            /* NB: a layer can't have more than one ->owner so we can't
             * simply take a references on each of the original
             * layer_differences, we have to derive new layers from the
             * originals instead. */
            cg_pipeline_layer_t *copy = _cg_pipeline_layer_copy(l->data);
            _cg_pipeline_add_layer_difference(dest, copy, false);
            cg_object_unref(copy);
        }

        /* Note: we initialize n_layers after adding the layer differences
         * since the act of adding the layers will initialize n_layers to 0
         * because dest isn't initially a STATE_LAYERS authority. */
        dest->n_layers = src->n_layers;
    }

    if (differences & CG_PIPELINE_STATE_NEEDS_BIG_STATE) {
        if (!dest->has_big_state) {
            dest->big_state = c_slice_new(cg_pipeline_big_state_t);
            dest->has_big_state = true;
        }
        big_state = dest->big_state;
    } else
        goto check_for_blending_change;

    if (differences & CG_PIPELINE_STATE_ALPHA_FUNC)
        big_state->alpha_state.alpha_func =
            src->big_state->alpha_state.alpha_func;

    if (differences & CG_PIPELINE_STATE_ALPHA_FUNC_REFERENCE)
        big_state->alpha_state.alpha_func_reference =
            src->big_state->alpha_state.alpha_func_reference;

    if (differences & CG_PIPELINE_STATE_BLEND) {
        memcpy(&big_state->blend_state,
               &src->big_state->blend_state,
               sizeof(cg_pipeline_blend_state_t));
    }

    if (differences & CG_PIPELINE_STATE_DEPTH) {
        memcpy(&big_state->depth_state,
               &src->big_state->depth_state,
               sizeof(cg_depth_state_t));
    }

    if (differences & CG_PIPELINE_STATE_NON_ZERO_POINT_SIZE)
        big_state->non_zero_point_size = src->big_state->non_zero_point_size;

    if (differences & CG_PIPELINE_STATE_POINT_SIZE)
        big_state->point_size = src->big_state->point_size;

    if (differences & CG_PIPELINE_STATE_PER_VERTEX_POINT_SIZE)
        big_state->per_vertex_point_size =
            src->big_state->per_vertex_point_size;

    if (differences & CG_PIPELINE_STATE_LOGIC_OPS) {
        memcpy(&big_state->logic_ops_state,
               &src->big_state->logic_ops_state,
               sizeof(cg_pipeline_logic_ops_state_t));
    }

    if (differences & CG_PIPELINE_STATE_CULL_FACE) {
        memcpy(&big_state->cull_face_state,
               &src->big_state->cull_face_state,
               sizeof(cg_pipeline_cull_face_state_t));
    }

    if (differences & CG_PIPELINE_STATE_UNIFORMS) {
        int n_overrides =
            _cg_bitmask_popcount(&src->big_state->uniforms_state.override_mask);
        int i;

        big_state->uniforms_state.override_values =
            c_malloc(n_overrides * sizeof(cg_boxed_value_t));

        for (i = 0; i < n_overrides; i++) {
            cg_boxed_value_t *dst_bv =
                big_state->uniforms_state.override_values + i;
            const cg_boxed_value_t *src_bv =
                src->big_state->uniforms_state.override_values + i;

            _cg_boxed_value_copy(dst_bv, src_bv);
        }

        _cg_bitmask_init(&big_state->uniforms_state.override_mask);
        _cg_bitmask_set_bits(&big_state->uniforms_state.override_mask,
                             &src->big_state->uniforms_state.override_mask);

        _cg_bitmask_init(&big_state->uniforms_state.changed_mask);
    }

    if (differences & CG_PIPELINE_STATE_VERTEX_SNIPPETS)
        _cg_pipeline_snippet_list_copy(&big_state->vertex_snippets,
                                       &src->big_state->vertex_snippets);

    if (differences & CG_PIPELINE_STATE_FRAGMENT_SNIPPETS)
        _cg_pipeline_snippet_list_copy(&big_state->fragment_snippets,
                                       &src->big_state->fragment_snippets);

/* XXX: we shouldn't bother doing this in most cases since
 * _copy_differences is typically used to initialize pipeline state
 * by copying it from the current authority, so it's not actually
 * *changing* anything.
 */
check_for_blending_change:
    if (differences & CG_PIPELINE_STATE_AFFECTS_BLENDING)
        dest->dirty_real_blend_enable = true;

    dest->differences |= differences;
}

static void
_cg_pipeline_init_multi_property_sparse_state(cg_pipeline_t *pipeline,
                                              cg_pipeline_state_t change)
{
    cg_pipeline_t *authority;

    c_return_if_fail(change & CG_PIPELINE_STATE_ALL_SPARSE);

    if (!(change & CG_PIPELINE_STATE_MULTI_PROPERTY))
        return;

    authority = _cg_pipeline_get_authority(pipeline, change);

    switch (change) {
    /* XXX: avoid using a default: label so we get a warning if we
    * don't explicitly handle a newly defined state-group here. */
    case CG_PIPELINE_STATE_COLOR:
    case CG_PIPELINE_STATE_BLEND_ENABLE:
    case CG_PIPELINE_STATE_ALPHA_FUNC:
    case CG_PIPELINE_STATE_ALPHA_FUNC_REFERENCE:
    case CG_PIPELINE_STATE_NON_ZERO_POINT_SIZE:
    case CG_PIPELINE_STATE_POINT_SIZE:
    case CG_PIPELINE_STATE_PER_VERTEX_POINT_SIZE:
    case CG_PIPELINE_STATE_REAL_BLEND_ENABLE:
        c_return_if_reached();

    case CG_PIPELINE_STATE_LAYERS:
        pipeline->n_layers = authority->n_layers;
        pipeline->layer_differences = NULL;
        break;
    case CG_PIPELINE_STATE_BLEND: {
        memcpy(&pipeline->big_state->blend_state,
               &authority->big_state->blend_state,
               sizeof(cg_pipeline_blend_state_t));
        break;
    }
    case CG_PIPELINE_STATE_DEPTH: {
        memcpy(&pipeline->big_state->depth_state,
               &authority->big_state->depth_state,
               sizeof(cg_depth_state_t));
        break;
    }
    case CG_PIPELINE_STATE_LOGIC_OPS: {
        memcpy(&pipeline->big_state->logic_ops_state,
               &authority->big_state->logic_ops_state,
               sizeof(cg_pipeline_logic_ops_state_t));
        break;
    }
    case CG_PIPELINE_STATE_CULL_FACE: {
        memcpy(&pipeline->big_state->cull_face_state,
               &authority->big_state->cull_face_state,
               sizeof(cg_pipeline_cull_face_state_t));
        break;
    }
    case CG_PIPELINE_STATE_UNIFORMS: {
        cg_pipeline_uniforms_state_t *uniforms_state =
            &pipeline->big_state->uniforms_state;
        _cg_bitmask_init(&uniforms_state->override_mask);
        _cg_bitmask_init(&uniforms_state->changed_mask);
        uniforms_state->override_values = NULL;
        break;
    }
    case CG_PIPELINE_STATE_VERTEX_SNIPPETS:
        _cg_pipeline_snippet_list_copy(&pipeline->big_state->vertex_snippets,
                                       &authority->big_state->vertex_snippets);
        break;

    case CG_PIPELINE_STATE_FRAGMENT_SNIPPETS:
        _cg_pipeline_snippet_list_copy(
            &pipeline->big_state->fragment_snippets,
            &authority->big_state->fragment_snippets);
        break;
    }
}

static bool
reparent_children_cb(cg_node_t *node, void *user_data)
{
    cg_pipeline_t *pipeline = CG_PIPELINE(node);
    cg_pipeline_t *parent = user_data;

    _cg_pipeline_set_parent(pipeline, parent);

    return true;
}

void
_cg_pipeline_pre_change_notify(cg_pipeline_t *pipeline,
                               cg_pipeline_state_t change,
                               const cg_color_t *new_color,
                               bool from_layer_change)
{
    _CG_GET_DEVICE(dev, NO_RETVAL);

    /* XXX:
     * To simplify things for the vertex, fragment and program backends
     * we are careful about how we report STATE_LAYERS changes.
     *
     * All STATE_LAYERS change notifications with the exception of
     * ->n_layers will also result in layer_pre_change_notifications.
     *
     * For backends that perform code generation for fragment processing
     * they typically need to understand the details of how layers get
     * changed to determine if they need to repeat codegen.  It doesn't
     * help them to report a pipeline STATE_LAYERS change for all layer
     * changes since it's so broad, they really need to wait for the
     * specific layer change to be notified.  What does help though is
     * to report a STATE_LAYERS change for a change in ->n_layers
     * because they typically do need to repeat codegen in that case.
     *
     * Here we ensure that change notifications against a pipeline or
     * against a layer are mutually exclusive as far as fragment, vertex
     * and program backends are concerned.
     *
     * NB: A pipeline can potentially have private state from multiple
     * backends associated with it because descendants may cache state
     * with an ancestor to maximize the chance that it can later be
     * re-used by other descendants and a descendent can require a
     * different backend to an ancestor.
     */
    if (!from_layer_change) {
        int i;

        for (i = 0; i < CG_PIPELINE_N_PROGENDS; i++) {
            const cg_pipeline_progend_t *progend = _cg_pipeline_progends[i];
            const cg_pipeline_vertend_t *vertend =
                _cg_pipeline_vertends[progend->vertend];
            const cg_pipeline_fragend_t *fragend =
                _cg_pipeline_fragends[progend->fragend];

            if (vertend->pipeline_pre_change_notify)
                vertend->pipeline_pre_change_notify(dev, pipeline, change,
                                                    new_color);

            /* TODO: make the vertend and fragend implementation details
             * of the progend */

            if (fragend->pipeline_pre_change_notify)
                fragend->pipeline_pre_change_notify(dev, pipeline, change,
                                                    new_color);

            if (progend->pipeline_pre_change_notify)
                progend->pipeline_pre_change_notify(dev, pipeline, change,
                                                    new_color);
        }
    }

    /* There may be an arbitrary tree of descendants of this pipeline;
     * any of which may indirectly depend on this pipeline as the
     * authority for some set of properties. (Meaning for example that
     * one of its descendants derives its color or blending state from
     * this pipeline.)
     *
     * We can't modify any property that this pipeline is the authority
     * for unless we create another pipeline to take its place first and
     * make sure descendants reference this new pipeline instead.
     */

    if (pipeline->immutable && !c_list_empty(&CG_NODE(pipeline)->children))
        c_warning("immutable pipeline %p being modified", pipeline);

    /* If there are still children remaining though we'll need to
     * perform a copy-on-write and reparent the dependants as children
     * of the copy. */
    if (!c_list_empty(&CG_NODE(pipeline)->children)) {
        cg_pipeline_t *new_authority;

        CG_STATIC_COUNTER(pipeline_copy_on_write_counter,
                          "pipeline copy on write counter",
                          "Increments each time a pipeline "
                          "must be copied to allow modification",
                          0 /* no application private data */);

        CG_COUNTER_INC(_cg_uprof_context, pipeline_copy_on_write_counter);


        new_authority = cg_pipeline_copy(_cg_pipeline_get_parent(pipeline));
#ifdef CG_DEBUG_ENABLED
        _cg_pipeline_set_static_breadcrumb(new_authority,
                                           "pre_change_notify:copy-on-write");
#endif

        /* We could explicitly walk the descendants, OR together the set
         * of differences that we determine this pipeline is the
         * authority on and only copy those differences copied across.
         *
         * Or, if we don't explicitly walk the descendants we at least
         * know that pipeline->differences represents the largest set of
         * differences that this pipeline could possibly be an authority
         * on.
         *
         * We do the later just because it's simplest, but we might need
         * to come back to this later...
         */
        _cg_pipeline_copy_differences(
            new_authority, pipeline, pipeline->differences);

        /* Reparent the dependants of pipeline to be children of
         * new_authority instead... */
        _cg_pipeline_node_foreach_child(
            CG_NODE(pipeline), reparent_children_cb, new_authority);

        /* The children will keep the new authority alive so drop the
         * reference we got when copying... */
        cg_object_unref(new_authority);
    }

    /* At this point we know we have a pipeline with no dependants so
     * we are now free to modify the pipeline. */

    pipeline->age++;

    if (change & CG_PIPELINE_STATE_NEEDS_BIG_STATE &&
        !pipeline->has_big_state) {
        pipeline->big_state = c_slice_new(cg_pipeline_big_state_t);
        pipeline->has_big_state = true;
    }

    /* Note: conceptually we have just been notified that a single
     * property value is about to change, but since some state-groups
     * contain multiple properties and 'pipeline' is about to take over
     * being the authority for the property's corresponding state-group
     * we need to maintain the integrity of the other property values
     * too.
     *
     * To ensure this we handle multi-property state-groups by copying
     * all the values from the old-authority to the new...
     *
     * We don't have to worry about non-sparse property groups since
     * we never take over being an authority for such properties so
     * they automatically maintain integrity.
     */
    if (change & CG_PIPELINE_STATE_ALL_SPARSE &&
        !(pipeline->differences & change)) {
        _cg_pipeline_init_multi_property_sparse_state(pipeline, change);
        pipeline->differences |= change;
    }

    /* Each pipeline has a sorted cache of the layers it depends on
     * which will need updating via _cg_pipeline_update_layers_cache
     * if a pipeline's layers are changed. */
    if (change == CG_PIPELINE_STATE_LAYERS)
        recursively_free_layer_caches(pipeline);

    /* If the pipeline being changed is the same as the last pipeline we
     * flushed then we keep a track of the changes so we can try to
     * minimize redundant OpenGL calls if the same pipeline is flushed
     * again.
     */
    if (dev->current_pipeline == pipeline)
        dev->current_pipeline_changes_since_flush |= change;
}

void
_cg_pipeline_add_layer_difference(cg_pipeline_t *pipeline,
                                  cg_pipeline_layer_t *layer,
                                  bool inc_n_layers)
{
    c_return_if_fail(layer->owner == NULL);

    layer->owner = pipeline;
    cg_object_ref(layer);

    /* - Make sure the pipeline has no dependants so it may be modified.
     * - If the pipeline isn't currently an authority for the state being
     *   changed, then initialize that state from the current authority.
     */
    /* Note: the last argument to _cg_pipeline_pre_change_notify is
     * needed to differentiate STATE_LAYER changes which don't affect
     * the number of layers from those that do. NB: Layer change
     * notifications that don't change the number of layers don't get
     * forwarded to the fragend. */
    _cg_pipeline_pre_change_notify(
        pipeline, CG_PIPELINE_STATE_LAYERS, NULL, !inc_n_layers);

    pipeline->differences |= CG_PIPELINE_STATE_LAYERS;

    pipeline->layer_differences =
        c_llist_prepend(pipeline->layer_differences, layer);

    if (inc_n_layers)
        pipeline->n_layers++;

    /* Adding a layer difference may mean this pipeline now overrides
     * all of the layers of its parent which might make the parent
     * redundant so we should try to prune the hierarchy */
    _cg_pipeline_prune_redundant_ancestry(pipeline);
}

void
_cg_pipeline_remove_layer_difference(cg_pipeline_t *pipeline,
                                     cg_pipeline_layer_t *layer,
                                     bool dec_n_layers)
{
    /* - Make sure the pipeline has no dependants so it may be modified.
     * - If the pipeline isn't currently an authority for the state being
     *   changed, then initialize that state from the current authority.
     */
    /* Note: the last argument to _cg_pipeline_pre_change_notify is
     * needed to differentiate STATE_LAYER changes which don't affect
     * the number of layers from those that do. NB: Layer change
     * notifications that don't change the number of layers don't get
     * forwarded to the fragend. */
    _cg_pipeline_pre_change_notify(
        pipeline, CG_PIPELINE_STATE_LAYERS, NULL, !dec_n_layers);

    /* We only need to remove the layer difference if the pipeline is
     * currently the owner. If it is not the owner then one of two
     * things will happen to make sure this layer is replaced. If it is
     * the last layer being removed then decrementing n_layers will
     * ensure that the last layer is skipped. If it is any other layer
     * then the subsequent layers will have been shifted down and cause
     * it be replaced */
    if (layer->owner == pipeline) {
        layer->owner = NULL;
        cg_object_unref(layer);

        pipeline->layer_differences =
            c_llist_remove(pipeline->layer_differences, layer);
    }

    pipeline->differences |= CG_PIPELINE_STATE_LAYERS;

    if (dec_n_layers)
        pipeline->n_layers--;
}

static void
_cg_pipeline_try_reverting_layers_authority(cg_pipeline_t *authority,
                                            cg_pipeline_t *old_authority)
{
    if (authority->layer_differences == NULL &&
        _cg_pipeline_get_parent(authority)) {
        /* If the previous _STATE_LAYERS authority has the same
         * ->n_layers then we can revert to that being the authority
         *  again. */
        if (!old_authority) {
            old_authority = _cg_pipeline_get_authority(
                _cg_pipeline_get_parent(authority), CG_PIPELINE_STATE_LAYERS);
        }

        if (old_authority->n_layers == authority->n_layers)
            authority->differences &= ~CG_PIPELINE_STATE_LAYERS;
    }
}

void
_cg_pipeline_update_real_blend_enable(cg_pipeline_t *pipeline,
                                      bool unknown_color_alpha)
{
    cg_pipeline_t *parent;
    unsigned int differences;

    if (pipeline->dirty_real_blend_enable == false &&
        pipeline->unknown_color_alpha == unknown_color_alpha)
        return;

    if (pipeline->dirty_real_blend_enable) {
        differences = pipeline->differences;

        parent = _cg_pipeline_get_parent(pipeline);
        while (parent->dirty_real_blend_enable) {
            differences |= parent->differences;
            parent = _cg_pipeline_get_parent(parent);
        }

        /* We initialize the pipeline's real_blend_enable with a known
         * reference value from its nearest ancestor with clean state so
         * we can then potentially reduce the work involved in checking
         * if the pipeline really needs blending itself because we can
         * just look at the things that differ between the ancestor and
         * this pipeline.
         */
        pipeline->real_blend_enable = parent->real_blend_enable;
    } else /* pipeline->unknown_color_alpha != unknown_color_alpha */
        differences = 0;

    /* Note we don't call _cg_pipeline_pre_change_notify() for this
     * state change because ->real_blend_enable is lazily derived from
     * other state while flushing the pipeline. */
    pipeline->real_blend_enable = _cg_pipeline_needs_blending_enabled(
        pipeline, differences, NULL, unknown_color_alpha);
    pipeline->dirty_real_blend_enable = false;
    pipeline->unknown_color_alpha = unknown_color_alpha;
}

typedef struct {
    int keep_n;
    int current_pos;
    int first_index_to_prune;
} cg_pipeline_prune_layers_info_t;

static bool
update_prune_layers_info_cb(cg_pipeline_layer_t *layer,
                            void *user_data)
{
    cg_pipeline_prune_layers_info_t *state = user_data;

    if (state->current_pos == state->keep_n) {
        state->first_index_to_prune = layer->index;
        return false;
    }
    state->current_pos++;
    return true;
}

void
_cg_pipeline_prune_to_n_layers(cg_pipeline_t *pipeline, int n)
{
    cg_pipeline_t *authority =
        _cg_pipeline_get_authority(pipeline, CG_PIPELINE_STATE_LAYERS);
    cg_pipeline_prune_layers_info_t state;
    c_llist_t *l;
    c_llist_t *next;

    if (authority->n_layers <= n)
        return;

    /* This call to foreach_layer_internal needs to be done before
     * calling pre_change_notify because it recreates the layer cache.
     * We are relying on pre_change_notify to clear the layer cache
     * before we change the number of layers */
    state.keep_n = n;
    state.current_pos = 0;
    _cg_pipeline_foreach_layer_internal(
        pipeline, update_prune_layers_info_cb, &state);

    _cg_pipeline_pre_change_notify(
        pipeline, CG_PIPELINE_STATE_LAYERS, NULL, false);

    pipeline->differences |= CG_PIPELINE_STATE_LAYERS;
    pipeline->n_layers = n;

    /* It's possible that this pipeline owns some of the layers being
     * discarded, so we'll need to unlink them... */
    for (l = pipeline->layer_differences; l; l = next) {
        cg_pipeline_layer_t *layer = l->data;
        next = l->next; /* we're modifying the list we're iterating */

        if (layer->index >= state.first_index_to_prune)
            _cg_pipeline_remove_layer_difference(pipeline, layer, false);
    }

    pipeline->differences |= CG_PIPELINE_STATE_LAYERS;
}

typedef struct {
    /* The layer we are trying to find */
    int layer_index;

    /* The layer we find or untouched if not found */
    cg_pipeline_layer_t *layer;

    /* If the layer can't be found then a new layer should be
     * inserted after this texture unit index... */
    int insert_after;

    /* When adding a layer we need the list of layers to shift up
     * to a new texture unit. When removing we need the list of
     * layers to shift down.
     *
     * Note: the list isn't sorted */
    cg_pipeline_layer_t **layers_to_shift;
    int n_layers_to_shift;

    /* When adding a layer we don't need a complete list of
     * layers_to_shift if we find a layer already corresponding to the
     * layer_index.  */
    bool ignore_shift_layers_if_found;

} cg_pipeline_layer_info_t;

/* Returns true once we know there is nothing more to update */
static bool
update_layer_info(cg_pipeline_layer_t *layer,
                  cg_pipeline_layer_info_t *layer_info)
{
    if (layer->index == layer_info->layer_index) {
        layer_info->layer = layer;
        if (layer_info->ignore_shift_layers_if_found)
            return true;
    } else if (layer->index < layer_info->layer_index) {
        int unit_index = _cg_pipeline_layer_get_unit_index(layer);
        layer_info->insert_after = unit_index;
    } else
        layer_info->layers_to_shift[layer_info->n_layers_to_shift++] = layer;

    return false;
}

/* Returns false to break out of a _foreach_layer () iteration */
static bool
update_layer_info_cb(cg_pipeline_layer_t *layer, void *user_data)
{
    cg_pipeline_layer_info_t *layer_info = user_data;

    if (update_layer_info(layer, layer_info))
        return false; /* break */
    else
        return true; /* continue */
}

static void
_cg_pipeline_get_layer_info(cg_pipeline_t *pipeline,
                            cg_pipeline_layer_info_t *layer_info)
{
    /* Note: we are assuming this pipeline is a _STATE_LAYERS authority */
    int n_layers = pipeline->n_layers;
    int i;

    /* FIXME: _cg_pipeline_foreach_layer_internal now calls
     * _cg_pipeline_update_layers_cache anyway so this codepath is
     * pointless! */
    if (layer_info->ignore_shift_layers_if_found &&
        pipeline->layers_cache_dirty) {
        /* The expectation is that callers of
         * _cg_pipeline_get_layer_info are likely to be modifying the
         * list of layers associated with a pipeline so in this case
         * where we don't have a cache of the layers and we don't
         * necessarily have to iterate all the layers of the pipeline we
         * use a foreach_layer callback instead of updating the cache
         * and iterating that as below. */
        _cg_pipeline_foreach_layer_internal(
            pipeline, update_layer_info_cb, layer_info);
        return;
    }

    _cg_pipeline_update_layers_cache(pipeline);
    for (i = 0; i < n_layers; i++) {
        cg_pipeline_layer_t *layer = pipeline->layers_cache[i];

        if (update_layer_info(layer, layer_info))
            return;
    }
}

cg_pipeline_layer_t *
_cg_pipeline_get_layer_with_flags(cg_pipeline_t *pipeline,
                                  int layer_index,
                                  cg_pipeline_get_layer_flags_t flags)
{
    cg_pipeline_t *authority =
        _cg_pipeline_get_authority(pipeline, CG_PIPELINE_STATE_LAYERS);
    cg_pipeline_layer_info_t layer_info;
    cg_pipeline_layer_t *layer;
    int unit_index;
    int i;
    cg_device_t *dev;

    /* The layer index of the layer we want info about */
    layer_info.layer_index = layer_index;

    /* If a layer already exists with the given index this will be
     * updated. */
    layer_info.layer = NULL;

    /* If a layer isn't found for the given index we'll need to know
     * where to insert a new layer. */
    layer_info.insert_after = -1;

    /* If a layer can't be found then we'll need to insert a new layer
     * and bump up the texture unit for all layers with an index
     * > layer_index. */
    layer_info.layers_to_shift =
        c_alloca(sizeof(cg_pipeline_layer_t *) * authority->n_layers);
    layer_info.n_layers_to_shift = 0;

    /* If an exact match is found though we don't need a complete
     * list of layers with indices > layer_index... */
    layer_info.ignore_shift_layers_if_found = true;

    _cg_pipeline_get_layer_info(authority, &layer_info);

    if (layer_info.layer || (flags & CG_PIPELINE_GET_LAYER_NO_CREATE))
        return layer_info.layer;

    dev = _cg_device_get_default();

    unit_index = layer_info.insert_after + 1;
    if (unit_index == 0)
        layer = _cg_pipeline_layer_copy(dev->default_layer_0);
    else {
        cg_pipeline_layer_t *new;
        layer = _cg_pipeline_layer_copy(dev->default_layer_n);
        new = _cg_pipeline_set_layer_unit(NULL, layer, unit_index);
        /* Since we passed a newly allocated layer we wouldn't expect
         * _set_layer_unit() to have to allocate *another* layer. */
        c_assert(new == layer);
    }
    layer->index = layer_index;

    for (i = 0; i < layer_info.n_layers_to_shift; i++) {
        cg_pipeline_layer_t *shift_layer = layer_info.layers_to_shift[i];

        unit_index = _cg_pipeline_layer_get_unit_index(shift_layer);
        _cg_pipeline_set_layer_unit(pipeline, shift_layer, unit_index + 1);
        /* NB: shift_layer may not be writeable so _set_layer_unit()
         * will allocate a derived layer internally which will become
         * owned by pipeline. Check the return value if we need to do
         * anything else with this layer. */
    }

    _cg_pipeline_add_layer_difference(pipeline, layer, true);

    cg_object_unref(layer);

    return layer;
}

void
_cg_pipeline_prune_empty_layer_difference(cg_pipeline_t *layers_authority,
                                          cg_pipeline_layer_t *layer)
{
    /* Find the c_llist_t link that references the empty layer */
    c_llist_t *link = c_llist_find(layers_authority->layer_differences, layer);
    /* No pipeline directly owns the root node layer so this is safe... */
    cg_pipeline_layer_t *layer_parent = _cg_pipeline_layer_get_parent(layer);
    cg_pipeline_layer_info_t layer_info;
    cg_pipeline_t *old_layers_authority;

    c_return_if_fail(link != NULL);

    /* If the layer's parent doesn't have an owner then we can simply
     * take ownership ourselves and drop our reference on the empty
     * layer. We don't want to take ownership of the root node layer so
     * we also need to verify that the parent has a parent
     */
    if (layer_parent->index == layer->index && layer_parent->owner == NULL &&
        _cg_pipeline_layer_get_parent(layer_parent) != NULL) {
        cg_object_ref(layer_parent);
        layer_parent->owner = layers_authority;
        link->data = layer_parent;
        cg_object_unref(layer);
        recursively_free_layer_caches(layers_authority);
        return;
    }

    /* Now we want to find the layer that would become the authority for
     * layer->index if we were to remove layer from
     * layers_authority->layer_differences
     */

    /* The layer index of the layer we want info about */
    layer_info.layer_index = layer->index;

    /* If a layer already exists with the given index this will be
     * updated. */
    layer_info.layer = NULL;

    /* If a layer can't be found then we'll need to insert a new layer
     * and bump up the texture unit for all layers with an index
     * > layer_index. */
    layer_info.layers_to_shift =
        c_alloca(sizeof(cg_pipeline_layer_t *) * layers_authority->n_layers);
    layer_info.n_layers_to_shift = 0;

    /* If an exact match is found though we don't need a complete
     * list of layers with indices > layer_index... */
    layer_info.ignore_shift_layers_if_found = true;

    /* We know the default/root pipeline isn't a LAYERS authority so it's
     * safe to use the result of _cg_pipeline_get_parent (layers_authority)
     * without checking it.
     */
    old_layers_authority = _cg_pipeline_get_authority(
        _cg_pipeline_get_parent(layers_authority), CG_PIPELINE_STATE_LAYERS);

    _cg_pipeline_get_layer_info(old_layers_authority, &layer_info);

    /* If layer is the defining layer for the corresponding ->index then
     * we can't get rid of it. */
    if (!layer_info.layer)
        return;

    /* If the layer that would become the authority for layer->index is
     * _cg_pipeline_layer_get_parent (layer) then we can simply remove the
     * layer difference. */
    if (layer_info.layer == _cg_pipeline_layer_get_parent(layer)) {
        _cg_pipeline_remove_layer_difference(layers_authority, layer, false);
        _cg_pipeline_try_reverting_layers_authority(layers_authority,
                                                    old_layers_authority);
    }
}

typedef struct {
    int i;
    cg_pipeline_t *pipeline;
    unsigned long fallback_layers;
} cg_pipeline_fallback_state_t;

static bool
fallback_layer_cb(cg_pipeline_layer_t *layer, void *user_data)
{
    cg_pipeline_fallback_state_t *state = user_data;
    cg_pipeline_t *pipeline = state->pipeline;
    cg_texture_type_t texture_type = _cg_pipeline_layer_get_texture_type(layer);
    cg_texture_t *texture = NULL;
    CG_STATIC_COUNTER(layer_fallback_counter,
                      "layer fallback counter",
                      "Increments each time a layer's texture is "
                      "forced to a fallback texture",
                      0 /* no application private data */);

    _CG_GET_DEVICE(dev, false);

    if (!(state->fallback_layers & 1 << state->i))
        return true;

    CG_COUNTER_INC(_cg_uprof_context, layer_fallback_counter);

    switch (texture_type) {
    case CG_TEXTURE_TYPE_2D:
        texture = CG_TEXTURE(dev->default_gl_texture_2d_tex);
        break;

    case CG_TEXTURE_TYPE_3D:
        texture = CG_TEXTURE(dev->default_gl_texture_3d_tex);
        break;
    }

    if (texture == NULL) {
        c_warning("We don't have a fallback texture we can use to fill "
                  "in for an invalid pipeline layer, since it was "
                  "using an unsupported texture target ");
        /* might get away with this... */
        texture = CG_TEXTURE(dev->default_gl_texture_2d_tex);
    }

    cg_pipeline_set_layer_texture(pipeline, layer->index, texture);

    state->i++;

    return true;
}

typedef struct {
    cg_pipeline_t *pipeline;
    cg_texture_t *texture;
} cg_pipeline_override_layer_state_t;

static bool
override_layer_texture_cb(cg_pipeline_layer_t *layer,
                          void *user_data)
{
    cg_pipeline_override_layer_state_t *state = user_data;

    cg_pipeline_set_layer_texture(
        state->pipeline, layer->index, state->texture);

    return true;
}

void
_cg_pipeline_apply_overrides(cg_pipeline_t *pipeline,
                             cg_pipeline_flush_options_t *options)
{
    CG_STATIC_COUNTER(apply_overrides_counter,
                      "pipeline overrides counter",
                      "Increments each time we have to apply "
                      "override options to a pipeline",
                      0 /* no application private data */);

    CG_COUNTER_INC(_cg_uprof_context, apply_overrides_counter);

    if (options->flags & CG_PIPELINE_FLUSH_DISABLE_MASK) {
        int i;

        /* NB: we can assume that once we see one bit to disable
         * a layer, all subsequent layers are also disabled. */
        for (i = 0; i < 32 && options->disable_layers & (1 << i); i++)
            ;

        _cg_pipeline_prune_to_n_layers(pipeline, i);
    }

    if (options->flags & CG_PIPELINE_FLUSH_FALLBACK_MASK) {
        cg_pipeline_fallback_state_t state;

        state.i = 0;
        state.pipeline = pipeline;
        state.fallback_layers = options->fallback_layers;

        _cg_pipeline_foreach_layer_internal(
            pipeline, fallback_layer_cb, &state);
    }

    if (options->flags & CG_PIPELINE_FLUSH_LAYER0_OVERRIDE) {
        cg_pipeline_override_layer_state_t state;

        _cg_pipeline_prune_to_n_layers(pipeline, 1);

        /* NB: we are overriding the first layer, but we don't know
         * the user's given layer_index, which is why we use
         * _cg_pipeline_foreach_layer_internal() here even though we know
         * there's only one layer. */
        state.pipeline = pipeline;
        state.texture = options->layer0_override_texture;
        _cg_pipeline_foreach_layer_internal(
            pipeline, override_layer_texture_cb, &state);
    }
}

static bool
_cg_pipeline_layers_equal(cg_pipeline_t *authority0,
                          cg_pipeline_t *authority1,
                          unsigned long differences,
                          cg_pipeline_eval_flags_t flags)
{
    int i;

    if (authority0->n_layers != authority1->n_layers)
        return false;

    _cg_pipeline_update_layers_cache(authority0);
    _cg_pipeline_update_layers_cache(authority1);

    for (i = 0; i < authority0->n_layers; i++) {
        if (!_cg_pipeline_layer_equal(authority0->layers_cache[i],
                                      authority1->layers_cache[i],
                                      differences,
                                      flags))
            return false;
    }
    return true;
}

/* Determine the mask of differences between two pipelines */
unsigned long
_cg_pipeline_compare_differences(cg_pipeline_t *pipeline0,
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
    unsigned long pipelines_difference = 0;

    /* Algorithm:
     *
     * 1) Walk the ancestors of each pipeline to the root node, adding a
     *    pointer to each ancester node to two linked lists
     *
     * 2) Compare the lists to find the nodes where they start to
     *    differ marking the common_ancestor node for each list.
     *
     * 3) For each list now iterate starting after the common_ancestor
     *    nodes ORing each nodes ->difference mask into the final
     *    differences mask.
     */

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
        pipelines_difference |= node0->differences;
    }
    for (head1 = common_ancestor1->next; head1; head1 = head1->next) {
        node1 = head1->data;
        pipelines_difference |= node1->differences;
    }

    return pipelines_difference;
}

static void
_cg_pipeline_resolve_authorities(cg_pipeline_t *pipeline,
                                 unsigned long differences,
                                 cg_pipeline_t **authorities)
{
    unsigned long remaining = differences;
    cg_pipeline_t *authority = pipeline;

    do {
        unsigned long found = authority->differences & remaining;
        int i;

        if (found == 0)
            continue;

        for (i = 0; true; i++) {
            unsigned long state = (1L << i);

            if (state & found)
                authorities[i] = authority;
            else if (state > found)
                break;
        }

        remaining &= ~found;
        if (remaining == 0)
            return;
    } while ((authority = _cg_pipeline_get_parent(authority)));

    c_assert(remaining == 0);
}

/* Comparison of two arbitrary pipelines is done by:
 * 1) walking up the parents of each pipeline until a common
 *    ancestor is found, and at each step ORing together the
 *    difference masks.
 *
 * 2) using the final difference mask to determine which state
 *    groups to compare.
 *
 * This can be used to compare pipelines so that it can split up
 * geometry that needs different gpu state.
 *
 * XXX: When comparing texture layers, _cg_pipeline_equal will actually
 * compare the underlying GL texture handle that the CGlib texture uses so that
 * atlas textures and sub textures will be considered equal if they point to
 * the same texture. This is useful for comparing pipelines for render batching
 * but it means that _cg_pipeline_equal doesn't strictly compare whether the
 * pipelines are the same. If we needed those semantics we could perhaps add
 * another function or some flags to control the behaviour.
 */
bool
_cg_pipeline_equal(cg_pipeline_t *pipeline0,
                   cg_pipeline_t *pipeline1,
                   unsigned int differences,
                   unsigned long layer_differences,
                   cg_pipeline_eval_flags_t flags)
{
    unsigned long pipelines_difference;
    cg_pipeline_t *authorities0[CG_PIPELINE_STATE_SPARSE_COUNT];
    cg_pipeline_t *authorities1[CG_PIPELINE_STATE_SPARSE_COUNT];
    int bit;
    bool ret;

    CG_STATIC_TIMER(pipeline_equal_timer,
                    "Mainloop", /* parent */
                    "_cg_pipeline_equal",
                    "The time spent comparing cg pipelines",
                    0 /* no application private data */);

    CG_TIMER_START(_cg_uprof_context, pipeline_equal_timer);

    if (pipeline0 == pipeline1) {
        ret = true;
        goto done;
    }

    ret = false;

    _cg_pipeline_update_real_blend_enable(pipeline0, false);
    _cg_pipeline_update_real_blend_enable(pipeline1, false);

    /* First check non-sparse properties */

    if (differences & CG_PIPELINE_STATE_REAL_BLEND_ENABLE &&
        pipeline0->real_blend_enable != pipeline1->real_blend_enable)
        goto done;

    /* Then check sparse properties */

    pipelines_difference =
        _cg_pipeline_compare_differences(pipeline0, pipeline1);

    /* Only compare the sparse state groups requested by the caller... */
    pipelines_difference &= differences;

    _cg_pipeline_resolve_authorities(
        pipeline0, pipelines_difference, authorities0);
    _cg_pipeline_resolve_authorities(
        pipeline1, pipelines_difference, authorities1);

    CG_FLAGS_FOREACH_START(&pipelines_difference, 1, bit)
    {
        /* XXX: We considered having an array of callbacks for each state index
         * that we'd call here but decided that this way the compiler is more
         * likely going to be able to in-line the comparison functions and use
         * the index to jump straight to the required code. */
        switch ((cg_pipeline_state_index_t)bit) {
        case CG_PIPELINE_STATE_COLOR_INDEX:
            if (!cg_color_equal(&authorities0[bit]->color,
                                &authorities1[bit]->color))
                goto done;
            break;
        case CG_PIPELINE_STATE_ALPHA_FUNC_INDEX:
            if (!_cg_pipeline_alpha_func_state_equal(authorities0[bit],
                                                     authorities1[bit]))
                goto done;
            break;
        case CG_PIPELINE_STATE_ALPHA_FUNC_REFERENCE_INDEX:
            if (!_cg_pipeline_alpha_func_reference_state_equal(
                    authorities0[bit], authorities1[bit]))
                goto done;
            break;
        case CG_PIPELINE_STATE_BLEND_INDEX:
            /* We don't need to compare the detailed blending state if we know
             * blending is disabled for both pipelines. */
            if (pipeline0->real_blend_enable) {
                if (!_cg_pipeline_blend_state_equal(authorities0[bit],
                                                    authorities1[bit]))
                    goto done;
            }
            break;
        case CG_PIPELINE_STATE_DEPTH_INDEX:
            if (!_cg_pipeline_depth_state_equal(authorities0[bit],
                                                authorities1[bit]))
                goto done;
            break;
        case CG_PIPELINE_STATE_CULL_FACE_INDEX:
            if (!_cg_pipeline_cull_face_state_equal(authorities0[bit],
                                                    authorities1[bit]))
                goto done;
            break;
        case CG_PIPELINE_STATE_NON_ZERO_POINT_SIZE_INDEX:
            if (!_cg_pipeline_non_zero_point_size_equal(authorities0[bit],
                                                        authorities1[bit]))
                goto done;
            break;
        case CG_PIPELINE_STATE_POINT_SIZE_INDEX:
            if (!_cg_pipeline_point_size_equal(authorities0[bit],
                                               authorities1[bit]))
                goto done;
            break;
        case CG_PIPELINE_STATE_PER_VERTEX_POINT_SIZE_INDEX:
            if (!_cg_pipeline_per_vertex_point_size_equal(authorities0[bit],
                                                          authorities1[bit]))
                goto done;
            break;
        case CG_PIPELINE_STATE_LOGIC_OPS_INDEX:
            if (!_cg_pipeline_logic_ops_state_equal(authorities0[bit],
                                                    authorities1[bit]))
                goto done;
            break;
        case CG_PIPELINE_STATE_UNIFORMS_INDEX:
            if (!_cg_pipeline_uniforms_state_equal(authorities0[bit],
                                                   authorities1[bit]))
                goto done;
            break;
        case CG_PIPELINE_STATE_VERTEX_SNIPPETS_INDEX:
            if (!_cg_pipeline_vertex_snippets_state_equal(authorities0[bit],
                                                          authorities1[bit]))
                goto done;
            break;
        case CG_PIPELINE_STATE_FRAGMENT_SNIPPETS_INDEX:
            if (!_cg_pipeline_fragment_snippets_state_equal(authorities0[bit],
                                                            authorities1[bit]))
                goto done;
            break;
        case CG_PIPELINE_STATE_LAYERS_INDEX: {
            if (!_cg_pipeline_layers_equal(authorities0[bit],
                                           authorities1[bit],
                                           layer_differences,
                                           flags))
                goto done;
            break;
        }

        case CG_PIPELINE_STATE_BLEND_ENABLE_INDEX:
        case CG_PIPELINE_STATE_REAL_BLEND_ENABLE_INDEX:
        case CG_PIPELINE_STATE_COUNT:
            c_warn_if_reached();
        }
    }
    CG_FLAGS_FOREACH_END;

    ret = true;
done:
    CG_TIMER_STOP(_cg_uprof_context, pipeline_equal_timer);
    return ret;
}

void
_cg_pipeline_prune_redundant_ancestry(cg_pipeline_t *pipeline)
{
    cg_pipeline_t *new_parent = _cg_pipeline_get_parent(pipeline);

    /* Before considering pruning redundant ancestry we check if this
     * pipeline is an authority for layer state and if so only consider
     * reparenting if it *owns* all the layers it depends on. NB: A
     * pipeline can be be a STATE_LAYERS authority but it may still
     * defer to its ancestors to define the state for some of its
     * layers.
     *
     * For example a pipeline that derives from a parent with 5 layers
     * can become a STATE_LAYERS authority by simply changing it's
     * ->n_layers count to 4 and in that case it can still defer to its
     * ancestors to define the state of those 4 layers.
     *
     * If a pipeline depends on any ancestors for layer state then we
     * immediatly bail out.
     */
    if (pipeline->differences & CG_PIPELINE_STATE_LAYERS) {
        if (pipeline->n_layers != c_llist_length(pipeline->layer_differences))
            return;
    }

    /* walk up past ancestors that are now redundant and potentially
     * reparent the pipeline. */
    while (_cg_pipeline_get_parent(new_parent) &&
           (new_parent->differences | pipeline->differences) ==
           pipeline->differences)
        new_parent = _cg_pipeline_get_parent(new_parent);

    if (new_parent != _cg_pipeline_get_parent(pipeline))
        _cg_pipeline_set_parent(pipeline, new_parent);
}

void
_cg_pipeline_update_authority(cg_pipeline_t *pipeline,
                              cg_pipeline_t *authority,
                              cg_pipeline_state_t state,
                              cg_pipeline_state_comparitor_t comparitor)
{
    /* If we are the current authority see if we can revert to one of
     * our ancestors being the authority */
    if (pipeline == authority && _cg_pipeline_get_parent(authority) != NULL) {
        cg_pipeline_t *parent = _cg_pipeline_get_parent(authority);
        cg_pipeline_t *old_authority =
            _cg_pipeline_get_authority(parent, state);

        if (comparitor(authority, old_authority))
            pipeline->differences &= ~state;
    } else if (pipeline != authority) {
        /* If we weren't previously the authority on this state then we
         * need to extended our differences mask and so it's possible
         * that some of our ancestry will now become redundant, so we
         * aim to reparent ourselves if that's true... */
        pipeline->differences |= state;
        _cg_pipeline_prune_redundant_ancestry(pipeline);
    }
}

unsigned long
_cg_pipeline_get_age(cg_pipeline_t *pipeline)
{
    c_return_val_if_fail(cg_is_pipeline(pipeline), 0);

    return pipeline->age;
}

void
cg_pipeline_remove_layer(cg_pipeline_t *pipeline, int layer_index)
{
    cg_pipeline_t *authority;
    cg_pipeline_layer_info_t layer_info;
    int i;

    c_return_if_fail(cg_is_pipeline(pipeline));

    authority = _cg_pipeline_get_authority(pipeline, CG_PIPELINE_STATE_LAYERS);

    /* The layer index of the layer we want info about */
    layer_info.layer_index = layer_index;

    /* This will be updated with a reference to the layer being removed
     * if it can be found. */
    layer_info.layer = NULL;

    /* This will be filled in with a list of layers that need to be
     * dropped down to a lower texture unit to fill the gap of the
     * removed layer. */
    layer_info.layers_to_shift =
        c_alloca(sizeof(cg_pipeline_layer_t *) * authority->n_layers);
    layer_info.n_layers_to_shift = 0;

    /* Unlike when we query layer info when adding a layer we must
     * always have a complete layers_to_shift list... */
    layer_info.ignore_shift_layers_if_found = false;

    _cg_pipeline_get_layer_info(authority, &layer_info);

    if (layer_info.layer == NULL)
        return;

    for (i = 0; i < layer_info.n_layers_to_shift; i++) {
        cg_pipeline_layer_t *shift_layer = layer_info.layers_to_shift[i];
        int unit_index = _cg_pipeline_layer_get_unit_index(shift_layer);
        _cg_pipeline_set_layer_unit(pipeline, shift_layer, unit_index - 1);
        /* NB: shift_layer may not be writeable so _set_layer_unit()
         * will allocate a derived layer internally which will become
         * owned by pipeline. Check the return value if we need to do
         * anything else with this layer. */
    }

    _cg_pipeline_remove_layer_difference(pipeline, layer_info.layer, true);
    _cg_pipeline_try_reverting_layers_authority(pipeline, NULL);

    pipeline->dirty_real_blend_enable = true;
}

int
cg_pipeline_get_n_layers(cg_pipeline_t *pipeline)
{
    cg_pipeline_t *authority;

    c_return_val_if_fail(cg_is_pipeline(pipeline), 0);

    authority = _cg_pipeline_get_authority(pipeline, CG_PIPELINE_STATE_LAYERS);

    return authority->n_layers;
}

void
_cg_pipeline_pre_paint_for_layer(cg_pipeline_t *pipeline, int layer_id)
{
    cg_pipeline_layer_t *layer = _cg_pipeline_get_layer(pipeline, layer_id);
    _cg_pipeline_layer_pre_paint(layer);
}

#ifdef CG_DEBUG_ENABLED
void
_cg_pipeline_set_static_breadcrumb(cg_pipeline_t *pipeline,
                                   const char *breadcrumb)
{
    pipeline->has_static_breadcrumb = true;
    pipeline->static_breadcrumb = breadcrumb;
}
#endif

typedef void (*Layerstate_hash_function_t)(cg_pipeline_layer_t *authority,
                                           cg_pipeline_layer_t **authorities,
                                           cg_pipeline_hash_state_t *state);

static Layerstate_hash_function_t layer_state_hash_functions
[CG_PIPELINE_LAYER_STATE_SPARSE_COUNT];

/* XXX: We don't statically initialize the array of hash functions, so
 * we won't get caught out by later re-indexing the groups for some
 * reason. */
void
_cg_pipeline_init_layer_state_hash_functions(void)
{
    cg_pipeline_layer_state_index_t _index;
    layer_state_hash_functions[CG_PIPELINE_LAYER_STATE_UNIT_INDEX] =
        _cg_pipeline_layer_hash_unit_state;
    layer_state_hash_functions[CG_PIPELINE_LAYER_STATE_TEXTURE_TYPE_INDEX] =
        _cg_pipeline_layer_hash_texture_type_state;
    layer_state_hash_functions[CG_PIPELINE_LAYER_STATE_TEXTURE_DATA_INDEX] =
        _cg_pipeline_layer_hash_texture_data_state;
    layer_state_hash_functions[CG_PIPELINE_LAYER_STATE_SAMPLER_INDEX] =
        _cg_pipeline_layer_hash_sampler_state;
    _index = CG_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS_INDEX;
    layer_state_hash_functions[_index] =
        _cg_pipeline_layer_hash_point_sprite_state;
    _index = CG_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS_INDEX;
    layer_state_hash_functions[_index] =
        _cg_pipeline_layer_hash_point_sprite_state;
    _index = CG_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS_INDEX;
    layer_state_hash_functions[_index] =
        _cg_pipeline_layer_hash_fragment_snippets_state;

    {
        /* So we get a big error if we forget to update this code! */
        _C_STATIC_ASSERT(CG_PIPELINE_LAYER_STATE_SPARSE_COUNT == 9,
                          "Don't forget to install a hash function for new "
                          "pipeline state and update assert at end of "
                          "_cg_pipeline_init_state_hash_functions");
    }
}

static bool
_cg_pipeline_hash_layer_cb(cg_pipeline_layer_t *layer,
                           void *user_data)
{
    cg_pipeline_hash_state_t *state = user_data;
    unsigned long differences = state->layer_differences;
    cg_pipeline_layer_t *authorities[CG_PIPELINE_LAYER_STATE_COUNT];
    unsigned long mask;
    int i;

    /* Theoretically we would hash non-sparse layer state here but
     * currently layers don't have any. */

    /* XXX: we resolve all the authorities here - not just those
     * corresponding to hash_state->layer_differences - because
     * the hashing of some state groups may depend on the state
     * of other groups.
     */
    mask = CG_PIPELINE_LAYER_STATE_ALL_SPARSE;
    _cg_pipeline_layer_resolve_authorities(layer, mask, authorities);

    /* So we go right ahead and hash the sparse state... */
    for (i = 0; i < CG_PIPELINE_LAYER_STATE_COUNT; i++) {
        unsigned long current_state = (1L << i);

        /* XXX: we are hashing the un-mixed hash values of all the
         * individual state groups; we should provide a means to test
         * the quality of the final hash values we are getting with this
         * approach... */
        if (differences & current_state) {
            cg_pipeline_layer_t *authority = authorities[i];
            layer_state_hash_functions[i](authority, authorities, state);
        }

        if (current_state > differences)
            break;
    }

    return true;
}

void
_cg_pipeline_hash_layers_state(cg_pipeline_t *authority,
                               cg_pipeline_hash_state_t *state)
{
    state->hash = _cg_util_one_at_a_time_hash(
        state->hash, &authority->n_layers, sizeof(authority->n_layers));
    _cg_pipeline_foreach_layer_internal(
        authority, _cg_pipeline_hash_layer_cb, state);
}

typedef void (*state_hash_function_t)(cg_pipeline_t *authority,
                                      cg_pipeline_hash_state_t *state);

static state_hash_function_t state_hash_functions
[CG_PIPELINE_STATE_SPARSE_COUNT];

/* We don't statically initialize the array of hash functions
 * so we won't get caught out by later re-indexing the groups for
 * some reason. */
void
_cg_pipeline_init_state_hash_functions(void)
{
    state_hash_functions[CG_PIPELINE_STATE_COLOR_INDEX] =
        _cg_pipeline_hash_color_state;
    state_hash_functions[CG_PIPELINE_STATE_BLEND_ENABLE_INDEX] =
        _cg_pipeline_hash_blend_enable_state;
    state_hash_functions[CG_PIPELINE_STATE_LAYERS_INDEX] =
        _cg_pipeline_hash_layers_state;
    state_hash_functions[CG_PIPELINE_STATE_ALPHA_FUNC_INDEX] =
        _cg_pipeline_hash_alpha_func_state;
    state_hash_functions[CG_PIPELINE_STATE_ALPHA_FUNC_REFERENCE_INDEX] =
        _cg_pipeline_hash_alpha_func_reference_state;
    state_hash_functions[CG_PIPELINE_STATE_BLEND_INDEX] =
        _cg_pipeline_hash_blend_state;
    state_hash_functions[CG_PIPELINE_STATE_DEPTH_INDEX] =
        _cg_pipeline_hash_depth_state;
    state_hash_functions[CG_PIPELINE_STATE_CULL_FACE_INDEX] =
        _cg_pipeline_hash_cull_face_state;
    state_hash_functions[CG_PIPELINE_STATE_NON_ZERO_POINT_SIZE_INDEX] =
        _cg_pipeline_hash_non_zero_point_size_state;
    state_hash_functions[CG_PIPELINE_STATE_POINT_SIZE_INDEX] =
        _cg_pipeline_hash_point_size_state;
    state_hash_functions[CG_PIPELINE_STATE_PER_VERTEX_POINT_SIZE_INDEX] =
        _cg_pipeline_hash_per_vertex_point_size_state;
    state_hash_functions[CG_PIPELINE_STATE_LOGIC_OPS_INDEX] =
        _cg_pipeline_hash_logic_ops_state;
    state_hash_functions[CG_PIPELINE_STATE_UNIFORMS_INDEX] =
        _cg_pipeline_hash_uniforms_state;
    state_hash_functions[CG_PIPELINE_STATE_VERTEX_SNIPPETS_INDEX] =
        _cg_pipeline_hash_vertex_snippets_state;
    state_hash_functions[CG_PIPELINE_STATE_FRAGMENT_SNIPPETS_INDEX] =
        _cg_pipeline_hash_fragment_snippets_state;

    {
        /* So we get a big error if we forget to update this code! */
        _C_STATIC_ASSERT(CG_PIPELINE_STATE_SPARSE_COUNT == 15,
                          "Make sure to install a hash function for "
                          "newly added pipeline state and update assert "
                          "in _cg_pipeline_init_state_hash_functions");
    }
}

unsigned int
_cg_pipeline_hash(cg_pipeline_t *pipeline,
                  unsigned int differences,
                  unsigned long layer_differences,
                  cg_pipeline_eval_flags_t flags)
{
    cg_pipeline_t *authorities[CG_PIPELINE_STATE_SPARSE_COUNT];
    unsigned int mask;
    int i;
    cg_pipeline_hash_state_t state;
    unsigned int final_hash = 0;

    state.hash = 0;
    state.layer_differences = layer_differences;
    state.flags = flags;

    _cg_pipeline_update_real_blend_enable(pipeline, false);

    /* hash non-sparse state */

    if (differences & CG_PIPELINE_STATE_REAL_BLEND_ENABLE) {
        bool enable = pipeline->real_blend_enable;
        state.hash =
            _cg_util_one_at_a_time_hash(state.hash, &enable, sizeof(enable));
    }

    /* hash sparse state */

    mask = differences & CG_PIPELINE_STATE_ALL_SPARSE;
    _cg_pipeline_resolve_authorities(pipeline, mask, authorities);

    for (i = 0; i < CG_PIPELINE_STATE_SPARSE_COUNT; i++) {
        unsigned int current_state = (1 << i);

        /* XXX: we are hashing the un-mixed hash values of all the
         * individual state groups; we should provide a means to test
         * the quality of the final hash values we are getting with this
         * approach... */
        if (differences & current_state) {
            cg_pipeline_t *authority = authorities[i];
            state_hash_functions[i](authority, &state);
            final_hash = _cg_util_one_at_a_time_hash(
                final_hash, &state.hash, sizeof(state.hash));
        }

        if (current_state > differences)
            break;
    }

    return _cg_util_one_at_a_time_mix(final_hash);
}

typedef struct {
    cg_device_t *dev;
    cg_pipeline_t *src_pipeline;
    cg_pipeline_t *dst_pipeline;
    unsigned int layer_differences;
} deep_copy_data_t;

static bool
deep_copy_layer_cb(cg_pipeline_layer_t *src_layer, void *user_data)
{
    deep_copy_data_t *data = user_data;
    cg_pipeline_layer_t *dst_layer;
    unsigned int differences = data->layer_differences;

    dst_layer = _cg_pipeline_get_layer(data->dst_pipeline, src_layer->index);

    while (src_layer != data->dev->default_layer_n &&
           src_layer != data->dev->default_layer_0 && differences) {
        unsigned long to_copy = differences & src_layer->differences;

        if (to_copy) {
            _cg_pipeline_layer_copy_differences(dst_layer, src_layer, to_copy);
            differences ^= to_copy;
        }

        src_layer = CG_PIPELINE_LAYER(CG_NODE(src_layer)->parent);
    }

    return true;
}

cg_pipeline_t *
_cg_pipeline_deep_copy(cg_device_t *dev,
                       cg_pipeline_t *pipeline,
                       unsigned long differences,
                       unsigned long layer_differences)
{
    cg_pipeline_t *new, *authority;
    bool copy_layer_state;

    if ((differences & CG_PIPELINE_STATE_LAYERS)) {
        copy_layer_state = true;
        differences &= ~CG_PIPELINE_STATE_LAYERS;
    } else
        copy_layer_state = false;

    new = cg_pipeline_new(dev);

    for (authority = pipeline;
         authority != dev->default_pipeline && differences;
         authority = CG_PIPELINE(CG_NODE(authority)->parent)) {
        unsigned long to_copy = differences & authority->differences;

        if (to_copy) {
            _cg_pipeline_copy_differences(new, authority, to_copy);
            differences ^= to_copy;
        }
    }

    if (copy_layer_state) {
        deep_copy_data_t data;

        /* The unit index doesn't need to be copied because it should
         * end up with the same values anyway because the new pipeline
         * will have the same indices as the source pipeline */
        layer_differences &= ~CG_PIPELINE_LAYER_STATE_UNIT;

        data.dev = dev;
        data.src_pipeline = pipeline;
        data.dst_pipeline = new;
        data.layer_differences = layer_differences;

        _cg_pipeline_foreach_layer_internal(
            pipeline, deep_copy_layer_cb, &data);
    }

    return new;
}

typedef struct {
    int i;
    cg_pipeline_layer_t **layers;
} add_layers_to_array_state_t;

static bool
add_layer_to_array_cb(cg_pipeline_layer_t *layer, void *user_data)
{
    add_layers_to_array_state_t *state = user_data;
    state->layers[state->i++] = layer;
    return true;
}

/* This tries to find the oldest ancestor whose pipeline and layer
   state matches the given flags. This is mostly used to detect code
   gen authorities so that we can reduce the numer of programs
   generated */
cg_pipeline_t *
_cg_pipeline_find_equivalent_parent(cg_pipeline_t *pipeline,
                                    cg_pipeline_state_t pipeline_state,
                                    cg_pipeline_layer_state_t layer_state)
{
    cg_pipeline_t *authority0;
    cg_pipeline_t *authority1;
    int n_layers;
    cg_pipeline_layer_t **authority0_layers;
    cg_pipeline_layer_t **authority1_layers;

    /* Find the first pipeline that modifies state that affects the
     * state or any layer state... */
    authority0 = _cg_pipeline_get_authority(
        pipeline, pipeline_state | CG_PIPELINE_STATE_LAYERS);

    /* Find the next ancestor after that, that also modifies the
     * state... */
    if (_cg_pipeline_get_parent(authority0)) {
        authority1 = _cg_pipeline_get_authority(
            _cg_pipeline_get_parent(authority0),
            pipeline_state | CG_PIPELINE_STATE_LAYERS);
    } else
        return authority0;

    n_layers = cg_pipeline_get_n_layers(authority0);

    for (;; ) {
        add_layers_to_array_state_t state;
        int i;

        if (n_layers != cg_pipeline_get_n_layers(authority1))
            return authority0;

        /* If the programs differ by anything that isn't part of the
           layer state then we can't continue */
        if (pipeline_state &&
            (_cg_pipeline_compare_differences(authority0, authority1) &
             pipeline_state))
            return authority0;

        authority0_layers = c_alloca(sizeof(cg_pipeline_layer_t *) * n_layers);
        state.i = 0;
        state.layers = authority0_layers;
        _cg_pipeline_foreach_layer_internal(
            authority0, add_layer_to_array_cb, &state);

        authority1_layers = c_alloca(sizeof(cg_pipeline_layer_t *) * n_layers);
        state.i = 0;
        state.layers = authority1_layers;
        _cg_pipeline_foreach_layer_internal(
            authority1, add_layer_to_array_cb, &state);

        for (i = 0; i < n_layers; i++) {
            unsigned long layer_differences;

            if (authority0_layers[i] == authority1_layers[i])
                continue;

            layer_differences = _cg_pipeline_layer_compare_differences(
                authority0_layers[i], authority1_layers[i]);

            if (layer_differences & layer_state)
                return authority0;
        }

        /* Find the next ancestor after that, that also modifies state
         * affecting codegen... */

        if (!_cg_pipeline_get_parent(authority1))
            break;

        authority0 = authority1;
        authority1 = _cg_pipeline_get_authority(
            _cg_pipeline_get_parent(authority1),
            pipeline_state | CG_PIPELINE_STATE_LAYERS);
        if (authority1 == authority0)
            break;
    }

    return authority1;
}

cg_pipeline_state_t
_cg_pipeline_get_state_for_vertex_codegen(cg_device_t *dev)
{
    cg_pipeline_state_t state =
        (CG_PIPELINE_STATE_LAYERS | CG_PIPELINE_STATE_PER_VERTEX_POINT_SIZE |
         CG_PIPELINE_STATE_VERTEX_SNIPPETS);

    /* If we don't have the builtin point size uniform then we'll add
     * one in the GLSL but we'll only do this if the point size is
     * non-zero. Whether or not the point size is zero is represented by
     * CG_PIPELINE_STATE_NON_ZERO_POINT_SIZE */
    if (!_cg_has_private_feature(dev,
                                 CG_PRIVATE_FEATURE_BUILTIN_POINT_SIZE_UNIFORM))
        state |= CG_PIPELINE_STATE_NON_ZERO_POINT_SIZE;

    return state;
}

cg_pipeline_layer_state_t
_cg_pipeline_get_layer_state_for_fragment_codegen(cg_device_t *dev)
{
    cg_pipeline_layer_state_t state =
        (CG_PIPELINE_LAYER_STATE_TEXTURE_TYPE | CG_PIPELINE_LAYER_STATE_UNIT |
         CG_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS);

    /* If the driver supports GLSL then we might be using gl_PointCoord
     * to implement the sprite coords. In that case the generated code
     * depends on the point sprite state */
    if (cg_has_feature(dev, CG_FEATURE_ID_GLSL))
        state |= CG_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS;

    return state;
}

cg_pipeline_state_t
_cg_pipeline_get_state_for_fragment_codegen(cg_device_t *dev)
{
    return (CG_PIPELINE_STATE_LAYERS | CG_PIPELINE_STATE_FRAGMENT_SNIPPETS |
            CG_PIPELINE_STATE_ALPHA_FUNC);
}

int
cg_pipeline_get_uniform_location(cg_pipeline_t *pipeline,
                                 const char *uniform_name)
{
    void *location_ptr;
    char *uniform_name_copy;

    _CG_GET_DEVICE(dev, -1);

    /* This API is designed as if the uniform locations are specific to
       a pipeline but they are actually unique across a whole
       cg_device_t. Potentially this could just be
       cg_device_get_uniform_location but it seems to make sense to
       keep the API this way so that we can change the internals if need
       be. */

    /* Look for an existing uniform with this name */
    if (c_hash_table_lookup_extended(
            dev->uniform_name_hash, uniform_name, NULL, &location_ptr))
        return C_POINTER_TO_INT(location_ptr);

    uniform_name_copy = c_strdup(uniform_name);
    c_ptr_array_add(dev->uniform_names, uniform_name_copy);
    c_hash_table_insert(dev->uniform_name_hash,
                        uniform_name_copy,
                        C_INT_TO_POINTER(dev->n_uniform_names));

    return dev->n_uniform_names++;
}
