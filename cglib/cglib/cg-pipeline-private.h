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

#ifndef __CG_PIPELINE_PRIVATE_H
#define __CG_PIPELINE_PRIVATE_H

#include <clib.h>

#include "cg-node-private.h"
#include "cg-pipeline-layer-private.h"
#include "cg-pipeline.h"
#include "cg-object-private.h"
#include "cg-profile.h"
#include "cg-boxed-value.h"
#include "cg-pipeline-snippet-private.h"
#include "cg-pipeline-state.h"
#include "cg-framebuffer.h"
#include "cg-bitmask.h"

#define CG_PIPELINE_PROGEND_GLSL 0
#define CG_PIPELINE_PROGEND_NOP 1
#define CG_PIPELINE_N_PROGENDS 2

#define CG_PIPELINE_VERTEND_GLSL 0
#define CG_PIPELINE_VERTEND_NOP 1
#define CG_PIPELINE_N_VERTENDS 2

#define CG_PIPELINE_FRAGEND_GLSL 0
#define CG_PIPELINE_FRAGEND_NOP 1
#define CG_PIPELINE_N_FRAGENDS 2

#define CG_PIPELINE_PROGEND_DEFAULT 0
#define CG_PIPELINE_PROGEND_UNDEFINED 3

/* XXX: should I rename these as
 * CG_PIPELINE_STATE_INDEX_XYZ... ?
 */
typedef enum {
    /* sparse state */
    CG_PIPELINE_STATE_COLOR_INDEX,
    CG_PIPELINE_STATE_BLEND_ENABLE_INDEX,
    CG_PIPELINE_STATE_LAYERS_INDEX,
    CG_PIPELINE_STATE_ALPHA_FUNC_INDEX,
    CG_PIPELINE_STATE_ALPHA_FUNC_REFERENCE_INDEX,
    CG_PIPELINE_STATE_BLEND_INDEX,
    CG_PIPELINE_STATE_DEPTH_INDEX,
    CG_PIPELINE_STATE_NON_ZERO_POINT_SIZE_INDEX,
    CG_PIPELINE_STATE_POINT_SIZE_INDEX,
    CG_PIPELINE_STATE_PER_VERTEX_POINT_SIZE_INDEX,
    CG_PIPELINE_STATE_LOGIC_OPS_INDEX,
    CG_PIPELINE_STATE_CULL_FACE_INDEX,
    CG_PIPELINE_STATE_UNIFORMS_INDEX,
    CG_PIPELINE_STATE_VERTEX_SNIPPETS_INDEX,
    CG_PIPELINE_STATE_FRAGMENT_SNIPPETS_INDEX,

    /* non-sparse */
    CG_PIPELINE_STATE_REAL_BLEND_ENABLE_INDEX,
    CG_PIPELINE_STATE_COUNT
} cg_pipeline_state_index_t;

#define CG_PIPELINE_STATE_SPARSE_COUNT (CG_PIPELINE_STATE_COUNT - 1)

/* Used in pipeline->differences masks and for notifying pipeline
 * state changes.
 *
 * XXX: If you add or remove state groups here you may need to update
 * some of the state masks following this enum too!
 *
 * FIXME: perhaps it would be better to rename this enum to
 * cg_pipeline_state_group_t to better convey the fact that a single enum
 * here can map to multiple properties.
 */
typedef enum _cg_pipeline_state_t {
    CG_PIPELINE_STATE_COLOR = 1L << CG_PIPELINE_STATE_COLOR_INDEX,
    CG_PIPELINE_STATE_BLEND_ENABLE = 1L << CG_PIPELINE_STATE_BLEND_ENABLE_INDEX,
    CG_PIPELINE_STATE_LAYERS = 1L << CG_PIPELINE_STATE_LAYERS_INDEX,
    CG_PIPELINE_STATE_ALPHA_FUNC = 1L << CG_PIPELINE_STATE_ALPHA_FUNC_INDEX,
    CG_PIPELINE_STATE_ALPHA_FUNC_REFERENCE =
        1L << CG_PIPELINE_STATE_ALPHA_FUNC_REFERENCE_INDEX,
    CG_PIPELINE_STATE_BLEND = 1L << CG_PIPELINE_STATE_BLEND_INDEX,
    CG_PIPELINE_STATE_DEPTH = 1L << CG_PIPELINE_STATE_DEPTH_INDEX,
    CG_PIPELINE_STATE_NON_ZERO_POINT_SIZE =
        1L << CG_PIPELINE_STATE_NON_ZERO_POINT_SIZE_INDEX,
    CG_PIPELINE_STATE_POINT_SIZE = 1L << CG_PIPELINE_STATE_POINT_SIZE_INDEX,
    CG_PIPELINE_STATE_PER_VERTEX_POINT_SIZE =
        1L << CG_PIPELINE_STATE_PER_VERTEX_POINT_SIZE_INDEX,
    CG_PIPELINE_STATE_LOGIC_OPS = 1L << CG_PIPELINE_STATE_LOGIC_OPS_INDEX,
    CG_PIPELINE_STATE_CULL_FACE = 1L << CG_PIPELINE_STATE_CULL_FACE_INDEX,
    CG_PIPELINE_STATE_UNIFORMS = 1L << CG_PIPELINE_STATE_UNIFORMS_INDEX,
    CG_PIPELINE_STATE_VERTEX_SNIPPETS =
        1L << CG_PIPELINE_STATE_VERTEX_SNIPPETS_INDEX,
    CG_PIPELINE_STATE_FRAGMENT_SNIPPETS =
        1L << CG_PIPELINE_STATE_FRAGMENT_SNIPPETS_INDEX,
    CG_PIPELINE_STATE_REAL_BLEND_ENABLE =
        1L << CG_PIPELINE_STATE_REAL_BLEND_ENABLE_INDEX,
} cg_pipeline_state_t;

/*
 * Various special masks that tag state-groups in different ways...
 */

#define CG_PIPELINE_STATE_ALL ((1L << CG_PIPELINE_STATE_COUNT) - 1)

#define CG_PIPELINE_STATE_ALL_SPARSE                                           \
    (CG_PIPELINE_STATE_ALL & ~CG_PIPELINE_STATE_REAL_BLEND_ENABLE)

#define CG_PIPELINE_STATE_AFFECTS_BLENDING                                     \
    (CG_PIPELINE_STATE_COLOR | CG_PIPELINE_STATE_BLEND_ENABLE |                \
     CG_PIPELINE_STATE_LAYERS | CG_PIPELINE_STATE_BLEND |                      \
     CG_PIPELINE_STATE_VERTEX_SNIPPETS | CG_PIPELINE_STATE_FRAGMENT_SNIPPETS)

#define CG_PIPELINE_STATE_NEEDS_BIG_STATE                                      \
    (CG_PIPELINE_STATE_ALPHA_FUNC | CG_PIPELINE_STATE_ALPHA_FUNC_REFERENCE |   \
     CG_PIPELINE_STATE_BLEND | CG_PIPELINE_STATE_DEPTH |                       \
     CG_PIPELINE_STATE_NON_ZERO_POINT_SIZE | CG_PIPELINE_STATE_POINT_SIZE |    \
     CG_PIPELINE_STATE_PER_VERTEX_POINT_SIZE | CG_PIPELINE_STATE_LOGIC_OPS |   \
     CG_PIPELINE_STATE_CULL_FACE | CG_PIPELINE_STATE_UNIFORMS |                \
     CG_PIPELINE_STATE_VERTEX_SNIPPETS | CG_PIPELINE_STATE_FRAGMENT_SNIPPETS)

#define CG_PIPELINE_STATE_MULTI_PROPERTY                                       \
    (CG_PIPELINE_STATE_LAYERS | CG_PIPELINE_STATE_BLEND |                      \
     CG_PIPELINE_STATE_DEPTH | CG_PIPELINE_STATE_LOGIC_OPS |                   \
     CG_PIPELINE_STATE_CULL_FACE | CG_PIPELINE_STATE_UNIFORMS |                \
     CG_PIPELINE_STATE_VERTEX_SNIPPETS | CG_PIPELINE_STATE_FRAGMENT_SNIPPETS)

typedef struct {
    /* Determines what fragments are discarded based on their alpha */
    cg_pipeline_alpha_func_t alpha_func;
    float alpha_func_reference;
} cg_pipeline_alpha_func_state_t;

typedef enum _cg_pipeline_blend_enable_t {
    /* XXX: we want to detect users mistakenly using true or false
     * so start the enum at 2. */
    CG_PIPELINE_BLEND_ENABLE_ENABLED = 2,
    CG_PIPELINE_BLEND_ENABLE_DISABLED,
    CG_PIPELINE_BLEND_ENABLE_AUTOMATIC
} cg_pipeline_blend_enable_t;

typedef struct {
/* Determines how this pipeline is blended with other primitives */
#if defined(CG_HAS_GLES2_SUPPORT) || defined(CG_HAS_GL_SUPPORT)
    GLenum blend_equation_rgb;
    GLenum blend_equation_alpha;
    GLint blend_src_factor_alpha;
    GLint blend_dst_factor_alpha;
    cg_color_t blend_constant;
#endif
    GLint blend_src_factor_rgb;
    GLint blend_dst_factor_rgb;
} cg_pipeline_blend_state_t;

typedef struct {
    cg_color_mask_t color_mask;
} cg_pipeline_logic_ops_state_t;

typedef struct {
    cg_pipeline_cull_face_mode_t mode;
    cg_winding_t front_winding;
} cg_pipeline_cull_face_state_t;

typedef struct {
    CGlibBitmask override_mask;

    /* This is an array of values. Only the uniforms that have a bit set
       in override_mask have a corresponding value here. The uniform's
       location is implicit from the order in this array */
    cg_boxed_value_t *override_values;

    /* Uniforms that have been modified since this pipeline was last
       flushed */
    CGlibBitmask changed_mask;
} cg_pipeline_uniforms_state_t;

typedef struct {
    cg_pipeline_alpha_func_state_t alpha_state;
    cg_pipeline_blend_state_t blend_state;
    cg_depth_state_t depth_state;
    float point_size;
    unsigned int non_zero_point_size : 1;
    unsigned int per_vertex_point_size : 1;
    cg_pipeline_logic_ops_state_t logic_ops_state;
    cg_pipeline_cull_face_state_t cull_face_state;
    cg_pipeline_uniforms_state_t uniforms_state;
    cg_pipeline_snippet_list_t vertex_snippets;
    cg_pipeline_snippet_list_t fragment_snippets;
} cg_pipeline_big_state_t;

typedef struct {
    cg_pipeline_t *owner;
    cg_pipeline_layer_t *layer;
} cg_pipeline_layer_cache_entry_t;

typedef struct _cg_pipeline_hash_state_t {
    unsigned long layer_differences;
    cg_pipeline_eval_flags_t flags;
    unsigned int hash;
} cg_pipeline_hash_state_t;

struct _cg_pipeline_t {
    /* XXX: Please think twice about adding members that *have* be
     * initialized during a cg_pipeline_copy. We are aiming to have
     * copies be as cheap as possible and copies may be done by the
     * primitives APIs which means they may happen in performance
     * critical code paths.
     *
     * XXX: If you are extending the state we track please consider if
     * the state is expected to vary frequently across many pipelines or
     * if the state can be shared among many derived pipelines instead.
     * This will determine if the state should be added directly to this
     * structure which will increase the memory overhead for *all*
     * pipelines or if instead it can go under ->big_state.
     */

    /* Layers represent their state in a tree structure where some of
     * the state relating to a given pipeline or layer may actually be
     * owned by one if is ancestors in the tree. We have a common data
     * type to track the tree heirachy so we can share code... */
    cg_node_t _parent;

    /* A mask of which sparse state groups are different in this
     * pipeline in comparison to its parent. */
    unsigned int differences;

    /* Whenever a pipeline is modified we increment the age. There's no
     * guarantee that it won't wrap but it can nevertheless be a
     * convenient mechanism to determine when a pipeline has been
     * changed to you can invalidate some some associated cache that
     * depends on the old state. */
    unsigned int age;

    /* This is the primary color of the pipeline.
     *
     * This is a sparse property, ref CG_PIPELINE_STATE_COLOR */
    cg_color_t color;

    /* A pipeline may be made up with multiple layers used to combine
     * textures together.
     *
     * This is sparse state, ref CG_PIPELINE_STATE_LAYERS */
    unsigned int n_layers;
    c_llist_t *layer_differences;

    /* As a basic way to reduce memory usage we divide the pipeline
     * state into two groups; the minimal state modified in 90% of
     * all pipelines and the rest, so that the second group can
     * be allocated dynamically when required... */
    cg_pipeline_big_state_t *big_state;

#ifdef CG_DEBUG_ENABLED
    /* For debugging purposes it's possible to associate a static const
     * string with a pipeline which can be an aid when trying to trace
     * where the pipeline originates from */
    const char *static_breadcrumb;
#endif

    /* Cached state... */

    /* A cached, complete list of the layers this pipeline depends
     * on sorted by layer->unit_index. */
    cg_pipeline_layer_t **layers_cache;
    /* To avoid a separate ->layers_cache allocation for common
     * pipelines with only a few layers... */
    cg_pipeline_layer_t *short_layers_cache[3];

    /* XXX: consider adding an authorities cache to speed up sparse
     * property value lookups:
     * cg_pipeline_t *authorities_cache[CG_PIPELINE_N_SPARSE_PROPERTIES];
     * and corresponding authorities_cache_dirty:1 bitfield
     */

    /* bitfields */

    /* Determines if pipeline->big_state is valid */
    unsigned int has_big_state : 1;

    /* By default blending is enabled automatically depending on the
     * unlit color, the lighting colors or the texture format. The user
     * can override this to explicitly enable or disable blending.
     *
     * This is a sparse property */
    unsigned int blend_enable : 3;

    /* There are many factors that can determine if we need to enable
     * blending, this holds our final decision */
    unsigned int real_blend_enable : 1;

    /* Since the code for deciding if blending really needs to be
     * enabled for a particular pipeline is quite expensive we update
     * the real_blend_enable flag lazily when flushing a pipeline if
     * this dirty flag has been set. */
    unsigned int dirty_real_blend_enable : 1;

    /* Whenever a pipeline is flushed we keep track of whether the
     * pipeline was used with a color attribute where we don't know
     * whether the colors are opaque. The real_blend_enable state
     * depends on this, and must be updated whenever this changes (even
     * if dirty_real_blend_enable isn't set) */
    unsigned int unknown_color_alpha : 1;

    unsigned int layers_cache_dirty : 1;

#ifdef CG_DEBUG_ENABLED
    /* For debugging purposes it's possible to associate a static const
     * string with a pipeline which can be an aid when trying to trace
     * where the pipeline originates from */
    unsigned int has_static_breadcrumb : 1;
#endif

    /* There are multiple fragment and vertex processing backends for
     * cg_pipeline_t that are bundled under a "progend". This identifies
     * the backend being used for the pipeline. */
    unsigned int progend : 3;

    /* We are moving towards CGlib considering pipelines to be
     * immutable once they get used so we can remove the
     * copy-on-write complexity. For now this is just used
     * for debugging... */
    unsigned int immutable : 1;
};

typedef struct _cg_pipeline_fragend_t {
    void (*start)(cg_device_t *dev,
                  cg_pipeline_t *pipeline,
                  int n_layers,
                  unsigned long pipelines_difference);
    bool (*add_layer)(cg_device_t *dev,
                      cg_pipeline_t *pipeline,
                      cg_pipeline_layer_t *layer,
                      unsigned long layers_difference);
    bool (*end)(cg_device_t *dev,
                cg_pipeline_t *pipeline, unsigned long pipelines_difference);

    void (*pipeline_pre_change_notify)(cg_device_t *dev,
                                       cg_pipeline_t *pipeline,
                                       cg_pipeline_state_t change,
                                       const cg_color_t *new_color);
    void (*layer_pre_change_notify)(cg_device_t *dev,
                                    cg_pipeline_t *owner,
                                    cg_pipeline_layer_t *layer,
                                    cg_pipeline_layer_state_t change);
} cg_pipeline_fragend_t;

typedef struct _cg_pipeline_vertend_t {
    void (*start)(cg_device_t *dev,
                  cg_pipeline_t *pipeline,
                  int n_layers,
                  unsigned long pipelines_difference);
    bool (*add_layer)(cg_device_t *dev,
                      cg_pipeline_t *pipeline,
                      cg_pipeline_layer_t *layer,
                      unsigned long layers_difference,
                      cg_framebuffer_t *framebuffer);
    bool (*end)(cg_device_t *dev,
                cg_pipeline_t *pipeline, unsigned long pipelines_difference);

    void (*pipeline_pre_change_notify)(cg_device_t *dev,
                                       cg_pipeline_t *pipeline,
                                       cg_pipeline_state_t change,
                                       const cg_color_t *new_color);
    void (*layer_pre_change_notify)(cg_device_t *dev,
                                    cg_pipeline_t *owner,
                                    cg_pipeline_layer_t *layer,
                                    cg_pipeline_layer_state_t change);
} cg_pipeline_vertend_t;

typedef struct {
    int vertend;
    int fragend;
    bool (*start)(cg_device_t *dev, cg_pipeline_t *pipeline);
    void (*end)(cg_device_t *dev, cg_pipeline_t *pipeline,
                unsigned long pipelines_difference);
    void (*pipeline_pre_change_notify)(cg_device_t *dev,
                                       cg_pipeline_t *pipeline,
                                       cg_pipeline_state_t change,
                                       const cg_color_t *new_color);
    void (*layer_pre_change_notify)(cg_device_t *dev,
                                    cg_pipeline_t *owner,
                                    cg_pipeline_layer_t *layer,
                                    cg_pipeline_layer_state_t change);
    /* This is called after all of the other functions whenever the
       pipeline is flushed, even if the pipeline hasn't changed since
       the last flush */
    void (*pre_paint)(cg_device_t *dev,
                      cg_pipeline_t *pipeline,
                      cg_framebuffer_t *framebuffer);
} cg_pipeline_progend_t;

typedef enum {
    CG_PIPELINE_PROGRAM_TYPE_GLSL = 1,
    CG_PIPELINE_PROGRAM_TYPE_NOP
} cg_pipeline_program_type_t;

extern const cg_pipeline_fragend_t *_cg_pipeline_fragends
[CG_PIPELINE_N_FRAGENDS];
extern const cg_pipeline_vertend_t *_cg_pipeline_vertends
[CG_PIPELINE_N_VERTENDS];
extern const cg_pipeline_progend_t *_cg_pipeline_progends[];

void _cg_pipeline_init_default_pipeline(cg_device_t *dev);

static inline cg_pipeline_t *
_cg_pipeline_get_parent(cg_pipeline_t *pipeline)
{
    cg_node_t *parent_node = CG_NODE(pipeline)->parent;
    return CG_PIPELINE(parent_node);
}

static inline cg_pipeline_t *
_cg_pipeline_get_authority(cg_pipeline_t *pipeline, unsigned long difference)
{
    cg_pipeline_t *authority = pipeline;
    while (!(authority->differences & difference))
        authority = _cg_pipeline_get_parent(authority);
    return authority;
}

typedef bool (*cg_pipeline_state_comparitor_t)(cg_pipeline_t *authority0,
                                               cg_pipeline_t *authority1);

void _cg_pipeline_update_authority(cg_pipeline_t *pipeline,
                                   cg_pipeline_t *authority,
                                   cg_pipeline_state_t state,
                                   cg_pipeline_state_comparitor_t comparitor);

void _cg_pipeline_pre_change_notify(cg_pipeline_t *pipeline,
                                    cg_pipeline_state_t change,
                                    const cg_color_t *new_color,
                                    bool from_layer_change);

void _cg_pipeline_prune_redundant_ancestry(cg_pipeline_t *pipeline);

void _cg_pipeline_update_real_blend_enable(cg_pipeline_t *pipeline,
                                           bool unknown_color_alpha);

typedef enum {
    CG_PIPELINE_GET_LAYER_NO_CREATE = 1 << 0
} cg_pipeline_get_layer_flags_t;

cg_pipeline_layer_t *
_cg_pipeline_get_layer_with_flags(cg_pipeline_t *pipeline,
                                  int layer_index,
                                  cg_pipeline_get_layer_flags_t flags);

#define _cg_pipeline_get_layer(p, l) _cg_pipeline_get_layer_with_flags(p, l, 0)

bool _cg_is_pipeline_layer(void *object);

void _cg_pipeline_prune_empty_layer_difference(cg_pipeline_t *layers_authority,
                                               cg_pipeline_layer_t *layer);

/*
 * SECTION:cg-pipeline-internals
 * @short_description: Functions for creating custom primitives that make use
 *    of CGlib pipelines for filling.
 *
 * Normally you shouldn't need to use this API directly, but if you need to
 * developing a custom/specialised primitive - probably using raw OpenGL - then
 * this API aims to expose enough of the pipeline internals to support being
 * able to fill your geometry according to a given CGlib pipeline.
 */

bool _cg_pipeline_get_real_blend_enabled(cg_pipeline_t *pipeline);

/*
 * Calls the pre_paint method on the layer texture if there is
 * one. This will determine whether mipmaps are needed based on the
 * filter settings.
 */
void _cg_pipeline_pre_paint_for_layer(cg_pipeline_t *pipeline, int layer_id);

/*
 * cg_pipeline_flush_flag_t:
 * @CG_PIPELINE_FLUSH_FALLBACK_MASK: The fallback_layers member is set to
 *      a uint32_t mask of the layers that can't be supported with the user
 *      supplied texture and need to be replaced with fallback textures. (1 =
 *      fallback, and the least significant bit = layer 0)
 * @CG_PIPELINE_FLUSH_DISABLE_MASK: The disable_layers member is set to
 *      a uint32_t mask of the layers that you want to completly disable
 *      texturing for (1 = fallback, and the least significant bit = layer 0)
 * @CG_PIPELINE_FLUSH_LAYER0_OVERRIDE: The layer0_override_texture member is
 *      set to a GLuint OpenGL texture name to override the texture used for
 *      layer 0 of the pipeline. This is intended for dealing with sliced
 *      textures where you will need to point to each of the texture slices in
 *      turn when drawing your geometry.  Passing a value of 0 is the same as
 *      not passing the option at all.
 * @CG_PIPELINE_FLUSH_SKIP_GL_COLOR: When flushing the GL state for the
 *      pipeline don't call glColor.
 */
typedef enum _cg_pipeline_flush_flag_t {
    CG_PIPELINE_FLUSH_FALLBACK_MASK = 1L << 0,
    CG_PIPELINE_FLUSH_DISABLE_MASK = 1L << 1,
    CG_PIPELINE_FLUSH_LAYER0_OVERRIDE = 1L << 2,
    CG_PIPELINE_FLUSH_SKIP_GL_COLOR = 1L << 3
} cg_pipeline_flush_flag_t;

/*
 * cg_pipeline_flush_options_t:
 *
 */
typedef struct _cg_pipeline_flush_options_t {
    cg_pipeline_flush_flag_t flags;

    uint32_t fallback_layers;
    uint32_t disable_layers;
    cg_texture_t *layer0_override_texture;
} cg_pipeline_flush_options_t;

void _cg_pipeline_set_progend(cg_pipeline_t *pipeline, int progend);

cg_pipeline_t *_cg_pipeline_get_parent(cg_pipeline_t *pipeline);

/* XXX: At some point it could be good for this to accept a mask of
 * the state groups we are interested in comparing since we can
 * probably use that information in a number situations to reduce
 * the work we do. */
unsigned long _cg_pipeline_compare_differences(cg_pipeline_t *pipeline0,
                                               cg_pipeline_t *pipeline1);

bool _cg_pipeline_equal(cg_pipeline_t *pipeline0,
                        cg_pipeline_t *pipeline1,
                        unsigned int differences,
                        unsigned long layer_differences,
                        cg_pipeline_eval_flags_t flags);

unsigned int _cg_pipeline_hash(cg_pipeline_t *pipeline,
                               unsigned int differences,
                               unsigned long layer_differences,
                               cg_pipeline_eval_flags_t flags);

/* Makes a copy of the given pipeline that is a child of the root
 * pipeline rather than a child of the source pipeline. That way the
 * new pipeline won't hold a reference to the source pipeline. The
 * differences specified in @differences and @layer_differences are
 * copied across and all other state is left with the default
 * values. */
cg_pipeline_t *_cg_pipeline_deep_copy(cg_device_t *dev,
                                      cg_pipeline_t *pipeline,
                                      unsigned long differences,
                                      unsigned long layer_differences);

void _cg_pipeline_texture_storage_change_notify(cg_texture_t *texture);

void _cg_pipeline_apply_overrides(cg_pipeline_t *pipeline,
                                  cg_pipeline_flush_options_t *options);

cg_pipeline_blend_enable_t
_cg_pipeline_get_blend_enabled(cg_pipeline_t *pipeline);

void _cg_pipeline_set_blend_enabled(cg_pipeline_t *pipeline,
                                    cg_pipeline_blend_enable_t enable);

#ifdef CG_DEBUG_ENABLED
void _cg_pipeline_set_static_breadcrumb(cg_pipeline_t *pipeline,
                                        const char *breadcrumb);
#endif

unsigned long _cg_pipeline_get_age(cg_pipeline_t *pipeline);

cg_pipeline_t *_cg_pipeline_get_authority(cg_pipeline_t *pipeline,
                                          unsigned long difference);

void _cg_pipeline_add_layer_difference(cg_pipeline_t *pipeline,
                                       cg_pipeline_layer_t *layer,
                                       bool inc_n_layers);

void _cg_pipeline_remove_layer_difference(cg_pipeline_t *pipeline,
                                          cg_pipeline_layer_t *layer,
                                          bool dec_n_layers);

cg_pipeline_t *
_cg_pipeline_find_equivalent_parent(cg_pipeline_t *pipeline,
                                    cg_pipeline_state_t pipeline_state,
                                    cg_pipeline_layer_state_t layer_state);

void _cg_pipeline_prune_to_n_layers(cg_pipeline_t *pipeline, int n);

/*
 * API to support the deprecate cg_pipeline_layer_xyz functions...
 */

typedef bool (*cg_pipeline_internal_layer_callback_t)(
    cg_pipeline_layer_t *layer, void *user_data);

void _cg_pipeline_foreach_layer_internal(
    cg_pipeline_t *pipeline,
    cg_pipeline_internal_layer_callback_t callback,
    void *user_data);

bool _cg_pipeline_layer_numbers_equal(cg_pipeline_t *pipeline0,
                                      cg_pipeline_t *pipeline1);

void _cg_pipeline_init_state_hash_functions(void);

void _cg_pipeline_init_layer_state_hash_functions(void);

cg_pipeline_state_t
_cg_pipeline_get_state_for_vertex_codegen(cg_device_t *dev);

cg_pipeline_layer_state_t
_cg_pipeline_get_layer_state_for_fragment_codegen(cg_device_t *dev);

cg_pipeline_state_t
_cg_pipeline_get_state_for_fragment_codegen(cg_device_t *dev);

#endif /* __CG_PIPELINE_PRIVATE_H */
