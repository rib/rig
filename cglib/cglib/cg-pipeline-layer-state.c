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
#include "cg-pipeline-private.h"
#include "cg-blend-string.h"
#include "cg-util.h"
#include "cg-snippet-private.h"
#include "cg-texture-private.h"
#include "cg-pipeline-layer-state-private.h"
#include "cg-error-private.h"

#include "string.h"
#if 0
#include "cg-device-private.h"
#include "cg-color-private.h"

#endif

/*
 * XXX: consider special casing layer->unit_index so it's not a sparse
 * property so instead we can assume it's valid for all layer
 * instances.
 * - We would need to initialize ->unit_index in
 *   _cg_pipeline_layer_copy ().
 *
 * XXX: If you use this API you should consider that the given layer
 * might not be writeable and so a new derived layer will be allocated
 * and modified instead. The layer modified will be returned so you
 * can identify when this happens.
 */
cg_pipeline_layer_t *
_cg_pipeline_set_layer_unit(cg_pipeline_t *required_owner,
                            cg_pipeline_layer_t *layer,
                            int unit_index)
{
    cg_pipeline_layer_state_t change = CG_PIPELINE_LAYER_STATE_UNIT;
    cg_pipeline_layer_t *authority =
        _cg_pipeline_layer_get_authority(layer, change);
    cg_pipeline_layer_t *new;

    if (authority->unit_index == unit_index)
        return layer;

    new = _cg_pipeline_layer_pre_change_notify(required_owner, layer, change);
    if (new != layer)
        layer = new;
    else {
        /* If the layer we found is currently the authority on the state
         * we are changing see if we can revert to one of our ancestors
         * being the authority. */
        if (layer == authority &&
            _cg_pipeline_layer_get_parent(authority) != NULL) {
            cg_pipeline_layer_t *parent =
                _cg_pipeline_layer_get_parent(authority);
            cg_pipeline_layer_t *old_authority =
                _cg_pipeline_layer_get_authority(parent, change);

            if (old_authority->unit_index == unit_index) {
                layer->differences &= ~change;
                return layer;
            }
        }
    }

    layer->unit_index = unit_index;

    /* If we weren't previously the authority on this state then we need
     * to extended our differences mask and so it's possible that some
     * of our ancestry will now become redundant, so we aim to reparent
     * ourselves if that's true... */
    if (layer != authority) {
        layer->differences |= change;
        _cg_pipeline_layer_prune_redundant_ancestry(layer);
    }

    return layer;
}

cg_texture_t *
_cg_pipeline_layer_get_texture_real(cg_pipeline_layer_t *layer)
{
    cg_pipeline_layer_t *authority = _cg_pipeline_layer_get_authority(
        layer, CG_PIPELINE_LAYER_STATE_TEXTURE_DATA);

    return authority->texture;
}

cg_texture_t *
cg_pipeline_get_layer_texture(cg_pipeline_t *pipeline,
                              int layer_index)
{
    cg_pipeline_layer_t *layer = _cg_pipeline_get_layer(pipeline, layer_index);
    return _cg_pipeline_layer_get_texture(layer);
}

cg_texture_type_t
_cg_pipeline_layer_get_texture_type(cg_pipeline_layer_t *layer)
{
    cg_pipeline_layer_t *authority = _cg_pipeline_layer_get_authority(
        layer, CG_PIPELINE_LAYER_STATE_TEXTURE_TYPE);

    return authority->texture_type;
}

static void
_cg_pipeline_set_layer_texture_type(cg_pipeline_t *pipeline,
                                    int layer_index,
                                    cg_texture_type_t texture_type)
{
    cg_pipeline_layer_state_t change = CG_PIPELINE_LAYER_STATE_TEXTURE_TYPE;
    cg_pipeline_layer_t *layer;
    cg_pipeline_layer_t *authority;
    cg_pipeline_layer_t *new;

    /* Note: this will ensure that the layer exists, creating one if it
     * doesn't already.
     *
     * Note: If the layer already existed it's possibly owned by another
     * pipeline. If the layer is created then it will be owned by
     * pipeline. */
    layer = _cg_pipeline_get_layer(pipeline, layer_index);

    /* Now find the ancestor of the layer that is the authority for the
     * state we want to change */
    authority = _cg_pipeline_layer_get_authority(layer, change);

    if (texture_type == authority->texture_type)
        return;

    new = _cg_pipeline_layer_pre_change_notify(pipeline, layer, change);
    if (new != layer)
        layer = new;
    else {
        /* If the original layer we found is currently the authority on
         * the state we are changing see if we can revert to one of our
         * ancestors being the authority. */
        if (layer == authority &&
            _cg_pipeline_layer_get_parent(authority) != NULL) {
            cg_pipeline_layer_t *parent =
                _cg_pipeline_layer_get_parent(authority);
            cg_pipeline_layer_t *old_authority =
                _cg_pipeline_layer_get_authority(parent, change);

            if (old_authority->texture_type == texture_type) {
                layer->differences &= ~change;

                c_assert(layer->owner == pipeline);
                if (layer->differences == 0)
                    _cg_pipeline_prune_empty_layer_difference(pipeline, layer);
                goto changed;
            }
        }
    }

    layer->texture_type = texture_type;

    /* If we weren't previously the authority on this state then we need
     * to extended our differences mask and so it's possible that some
     * of our ancestry will now become redundant, so we aim to reparent
     * ourselves if that's true... */
    if (layer != authority) {
        layer->differences |= change;
        _cg_pipeline_layer_prune_redundant_ancestry(layer);
    }

changed:

    pipeline->dirty_real_blend_enable = true;
}

static void
_cg_pipeline_set_layer_texture_data(cg_pipeline_t *pipeline,
                                    int layer_index,
                                    cg_texture_t *texture)
{
    cg_pipeline_layer_state_t change = CG_PIPELINE_LAYER_STATE_TEXTURE_DATA;
    cg_pipeline_layer_t *layer;
    cg_pipeline_layer_t *authority;
    cg_pipeline_layer_t *new;

    /* Note: this will ensure that the layer exists, creating one if it
     * doesn't already.
     *
     * Note: If the layer already existed it's possibly owned by another
     * pipeline. If the layer is created then it will be owned by
     * pipeline. */
    layer = _cg_pipeline_get_layer(pipeline, layer_index);

    /* Now find the ancestor of the layer that is the authority for the
     * state we want to change */
    authority = _cg_pipeline_layer_get_authority(layer, change);

    if (authority->texture == texture)
        return;

    new = _cg_pipeline_layer_pre_change_notify(pipeline, layer, change);
    if (new != layer)
        layer = new;
    else {
        /* If the original layer we found is currently the authority on
         * the state we are changing see if we can revert to one of our
         * ancestors being the authority. */
        if (layer == authority &&
            _cg_pipeline_layer_get_parent(authority) != NULL) {
            cg_pipeline_layer_t *parent =
                _cg_pipeline_layer_get_parent(authority);
            cg_pipeline_layer_t *old_authority =
                _cg_pipeline_layer_get_authority(parent, change);

            if (old_authority->texture == texture) {
                layer->differences &= ~change;

                if (layer->texture != NULL)
                    cg_object_unref(layer->texture);

                c_assert(layer->owner == pipeline);
                if (layer->differences == 0)
                    _cg_pipeline_prune_empty_layer_difference(pipeline, layer);
                goto changed;
            }
        }
    }

    if (texture != NULL)
        cg_object_ref(texture);
    if (layer == authority && layer->texture != NULL)
        cg_object_unref(layer->texture);
    layer->texture = texture;

    /* If we weren't previously the authority on this state then we need
     * to extended our differences mask and so it's possible that some
     * of our ancestry will now become redundant, so we aim to reparent
     * ourselves if that's true... */
    if (layer != authority) {
        layer->differences |= change;
        _cg_pipeline_layer_prune_redundant_ancestry(layer);
    }

changed:

    pipeline->dirty_real_blend_enable = true;
}

void
cg_pipeline_set_layer_texture(cg_pipeline_t *pipeline,
                              int layer_index,
                              cg_texture_t *texture)
{
    /* For the convenience of fragend code we separate texture state
     * into the "type" and the "data", and setting a layer texture
     * updates both of these properties.
     *
     * One example for why this is helpful is that the fragends may
     * cache programs they generate and want to re-use those programs
     * with all pipelines having equivalent fragment processing state.
     * For the sake of determining if pipelines have equivalent fragment
     * processing state we don't need to compare that the same
     * underlying texture objects are referenced by the pipelines but we
     * do need to see if they use the same texture types. Making this
     * distinction is much simpler if they are in different state
     * groups.
     *
     * Note: if a NULL texture is set then we leave the type unchanged
     * so we can avoid needlessly invalidating any associated fragment
     * program.
     */
    if (texture) {
        cg_texture_type_t texture_type = _cg_texture_get_type(texture);
        _cg_pipeline_set_layer_texture_type(
            pipeline, layer_index, texture_type);
    }
    _cg_pipeline_set_layer_texture_data(pipeline, layer_index, texture);
}

void
cg_pipeline_set_layer_null_texture(cg_pipeline_t *pipeline,
                                   int layer_index,
                                   cg_texture_type_t texture_type)
{
    cg_device_t *dev = _cg_device_get_default();

    /* Disallow setting texture types that aren't supported */
    switch (texture_type) {
    case CG_TEXTURE_TYPE_2D:
        break;

    case CG_TEXTURE_TYPE_3D:
        if (dev->default_gl_texture_3d_tex == NULL) {
            c_warning("The default 3D texture was set on a pipeline but "
                      "3D textures are not supported");
            texture_type = CG_TEXTURE_TYPE_2D;
            return;
        }
        break;
    }

    _cg_pipeline_set_layer_texture_type(pipeline, layer_index, texture_type);
    _cg_pipeline_set_layer_texture_data(pipeline, layer_index, NULL);
}

static void
_cg_pipeline_set_layer_sampler_state(cg_pipeline_t *pipeline,
                                     cg_pipeline_layer_t *layer,
                                     cg_pipeline_layer_t *authority,
                                     const cg_sampler_cache_entry_t *state)
{
    cg_pipeline_layer_t *new;
    cg_pipeline_layer_state_t change = CG_PIPELINE_LAYER_STATE_SAMPLER;

    if (authority->sampler_cache_entry == state)
        return;

    new = _cg_pipeline_layer_pre_change_notify(pipeline, layer, change);
    if (new != layer)
        layer = new;
    else {
        /* If the original layer we found is currently the authority on
         * the state we are changing see if we can revert to one of our
         * ancestors being the authority. */
        if (layer == authority &&
            _cg_pipeline_layer_get_parent(authority) != NULL) {
            cg_pipeline_layer_t *parent =
                _cg_pipeline_layer_get_parent(authority);
            cg_pipeline_layer_t *old_authority =
                _cg_pipeline_layer_get_authority(parent, change);

            if (old_authority->sampler_cache_entry == state) {
                layer->differences &= ~change;

                c_assert(layer->owner == pipeline);
                if (layer->differences == 0)
                    _cg_pipeline_prune_empty_layer_difference(pipeline, layer);
                return;
            }
        }
    }

    layer->sampler_cache_entry = state;

    /* If we weren't previously the authority on this state then we need
     * to extended our differences mask and so it's possible that some
     * of our ancestry will now become redundant, so we aim to reparent
     * ourselves if that's true... */
    if (layer != authority) {
        layer->differences |= change;
        _cg_pipeline_layer_prune_redundant_ancestry(layer);
    }
}

static cg_sampler_cache_wrap_mode_t
public_to_internal_wrap_mode(cg_pipeline_wrap_mode_t mode)
{
    return (cg_sampler_cache_wrap_mode_t)mode;
}

static cg_pipeline_wrap_mode_t
internal_to_public_wrap_mode(cg_sampler_cache_wrap_mode_t internal_mode)
{
    c_return_val_if_fail(internal_mode !=
                         CG_SAMPLER_CACHE_WRAP_MODE_CLAMP_TO_BORDER,
                         CG_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);
    return (cg_pipeline_wrap_mode_t)internal_mode;
}

void
cg_pipeline_set_layer_wrap_mode_s(cg_pipeline_t *pipeline,
                                  int layer_index,
                                  cg_pipeline_wrap_mode_t mode)
{
    cg_pipeline_layer_state_t change = CG_PIPELINE_LAYER_STATE_SAMPLER;
    cg_pipeline_layer_t *layer;
    cg_pipeline_layer_t *authority;
    cg_sampler_cache_wrap_mode_t internal_mode =
        public_to_internal_wrap_mode(mode);
    const cg_sampler_cache_entry_t *sampler_state;

    _CG_GET_DEVICE(dev, NO_RETVAL);

    c_return_if_fail(cg_is_pipeline(pipeline));

    /* Note: this will ensure that the layer exists, creating one if it
     * doesn't already.
     *
     * Note: If the layer already existed it's possibly owned by another
     * pipeline. If the layer is created then it will be owned by
     * pipeline. */
    layer = _cg_pipeline_get_layer(pipeline, layer_index);

    /* Now find the ancestor of the layer that is the authority for the
     * state we want to change */
    authority = _cg_pipeline_layer_get_authority(layer, change);

    sampler_state = _cg_sampler_cache_update_wrap_modes(
        dev->sampler_cache,
        authority->sampler_cache_entry,
        internal_mode,
        authority->sampler_cache_entry->wrap_mode_t,
        authority->sampler_cache_entry->wrap_mode_p);
    _cg_pipeline_set_layer_sampler_state(
        pipeline, layer, authority, sampler_state);
}

void
cg_pipeline_set_layer_wrap_mode_t(cg_pipeline_t *pipeline,
                                  int layer_index,
                                  cg_pipeline_wrap_mode_t mode)
{
    cg_pipeline_layer_state_t change = CG_PIPELINE_LAYER_STATE_SAMPLER;
    cg_pipeline_layer_t *layer;
    cg_pipeline_layer_t *authority;
    cg_sampler_cache_wrap_mode_t internal_mode =
        public_to_internal_wrap_mode(mode);
    const cg_sampler_cache_entry_t *sampler_state;

    _CG_GET_DEVICE(dev, NO_RETVAL);

    c_return_if_fail(cg_is_pipeline(pipeline));

    /* Note: this will ensure that the layer exists, creating one if it
     * doesn't already.
     *
     * Note: If the layer already existed it's possibly owned by another
     * pipeline. If the layer is created then it will be owned by
     * pipeline. */
    layer = _cg_pipeline_get_layer(pipeline, layer_index);

    /* Now find the ancestor of the layer that is the authority for the
     * state we want to change */
    authority = _cg_pipeline_layer_get_authority(layer, change);

    sampler_state = _cg_sampler_cache_update_wrap_modes(
        dev->sampler_cache,
        authority->sampler_cache_entry,
        authority->sampler_cache_entry->wrap_mode_s,
        internal_mode,
        authority->sampler_cache_entry->wrap_mode_p);
    _cg_pipeline_set_layer_sampler_state(
        pipeline, layer, authority, sampler_state);
}

/* The rationale for naming the third texture coordinate 'p' instead
   of OpenGL's usual 'r' is that 'r' conflicts with the usual naming
   of the 'red' component when treating a vector as a color. Under
   GLSL this is awkward because the texture swizzling for a vector
   uses a single letter for each component and the names for colors,
   textures and positions are synonymous. GLSL works around this by
   naming the components of the texture s, t, p and q. CGlib already
   effectively already exposes this naming because it exposes GLSL so
   it makes sense to use that naming consistently. Another alternative
   could be u, v and w. This is what Blender and Direct3D use. However
   the w component conflicts with the w component of a position
   vertex.  */
void
cg_pipeline_set_layer_wrap_mode_p(cg_pipeline_t *pipeline,
                                  int layer_index,
                                  cg_pipeline_wrap_mode_t mode)
{
    cg_pipeline_layer_state_t change = CG_PIPELINE_LAYER_STATE_SAMPLER;
    cg_pipeline_layer_t *layer;
    cg_pipeline_layer_t *authority;
    cg_sampler_cache_wrap_mode_t internal_mode =
        public_to_internal_wrap_mode(mode);
    const cg_sampler_cache_entry_t *sampler_state;

    _CG_GET_DEVICE(dev, NO_RETVAL);

    c_return_if_fail(cg_is_pipeline(pipeline));

    /* Note: this will ensure that the layer exists, creating one if it
     * doesn't already.
     *
     * Note: If the layer already existed it's possibly owned by another
     * pipeline. If the layer is created then it will be owned by
     * pipeline. */
    layer = _cg_pipeline_get_layer(pipeline, layer_index);

    /* Now find the ancestor of the layer that is the authority for the
     * state we want to change */
    authority = _cg_pipeline_layer_get_authority(layer, change);

    sampler_state = _cg_sampler_cache_update_wrap_modes(
        dev->sampler_cache,
        authority->sampler_cache_entry,
        authority->sampler_cache_entry->wrap_mode_s,
        authority->sampler_cache_entry->wrap_mode_t,
        internal_mode);
    _cg_pipeline_set_layer_sampler_state(
        pipeline, layer, authority, sampler_state);
}

void
cg_pipeline_set_layer_wrap_mode(cg_pipeline_t *pipeline,
                                int layer_index,
                                cg_pipeline_wrap_mode_t mode)
{
    cg_pipeline_layer_state_t change = CG_PIPELINE_LAYER_STATE_SAMPLER;
    cg_pipeline_layer_t *layer;
    cg_pipeline_layer_t *authority;
    cg_sampler_cache_wrap_mode_t internal_mode =
        public_to_internal_wrap_mode(mode);
    const cg_sampler_cache_entry_t *sampler_state;

    _CG_GET_DEVICE(dev, NO_RETVAL);

    c_return_if_fail(cg_is_pipeline(pipeline));

    /* Note: this will ensure that the layer exists, creating one if it
     * doesn't already.
     *
     * Note: If the layer already existed it's possibly owned by another
     * pipeline. If the layer is created then it will be owned by
     * pipeline. */
    layer = _cg_pipeline_get_layer(pipeline, layer_index);

    /* Now find the ancestor of the layer that is the authority for the
     * state we want to change */
    authority = _cg_pipeline_layer_get_authority(layer, change);

    sampler_state =
        _cg_sampler_cache_update_wrap_modes(dev->sampler_cache,
                                            authority->sampler_cache_entry,
                                            internal_mode,
                                            internal_mode,
                                            internal_mode);
    _cg_pipeline_set_layer_sampler_state(
        pipeline, layer, authority, sampler_state);
    /* XXX: I wonder if we should really be duplicating the mode into
     * the 'p' wrap mode too? */
}

/* FIXME: deprecate this API */
cg_pipeline_wrap_mode_t
_cg_pipeline_layer_get_wrap_mode_s(cg_pipeline_layer_t *layer)
{
    cg_pipeline_layer_state_t change = CG_PIPELINE_LAYER_STATE_SAMPLER;
    cg_pipeline_layer_t *authority;
    const cg_sampler_cache_entry_t *sampler_state;

    c_return_val_if_fail(_cg_is_pipeline_layer(layer), false);

    /* Now find the ancestor of the layer that is the authority for the
     * state we want to change */
    authority = _cg_pipeline_layer_get_authority(layer, change);

    sampler_state = authority->sampler_cache_entry;
    return internal_to_public_wrap_mode(sampler_state->wrap_mode_s);
}

cg_pipeline_wrap_mode_t
cg_pipeline_get_layer_wrap_mode_s(cg_pipeline_t *pipeline, int layer_index)
{
    cg_pipeline_layer_t *layer;

    c_return_val_if_fail(cg_is_pipeline(pipeline), false);

    /* Note: this will ensure that the layer exists, creating one if it
     * doesn't already.
     *
     * Note: If the layer already existed it's possibly owned by another
     * pipeline. If the layer is created then it will be owned by
     * pipeline. */
    layer = _cg_pipeline_get_layer(pipeline, layer_index);
    /* FIXME: we shouldn't ever construct a layer in a getter function */

    return _cg_pipeline_layer_get_wrap_mode_s(layer);
}

/* FIXME: deprecate this API */
cg_pipeline_wrap_mode_t
_cg_pipeline_layer_get_wrap_mode_t(cg_pipeline_layer_t *layer)
{
    cg_pipeline_layer_state_t change = CG_PIPELINE_LAYER_STATE_SAMPLER;
    cg_pipeline_layer_t *authority;
    const cg_sampler_cache_entry_t *sampler_state;

    c_return_val_if_fail(_cg_is_pipeline_layer(layer), false);

    /* Now find the ancestor of the layer that is the authority for the
     * state we want to change */
    authority = _cg_pipeline_layer_get_authority(layer, change);

    sampler_state = authority->sampler_cache_entry;
    return internal_to_public_wrap_mode(sampler_state->wrap_mode_t);
}

cg_pipeline_wrap_mode_t
cg_pipeline_get_layer_wrap_mode_t(cg_pipeline_t *pipeline, int layer_index)
{
    cg_pipeline_layer_t *layer;

    c_return_val_if_fail(cg_is_pipeline(pipeline), false);

    /* Note: this will ensure that the layer exists, creating one if it
     * doesn't already.
     *
     * Note: If the layer already existed it's possibly owned by another
     * pipeline. If the layer is created then it will be owned by
     * pipeline. */
    layer = _cg_pipeline_get_layer(pipeline, layer_index);
    /* FIXME: we shouldn't ever construct a layer in a getter function */

    return _cg_pipeline_layer_get_wrap_mode_t(layer);
}

cg_pipeline_wrap_mode_t
_cg_pipeline_layer_get_wrap_mode_p(cg_pipeline_layer_t *layer)
{
    cg_pipeline_layer_state_t change = CG_PIPELINE_LAYER_STATE_SAMPLER;
    cg_pipeline_layer_t *authority =
        _cg_pipeline_layer_get_authority(layer, change);
    const cg_sampler_cache_entry_t *sampler_state;

    sampler_state = authority->sampler_cache_entry;
    return internal_to_public_wrap_mode(sampler_state->wrap_mode_p);
}

cg_pipeline_wrap_mode_t
cg_pipeline_get_layer_wrap_mode_p(cg_pipeline_t *pipeline, int layer_index)
{
    cg_pipeline_layer_t *layer;

    c_return_val_if_fail(cg_is_pipeline(pipeline), false);

    /* Note: this will ensure that the layer exists, creating one if it
     * doesn't already.
     *
     * Note: If the layer already existed it's possibly owned by another
     * pipeline. If the layer is created then it will be owned by
     * pipeline. */
    layer = _cg_pipeline_get_layer(pipeline, layer_index);

    return _cg_pipeline_layer_get_wrap_mode_p(layer);
}

void
_cg_pipeline_layer_get_wrap_modes(cg_pipeline_layer_t *layer,
                                  cg_sampler_cache_wrap_mode_t *wrap_mode_s,
                                  cg_sampler_cache_wrap_mode_t *wrap_mode_t,
                                  cg_sampler_cache_wrap_mode_t *wrap_mode_p)
{
    cg_pipeline_layer_t *authority = _cg_pipeline_layer_get_authority(
        layer, CG_PIPELINE_LAYER_STATE_SAMPLER);

    *wrap_mode_s = authority->sampler_cache_entry->wrap_mode_s;
    *wrap_mode_t = authority->sampler_cache_entry->wrap_mode_t;
    *wrap_mode_p = authority->sampler_cache_entry->wrap_mode_p;
}

bool
cg_pipeline_set_layer_point_sprite_coords_enabled(cg_pipeline_t *pipeline,
                                                  int layer_index,
                                                  bool enable,
                                                  cg_error_t **error)
{
    cg_pipeline_layer_state_t change =
        CG_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS;
    cg_pipeline_layer_t *layer;
    cg_pipeline_layer_t *new;
    cg_pipeline_layer_t *authority;

    _CG_GET_DEVICE(dev, false);

    c_return_val_if_fail(cg_is_pipeline(pipeline), false);

    /* Don't allow point sprite coordinates to be enabled if the driver
       doesn't support it */
    if (enable && !cg_has_feature(dev, CG_FEATURE_ID_POINT_SPRITE)) {
        if (error) {
            _cg_set_error(error,
                          CG_SYSTEM_ERROR,
                          CG_SYSTEM_ERROR_UNSUPPORTED,
                          "Point sprite texture coordinates are enabled for "
                          "a layer but the GL driver does not support it.");
        } else {
            static bool warning_seen = false;
            if (!warning_seen)
                c_warning("Point sprite texture coordinates are enabled "
                          "for a layer but the GL driver does not support it.");
            warning_seen = true;
        }

        return false;
    }

    /* Note: this will ensure that the layer exists, creating one if it
     * doesn't already.
     *
     * Note: If the layer already existed it's possibly owned by another
     * pipeline. If the layer is created then it will be owned by
     * pipeline. */
    layer = _cg_pipeline_get_layer(pipeline, layer_index);

    /* Now find the ancestor of the layer that is the authority for the
     * state we want to change */
    authority = _cg_pipeline_layer_get_authority(layer, change);

    if (authority->big_state->point_sprite_coords == enable)
        return true;

    new = _cg_pipeline_layer_pre_change_notify(pipeline, layer, change);
    if (new != layer)
        layer = new;
    else {
        /* If the original layer we found is currently the authority on
         * the state we are changing see if we can revert to one of our
         * ancestors being the authority. */
        if (layer == authority &&
            _cg_pipeline_layer_get_parent(authority) != NULL) {
            cg_pipeline_layer_t *parent =
                _cg_pipeline_layer_get_parent(authority);
            cg_pipeline_layer_t *old_authority =
                _cg_pipeline_layer_get_authority(parent, change);

            if (old_authority->big_state->point_sprite_coords == enable) {
                layer->differences &= ~change;

                c_assert(layer->owner == pipeline);
                if (layer->differences == 0)
                    _cg_pipeline_prune_empty_layer_difference(pipeline, layer);
                return true;
            }
        }
    }

    layer->big_state->point_sprite_coords = enable;

    /* If we weren't previously the authority on this state then we need
     * to extended our differences mask and so it's possible that some
     * of our ancestry will now become redundant, so we aim to reparent
     * ourselves if that's true... */
    if (layer != authority) {
        layer->differences |= change;
        _cg_pipeline_layer_prune_redundant_ancestry(layer);
    }

    return true;
}

bool
cg_pipeline_get_layer_point_sprite_coords_enabled(cg_pipeline_t *pipeline,
                                                  int layer_index)
{
    cg_pipeline_layer_state_t change =
        CG_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS;
    cg_pipeline_layer_t *layer;
    cg_pipeline_layer_t *authority;

    c_return_val_if_fail(cg_is_pipeline(pipeline), false);

    /* Note: this will ensure that the layer exists, creating one if it
     * doesn't already.
     *
     * Note: If the layer already existed it's possibly owned by another
     * pipeline. If the layer is created then it will be owned by
     * pipeline. */
    layer = _cg_pipeline_get_layer(pipeline, layer_index);
    /* FIXME: we shouldn't ever construct a layer in a getter function */

    authority = _cg_pipeline_layer_get_authority(layer, change);

    return authority->big_state->point_sprite_coords;
}

static void
_cg_pipeline_layer_add_vertex_snippet(cg_pipeline_t *pipeline,
                                      int layer_index,
                                      cg_snippet_t *snippet)
{
    cg_pipeline_layer_state_t change = CG_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS;
    cg_pipeline_layer_t *layer, *authority;

    /* Note: this will ensure that the layer exists, creating one if it
     * doesn't already.
     *
     * Note: If the layer already existed it's possibly owned by another
     * pipeline. If the layer is created then it will be owned by
     * pipeline. */
    layer = _cg_pipeline_get_layer(pipeline, layer_index);

    /* Now find the ancestor of the layer that is the authority for the
     * state we want to change */
    authority = _cg_pipeline_layer_get_authority(layer, change);

    layer = _cg_pipeline_layer_pre_change_notify(pipeline, layer, change);

    _cg_pipeline_snippet_list_add(&layer->big_state->vertex_snippets, snippet);

    /* If we weren't previously the authority on this state then we need
     * to extended our differences mask and so it's possible that some
     * of our ancestry will now become redundant, so we aim to reparent
     * ourselves if that's true... */
    if (layer != authority) {
        layer->differences |= change;
        _cg_pipeline_layer_prune_redundant_ancestry(layer);
    }
}

static void
_cg_pipeline_layer_add_fragment_snippet(cg_pipeline_t *pipeline,
                                        int layer_index,
                                        cg_snippet_t *snippet)
{
    cg_pipeline_layer_state_t change =
        CG_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS;
    cg_pipeline_layer_t *layer, *authority;

    /* Note: this will ensure that the layer exists, creating one if it
     * doesn't already.
     *
     * Note: If the layer already existed it's possibly owned by another
     * pipeline. If the layer is created then it will be owned by
     * pipeline. */
    layer = _cg_pipeline_get_layer(pipeline, layer_index);

    /* Now find the ancestor of the layer that is the authority for the
     * state we want to change */
    authority = _cg_pipeline_layer_get_authority(layer, change);

    layer = _cg_pipeline_layer_pre_change_notify(pipeline, layer, change);

    _cg_pipeline_snippet_list_add(&layer->big_state->fragment_snippets,
                                  snippet);

    /* If we weren't previously the authority on this state then we need
     * to extended our differences mask and so it's possible that some
     * of our ancestry will now become redundant, so we aim to reparent
     * ourselves if that's true... */
    if (layer != authority) {
        layer->differences |= change;
        _cg_pipeline_layer_prune_redundant_ancestry(layer);
    }
}

void
cg_pipeline_add_layer_snippet(cg_pipeline_t *pipeline,
                              int layer_index,
                              cg_snippet_t *snippet)
{
    c_return_if_fail(cg_is_pipeline(pipeline));
    c_return_if_fail(cg_is_snippet(snippet));
    c_return_if_fail(snippet->hook >= CG_SNIPPET_FIRST_LAYER_HOOK);

    if (snippet->hook < CG_SNIPPET_FIRST_LAYER_FRAGMENT_HOOK)
        _cg_pipeline_layer_add_vertex_snippet(pipeline, layer_index, snippet);
    else
        _cg_pipeline_layer_add_fragment_snippet(pipeline, layer_index, snippet);
}

bool
_cg_pipeline_layer_texture_type_equal(cg_pipeline_layer_t *authority0,
                                      cg_pipeline_layer_t *authority1,
                                      cg_pipeline_eval_flags_t flags)
{
    return authority0->texture_type == authority1->texture_type;
}

bool
_cg_pipeline_layer_texture_data_equal(cg_pipeline_layer_t *authority0,
                                      cg_pipeline_layer_t *authority1,
                                      cg_pipeline_eval_flags_t flags)
{
    if (authority0->texture == NULL) {
        if (authority1->texture == NULL)
            return (_cg_pipeline_layer_get_texture_type(authority0) ==
                    _cg_pipeline_layer_get_texture_type(authority1));
        else
            return false;
    } else if (authority1->texture == NULL)
        return false;
    else {
        GLuint gl_handle0, gl_handle1;

        cg_texture_get_gl_texture(authority0->texture, &gl_handle0, NULL);
        cg_texture_get_gl_texture(authority1->texture, &gl_handle1, NULL);

        return gl_handle0 == gl_handle1;
    }
}

bool
_cg_pipeline_layer_sampler_equal(cg_pipeline_layer_t *authority0,
                                 cg_pipeline_layer_t *authority1)
{
    /* We compare the actual sampler objects rather than just the entry
       pointers because two states with different values can lead to the
       same state in GL terms when AUTOMATIC is used as a wrap mode */
    return (authority0->sampler_cache_entry->sampler_object ==
            authority1->sampler_cache_entry->sampler_object);
}

bool
_cg_pipeline_layer_point_sprite_coords_equal(cg_pipeline_layer_t *authority0,
                                             cg_pipeline_layer_t *authority1)
{
    cg_pipeline_layer_big_state_t *big_state0 = authority0->big_state;
    cg_pipeline_layer_big_state_t *big_state1 = authority1->big_state;

    return big_state0->point_sprite_coords == big_state1->point_sprite_coords;
}

bool
_cg_pipeline_layer_vertex_snippets_equal(cg_pipeline_layer_t *authority0,
                                         cg_pipeline_layer_t *authority1)
{
    return _cg_pipeline_snippet_list_equal(
        &authority0->big_state->vertex_snippets,
        &authority1->big_state->vertex_snippets);
}

bool
_cg_pipeline_layer_fragment_snippets_equal(cg_pipeline_layer_t *authority0,
                                           cg_pipeline_layer_t *authority1)
{
    return _cg_pipeline_snippet_list_equal(
        &authority0->big_state->fragment_snippets,
        &authority1->big_state->fragment_snippets);
}

cg_texture_t *
_cg_pipeline_layer_get_texture(cg_pipeline_layer_t *layer)
{
    c_return_val_if_fail(_cg_is_pipeline_layer(layer), NULL);

    return _cg_pipeline_layer_get_texture_real(layer);
}

void
_cg_pipeline_layer_get_filters(cg_pipeline_layer_t *layer,
                               cg_pipeline_filter_t *min_filter,
                               cg_pipeline_filter_t *mag_filter)
{
    cg_pipeline_layer_t *authority = _cg_pipeline_layer_get_authority(
        layer, CG_PIPELINE_LAYER_STATE_SAMPLER);

    *min_filter = authority->sampler_cache_entry->min_filter;
    *mag_filter = authority->sampler_cache_entry->mag_filter;
}

void
_cg_pipeline_get_layer_filters(cg_pipeline_t *pipeline,
                               int layer_index,
                               cg_pipeline_filter_t *min_filter,
                               cg_pipeline_filter_t *mag_filter)
{
    cg_pipeline_layer_t *layer;
    cg_pipeline_layer_t *authority;

    c_return_if_fail(cg_is_pipeline(pipeline));

    layer = _cg_pipeline_get_layer(pipeline, layer_index);

    authority = _cg_pipeline_layer_get_authority(
        layer, CG_PIPELINE_LAYER_STATE_SAMPLER);

    *min_filter = authority->sampler_cache_entry->min_filter;
    *mag_filter = authority->sampler_cache_entry->mag_filter;
}

cg_pipeline_filter_t
cg_pipeline_get_layer_min_filter(cg_pipeline_t *pipeline,
                                 int layer_index)
{
    cg_pipeline_filter_t min_filter;
    cg_pipeline_filter_t mag_filter;

    _cg_pipeline_get_layer_filters(
        pipeline, layer_index, &min_filter, &mag_filter);
    return min_filter;
}

cg_pipeline_filter_t
cg_pipeline_get_layer_mag_filter(cg_pipeline_t *pipeline,
                                 int layer_index)
{
    cg_pipeline_filter_t min_filter;
    cg_pipeline_filter_t mag_filter;

    _cg_pipeline_get_layer_filters(
        pipeline, layer_index, &min_filter, &mag_filter);
    return mag_filter;
}

cg_pipeline_filter_t
_cg_pipeline_layer_get_min_filter(cg_pipeline_layer_t *layer)
{
    cg_pipeline_layer_t *authority;

    c_return_val_if_fail(_cg_is_pipeline_layer(layer), 0);

    authority = _cg_pipeline_layer_get_authority(
        layer, CG_PIPELINE_LAYER_STATE_SAMPLER);

    return authority->sampler_cache_entry->min_filter;
}

cg_pipeline_filter_t
_cg_pipeline_layer_get_mag_filter(cg_pipeline_layer_t *layer)
{
    cg_pipeline_layer_t *authority;

    c_return_val_if_fail(_cg_is_pipeline_layer(layer), 0);

    authority = _cg_pipeline_layer_get_authority(
        layer, CG_PIPELINE_LAYER_STATE_SAMPLER);

    return authority->sampler_cache_entry->mag_filter;
}

void
cg_pipeline_set_layer_filters(cg_pipeline_t *pipeline,
                              int layer_index,
                              cg_pipeline_filter_t min_filter,
                              cg_pipeline_filter_t mag_filter)
{
    cg_pipeline_layer_state_t state = CG_PIPELINE_LAYER_STATE_SAMPLER;
    cg_pipeline_layer_t *layer;
    cg_pipeline_layer_t *authority;
    const cg_sampler_cache_entry_t *sampler_state;

    _CG_GET_DEVICE(dev, NO_RETVAL);

    c_return_if_fail(cg_is_pipeline(pipeline));

    c_return_if_fail(mag_filter == CG_PIPELINE_FILTER_NEAREST ||
                       mag_filter == CG_PIPELINE_FILTER_LINEAR);

    /* Note: this will ensure that the layer exists, creating one if it
     * doesn't already.
     *
     * Note: If the layer already existed it's possibly owned by another
     * pipeline. If the layer is created then it will be owned by
     * pipeline. */
    layer = _cg_pipeline_get_layer(pipeline, layer_index);

    /* Now find the ancestor of the layer that is the authority for the
     * state we want to change */
    authority = _cg_pipeline_layer_get_authority(layer, state);

    sampler_state =
        _cg_sampler_cache_update_filters(dev->sampler_cache,
                                         authority->sampler_cache_entry,
                                         min_filter,
                                         mag_filter);
    _cg_pipeline_set_layer_sampler_state(
        pipeline, layer, authority, sampler_state);
}

const cg_sampler_cache_entry_t *
_cg_pipeline_layer_get_sampler_state(cg_pipeline_layer_t *layer)
{
    cg_pipeline_layer_t *authority;

    authority = _cg_pipeline_layer_get_authority(
        layer, CG_PIPELINE_LAYER_STATE_SAMPLER);

    return authority->sampler_cache_entry;
}

void
_cg_pipeline_layer_hash_unit_state(cg_pipeline_layer_t *authority,
                                   cg_pipeline_layer_t **authorities,
                                   cg_pipeline_hash_state_t *state)
{
    int unit = authority->unit_index;
    state->hash = _cg_util_one_at_a_time_hash(state->hash, &unit, sizeof(unit));
}

void
_cg_pipeline_layer_hash_texture_type_state(cg_pipeline_layer_t *authority,
                                           cg_pipeline_layer_t **authorities,
                                           cg_pipeline_hash_state_t *state)
{
    cg_texture_type_t texture_type = authority->texture_type;

    state->hash = _cg_util_one_at_a_time_hash(
        state->hash, &texture_type, sizeof(texture_type));
}

void
_cg_pipeline_layer_hash_texture_data_state(cg_pipeline_layer_t *authority,
                                           cg_pipeline_layer_t **authorities,
                                           cg_pipeline_hash_state_t *state)
{
    GLuint gl_handle;

    cg_texture_get_gl_texture(authority->texture, &gl_handle, NULL);

    state->hash =
        _cg_util_one_at_a_time_hash(state->hash, &gl_handle, sizeof(gl_handle));
}

void
_cg_pipeline_layer_hash_sampler_state(cg_pipeline_layer_t *authority,
                                      cg_pipeline_layer_t **authorities,
                                      cg_pipeline_hash_state_t *state)
{
    state->hash =
        _cg_util_one_at_a_time_hash(state->hash,
                                    &authority->sampler_cache_entry,
                                    sizeof(authority->sampler_cache_entry));
}

void
_cg_pipeline_layer_hash_point_sprite_state(cg_pipeline_layer_t *authority,
                                           cg_pipeline_layer_t **authorities,
                                           cg_pipeline_hash_state_t *state)
{
    cg_pipeline_layer_big_state_t *big_state = authority->big_state;
    state->hash =
        _cg_util_one_at_a_time_hash(state->hash,
                                    &big_state->point_sprite_coords,
                                    sizeof(big_state->point_sprite_coords));
}

void
_cg_pipeline_layer_hash_vertex_snippets_state(cg_pipeline_layer_t *authority,
                                              cg_pipeline_layer_t **authorities,
                                              cg_pipeline_hash_state_t *state)
{
    _cg_pipeline_snippet_list_hash(&authority->big_state->vertex_snippets,
                                   &state->hash);
}

void
_cg_pipeline_layer_hash_fragment_snippets_state(
    cg_pipeline_layer_t *authority,
    cg_pipeline_layer_t **authorities,
    cg_pipeline_hash_state_t *state)
{
    _cg_pipeline_snippet_list_hash(&authority->big_state->fragment_snippets,
                                   &state->hash);
}
