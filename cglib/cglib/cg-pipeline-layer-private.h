/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009,2010,2011 Intel Corporation.
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

#ifndef __CG_PIPELINE_LAYER_PRIVATE_H
#define __CG_PIPELINE_LAYER_PRIVATE_H

#include "cg-private.h"
#include "cg-pipeline.h"
#include "cg-node-private.h"
#include "cg-texture.h"
#include "cg-pipeline-layer-state.h"
#include "cg-pipeline-snippet-private.h"
#include "cg-sampler-cache-private.h"

#include <clib.h>

typedef struct _cg_pipeline_layer_t cg_pipeline_layer_t;
#define CG_PIPELINE_LAYER(OBJECT) ((cg_pipeline_layer_t *)OBJECT)

/* XXX: should I rename these as
 * CG_PIPELINE_LAYER_STATE_INDEX_XYZ... ?
 */
typedef enum {
    /* sparse state */
    CG_PIPELINE_LAYER_STATE_UNIT_INDEX,
    CG_PIPELINE_LAYER_STATE_TEXTURE_TYPE_INDEX,
    CG_PIPELINE_LAYER_STATE_TEXTURE_DATA_INDEX,
    CG_PIPELINE_LAYER_STATE_SAMPLER_INDEX,
    CG_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS_INDEX,
    CG_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS_INDEX,
    CG_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS_INDEX,

    /* note: layers don't currently have any non-sparse state */
    CG_PIPELINE_LAYER_STATE_SPARSE_COUNT,
    CG_PIPELINE_LAYER_STATE_COUNT = CG_PIPELINE_LAYER_STATE_SPARSE_COUNT
} cg_pipeline_layer_state_index_t;

/* XXX: If you add or remove state groups here you may need to update
 * some of the state masks following this enum too!
 *
 * FIXME: perhaps it would be better to rename this enum to
 * cg_pipeline_layer_state_group_t to better convey the fact that a single
 * enum here can map to multiple properties.
 */
typedef enum {
    CG_PIPELINE_LAYER_STATE_UNIT = 1L << CG_PIPELINE_LAYER_STATE_UNIT_INDEX,
    CG_PIPELINE_LAYER_STATE_TEXTURE_TYPE =
        1L << CG_PIPELINE_LAYER_STATE_TEXTURE_TYPE_INDEX,
    CG_PIPELINE_LAYER_STATE_TEXTURE_DATA =
        1L << CG_PIPELINE_LAYER_STATE_TEXTURE_DATA_INDEX,
    CG_PIPELINE_LAYER_STATE_SAMPLER =
        1L << CG_PIPELINE_LAYER_STATE_SAMPLER_INDEX,
    CG_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS =
        1L << CG_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS_INDEX,
    CG_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS =
        1L << CG_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS_INDEX,
    CG_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS =
        1L << CG_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS_INDEX,

    /* CG_PIPELINE_LAYER_STATE_TEXTURE_INTERN   = 1L<<8, */
} cg_pipeline_layer_state_t;

/*
 * Various special masks that tag state-groups in different ways...
 */

#define CG_PIPELINE_LAYER_STATE_ALL ((1L << CG_PIPELINE_LAYER_STATE_COUNT) - 1)

#define CG_PIPELINE_LAYER_STATE_ALL_SPARSE CG_PIPELINE_LAYER_STATE_ALL

#define CG_PIPELINE_LAYER_STATE_NEEDS_BIG_STATE                                \
    (CG_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS |                             \
     CG_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS |                                 \
     CG_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS)

#define CG_PIPELINE_LAYER_STATE_MULTI_PROPERTY                                 \
    (CG_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS |                                 \
     CG_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS)

#define CG_PIPELINE_LAYER_STATE_AFFECTS_VERTEX_CODEGEN                         \
    CG_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS

typedef struct {
    cg_pipeline_snippet_list_t vertex_snippets;
    cg_pipeline_snippet_list_t fragment_snippets;

    bool point_sprite_coords;
} cg_pipeline_layer_big_state_t;

struct _cg_pipeline_layer_t {
    /* XXX: Please think twice about adding members that *have* be
     * initialized during a _cg_pipeline_layer_copy. We are aiming
     * to have copies be as cheap as possible and copies may be
     * done by the primitives APIs which means they may happen
     * in performance critical code paths.
     *
     * XXX: If you are extending the state we track please consider if
     * the state is expected to vary frequently across many pipelines or
     * if the state can be shared among many derived pipelines instead.
     * This will determine if the state should be added directly to this
     * structure which will increase the memory overhead for *all*
     * layers or if instead it can go under ->big_state.
     */

    /* Layers represent their state in a tree structure where some of
     * the state relating to a given pipeline or layer may actually be
     * owned by one if is ancestors in the tree. We have a common data
     * type to track the tree heirachy so we can share code... */
    cg_node_t _parent;

    /* Some layers have a pipeline owner, which is to say that the layer
     * is referenced in that pipelines->layer_differences list.  A layer
     * doesn't always have an owner and may simply be an ancestor for
     * other layers that keeps track of some shared state. */
    cg_pipeline_t *owner;

    /* The lowest index is blended first then others on top */
    int index;

    /* A mask of which state groups are different in this layer
     * in comparison to its parent. */
    unsigned int differences;

    /* Common differences
     *
     * As a basic way to reduce memory usage we divide the layer
     * state into two groups; the minimal state modified in 90% of
     * all layers and the rest, so that the second group can
     * be allocated dynamically when required.
     */

    /* Each layer is directly associated with a single texture unit */
    int unit_index;

    /* The type of the texture. This is always set even if the texture
       is NULL and it will be used to determine what type of texture
       lookups to use in any shaders generated by the pipeline
       backends. */
    cg_texture_type_t texture_type;
    /* The texture for this layer, or NULL for an empty
     * layer */
    cg_texture_t *texture;

    const cg_sampler_cache_entry_t *sampler_cache_entry;

    /* Infrequent differences aren't currently tracked in
     * a separate, dynamically allocated structure as they are
     * for pipelines... */
    cg_pipeline_layer_big_state_t *big_state;

    /* bitfields */

    /* Determines if layer->big_state is valid */
    unsigned int has_big_state : 1;
};

typedef bool (*cg_pipeline_layer_state_comparitor_t)(
    cg_pipeline_layer_t *authority0, cg_pipeline_layer_t *authority1);

void _cg_pipeline_init_default_layers(cg_device_t *dev);

static inline cg_pipeline_layer_t *
_cg_pipeline_layer_get_parent(cg_pipeline_layer_t *layer)
{
    cg_node_t *parent_node = CG_NODE(layer)->parent;
    return CG_PIPELINE_LAYER(parent_node);
}

cg_pipeline_layer_t *_cg_pipeline_layer_copy(cg_pipeline_layer_t *layer);

void _cg_pipeline_layer_resolve_authorities(cg_pipeline_layer_t *layer,
                                            unsigned long differences,
                                            cg_pipeline_layer_t **authorities);

bool _cg_pipeline_layer_equal(cg_pipeline_layer_t *layer0,
                              cg_pipeline_layer_t *layer1,
                              unsigned long differences_mask,
                              cg_pipeline_eval_flags_t flags);

cg_pipeline_layer_t *
_cg_pipeline_layer_pre_change_notify(cg_pipeline_t *required_owner,
                                     cg_pipeline_layer_t *layer,
                                     cg_pipeline_layer_state_t change);

void _cg_pipeline_layer_prune_redundant_ancestry(cg_pipeline_layer_t *layer);

bool _cg_pipeline_layer_has_alpha(cg_pipeline_layer_t *layer);

/*
 * Calls the pre_paint method on the layer texture if there is
 * one. This will determine whether mipmaps are needed based on the
 * filter settings.
 */
void _cg_pipeline_layer_pre_paint(cg_pipeline_layer_t *layerr);

void
_cg_pipeline_layer_get_wrap_modes(cg_pipeline_layer_t *layer,
                                  cg_sampler_cache_wrap_mode_t *wrap_mode_s,
                                  cg_sampler_cache_wrap_mode_t *wrap_mode_t,
                                  cg_sampler_cache_wrap_mode_t *wrap_mode_r);

void _cg_pipeline_layer_get_filters(cg_pipeline_layer_t *layer,
                                    cg_pipeline_filter_t *min_filter,
                                    cg_pipeline_filter_t *mag_filter);

const cg_sampler_cache_entry_t *
_cg_pipeline_layer_get_sampler_state(cg_pipeline_layer_t *layer);

void _cg_pipeline_get_layer_filters(cg_pipeline_t *pipeline,
                                    int layer_index,
                                    cg_pipeline_filter_t *min_filter,
                                    cg_pipeline_filter_t *mag_filter);

typedef enum {
    CG_PIPELINE_LAYER_TYPE_TEXTURE
} cg_pipeline_layer_type_t;

cg_pipeline_layer_type_t
_cg_pipeline_layer_get_type(cg_pipeline_layer_t *layer);

cg_texture_t *_cg_pipeline_layer_get_texture(cg_pipeline_layer_t *layer);

cg_texture_t *_cg_pipeline_layer_get_texture_real(cg_pipeline_layer_t *layer);

cg_texture_type_t
_cg_pipeline_layer_get_texture_type(cg_pipeline_layer_t *layer);

cg_pipeline_filter_t
_cg_pipeline_layer_get_min_filter(cg_pipeline_layer_t *layer);

cg_pipeline_filter_t
_cg_pipeline_layer_get_mag_filter(cg_pipeline_layer_t *layer);

cg_pipeline_wrap_mode_t
_cg_pipeline_layer_get_wrap_mode_s(cg_pipeline_layer_t *layer);

cg_pipeline_wrap_mode_t
_cg_pipeline_layer_get_wrap_mode_t(cg_pipeline_layer_t *layer);

cg_pipeline_wrap_mode_t
_cg_pipeline_layer_get_wrap_mode_p(cg_pipeline_layer_t *layer);

void _cg_pipeline_layer_copy_differences(cg_pipeline_layer_t *dest,
                                         cg_pipeline_layer_t *src,
                                         unsigned long differences);

unsigned long
_cg_pipeline_layer_compare_differences(cg_pipeline_layer_t *layer0,
                                       cg_pipeline_layer_t *layer1);

cg_pipeline_layer_t *
_cg_pipeline_layer_get_authority(cg_pipeline_layer_t *layer,
                                 unsigned long difference);

cg_texture_t *_cg_pipeline_layer_get_texture(cg_pipeline_layer_t *layer);

int _cg_pipeline_layer_get_unit_index(cg_pipeline_layer_t *layer);

bool _cg_pipeline_layer_needs_combine_separate(
    cg_pipeline_layer_t *combine_authority);

#endif /* __CG_PIPELINE_LAYER_PRIVATE_H */
