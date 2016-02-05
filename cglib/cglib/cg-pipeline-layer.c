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

#include "cg-util.h"
#include "cg-device-private.h"
#include "cg-texture-private.h"

#include "cg-pipeline.h"
#include "cg-pipeline-layer-private.h"
#include "cg-pipeline-layer-state-private.h"
#include "cg-pipeline-layer-state.h"
#include "cg-node-private.h"
#include "cg-pipeline-opengl-private.h"
#include "cg-device-private.h"
#include "cg-texture-private.h"

#include <string.h>

static void _cg_pipeline_layer_free(cg_pipeline_layer_t *layer);

/* This type was made deprecated before the cg_is_pipeline_layer
   function was ever exposed in the public headers so there's no need
   to make the cg_is_pipeline_layer function public. We use INTERNAL
   so that the cg_is_* function won't get defined */
CG_OBJECT_INTERNAL_DEFINE(PipelineLayer, pipeline_layer);

cg_pipeline_layer_t *
_cg_pipeline_layer_get_authority(cg_pipeline_layer_t *layer,
                                 unsigned long difference)
{
    cg_pipeline_layer_t *authority = layer;
    while (!(authority->differences & difference))
        authority = _cg_pipeline_layer_get_parent(authority);
    return authority;
}

int
_cg_pipeline_layer_get_unit_index(cg_pipeline_layer_t *layer)
{
    cg_pipeline_layer_t *authority =
        _cg_pipeline_layer_get_authority(layer, CG_PIPELINE_LAYER_STATE_UNIT);
    return authority->unit_index;
}

bool
_cg_pipeline_layer_has_alpha(cg_pipeline_layer_t *layer)
{
    cg_pipeline_layer_t *tex_authority;
    cg_pipeline_layer_t *snippets_authority;

    /* NB: If the layer does not yet have an associated texture we'll
     * fallback to the default texture which doesn't have an alpha
     * component
     */
    tex_authority = _cg_pipeline_layer_get_authority(
        layer, CG_PIPELINE_LAYER_STATE_TEXTURE_DATA);
    if (tex_authority->texture) {
        cg_pixel_format_t authority_tex_format =
            _cg_texture_get_format(tex_authority->texture);

        if (_cg_pixel_format_has_alpha(authority_tex_format))
            return true;
    }

    /* All bets are off if the layer contains any snippets */
    snippets_authority = _cg_pipeline_layer_get_authority(
        layer, CG_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS);
    if (snippets_authority->big_state->vertex_snippets.entries != NULL)
        return true;
    snippets_authority = _cg_pipeline_layer_get_authority(
        layer, CG_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS);
    if (snippets_authority->big_state->fragment_snippets.entries != NULL)
        return true;

    return false;
}

void
_cg_pipeline_layer_copy_differences(cg_pipeline_layer_t *dest,
                                    cg_pipeline_layer_t *src,
                                    unsigned long differences)
{
    cg_pipeline_layer_big_state_t *big_dest, *big_src;

    if ((differences & CG_PIPELINE_LAYER_STATE_NEEDS_BIG_STATE) &&
        !dest->has_big_state) {
        dest->big_state = c_slice_new(cg_pipeline_layer_big_state_t);
        dest->has_big_state = true;
    }

    big_dest = dest->big_state;
    big_src = src->big_state;

    dest->differences |= differences;

    while (differences) {
        int index = _cg_util_ffs(differences) - 1;

        differences &= ~(1 << index);

        /* This convoluted switch statement is just here so that we'll
         * get a warning if a new state is added without handling it
         * here */
        switch (index) {
        case CG_PIPELINE_LAYER_STATE_COUNT:
        case CG_PIPELINE_LAYER_STATE_UNIT_INDEX:
            c_warn_if_reached();
            break;

        case CG_PIPELINE_LAYER_STATE_TEXTURE_TYPE_INDEX:
            dest->texture_type = src->texture_type;
            break;

        case CG_PIPELINE_LAYER_STATE_TEXTURE_DATA_INDEX:
            dest->texture = src->texture;
            if (dest->texture)
                cg_object_ref(dest->texture);
            break;

        case CG_PIPELINE_LAYER_STATE_SAMPLER_INDEX:
            dest->sampler_cache_entry = src->sampler_cache_entry;
            break;

        case CG_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS_INDEX:
            big_dest->point_sprite_coords = big_src->point_sprite_coords;
            break;

        case CG_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS_INDEX:
            _cg_pipeline_snippet_list_copy(&big_dest->vertex_snippets,
                                           &big_src->vertex_snippets);
            break;

        case CG_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS_INDEX:
            _cg_pipeline_snippet_list_copy(&big_dest->fragment_snippets,
                                           &big_src->fragment_snippets);
            break;
        }
    }
}

static void
_cg_pipeline_layer_init_multi_property_sparse_state(
    cg_pipeline_layer_t *layer, cg_pipeline_layer_state_t change)
{
    cg_pipeline_layer_t *authority;

    /* Nothing to initialize in these cases since they are all comprised
     * of one member which we expect to immediately be overwritten. */
    if (!(change & CG_PIPELINE_LAYER_STATE_MULTI_PROPERTY))
        return;

    authority = _cg_pipeline_layer_get_authority(layer, change);

    switch (change) {
    /* XXX: avoid using a default: label so we get a warning if we
    * don't explicitly handle a newly defined state-group here. */
    case CG_PIPELINE_LAYER_STATE_UNIT:
    case CG_PIPELINE_LAYER_STATE_TEXTURE_TYPE:
    case CG_PIPELINE_LAYER_STATE_TEXTURE_DATA:
    case CG_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS:
    case CG_PIPELINE_LAYER_STATE_SAMPLER:
        c_return_if_reached();

    case CG_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS:
        _cg_pipeline_snippet_list_copy(&layer->big_state->vertex_snippets,
                                       &authority->big_state->vertex_snippets);
        break;
    case CG_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS:
        _cg_pipeline_snippet_list_copy(
            &layer->big_state->fragment_snippets,
            &authority->big_state->fragment_snippets);
        break;
    }
}

/* NB: If a layer has descendants we can't modify the layer
 * NB: If the layer is owned and the owner has descendants we can't
 *     modify the layer.
 *
 * This function will allocate a new derived layer if you are trying
 * to change the state of a layer with dependants (as described above)
 * so you must always check the return value.
 *
 * If a new layer is returned it will be owned by required_owner.
 * (NB: a layer is always modified with respect to a pipeline - the
 *  "required_owner")
 *
 * required_owner can only by NULL for new, currently unowned layers
 * with no dependants.
 */
cg_pipeline_layer_t *
_cg_pipeline_layer_pre_change_notify(cg_pipeline_t *required_owner,
                                     cg_pipeline_layer_t *layer,
                                     cg_pipeline_layer_state_t change)
{
    cg_texture_unit_t *unit;

    _CG_GET_DEVICE(dev, NULL);

    /* Identify the case where the layer is new with no owner or
     * dependants and so we don't need to do anything. */
    if (c_list_empty(&CG_NODE(layer)->children) && layer->owner == NULL)
        goto init_layer_state;

    /* We only allow a NULL required_owner for new layers */
    c_return_val_if_fail(required_owner != NULL, layer);

    /* Chain up:
     * A modification of a layer is indirectly also a modification of
     * its owner so first make sure to perform any copy-on-write
     * if necessary for the required_owner if it has dependants.
     */
    _cg_pipeline_pre_change_notify(required_owner,
                                   CG_PIPELINE_STATE_LAYERS, NULL, true);

    /* Unlike pipelines; layers are simply considered immutable once
     * they have dependants - either direct children, or another
     * pipeline as an owner.
     */
    if (!c_list_empty(&CG_NODE(layer)->children) ||
        layer->owner != required_owner) {
        cg_pipeline_layer_t *new = _cg_pipeline_layer_copy(layer);
        if (layer->owner == required_owner)
            _cg_pipeline_remove_layer_difference(required_owner, layer, false);
        _cg_pipeline_add_layer_difference(required_owner, new, false);
        cg_object_unref(new);
        layer = new;
        goto init_layer_state;
    }

    /* Note: At this point we know there is only one pipeline dependant on
     * this layer (required_owner), and there are no other layers
     * dependant on this layer so it's ok to modify it. */

    /* NB: Although layers can have private state associated with them
     * by multiple backends we know that a layer can't be *changed* if
     * it has multiple dependants so if we reach here we know we only
     * have a single owner and can only be associated with a single
     * backend that needs to be notified of the layer change...
     */
    if (required_owner->progend != CG_PIPELINE_PROGEND_UNDEFINED) {
        const cg_pipeline_progend_t *progend =
            _cg_pipeline_progends[required_owner->progend];
        const cg_pipeline_fragend_t *fragend =
            _cg_pipeline_fragends[progend->fragend];
        const cg_pipeline_vertend_t *vertend =
            _cg_pipeline_vertends[progend->vertend];

        if (fragend->layer_pre_change_notify)
            fragend->layer_pre_change_notify(dev, required_owner, layer, change);
        if (vertend->layer_pre_change_notify)
            vertend->layer_pre_change_notify(dev, required_owner, layer, change);
        if (progend->layer_pre_change_notify)
            progend->layer_pre_change_notify(dev, required_owner, layer, change);
    }

    /* If the layer being changed is the same as the last layer we
     * flushed to the corresponding texture unit then we keep a track of
     * the changes so we can try to minimize redundant OpenGL calls if
     * the same layer is flushed again.
     */
    unit = _cg_get_texture_unit(dev, _cg_pipeline_layer_get_unit_index(layer));
    if (unit->layer == layer)
        unit->layer_changes_since_flush |= change;

init_layer_state:

    if (required_owner)
        required_owner->age++;

    if (change & CG_PIPELINE_LAYER_STATE_NEEDS_BIG_STATE &&
        !layer->has_big_state) {
        layer->big_state = c_slice_new(cg_pipeline_layer_big_state_t);
        layer->has_big_state = true;
    }

    /* Note: conceptually we have just been notified that a single
     * property value is about to change, but since some state-groups
     * contain multiple properties and 'layer' is about to take over
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
    if (change & CG_PIPELINE_LAYER_STATE_ALL_SPARSE &&
        !(layer->differences & change)) {
        _cg_pipeline_layer_init_multi_property_sparse_state(layer, change);
        layer->differences |= change;
    }

    return layer;
}

static void
_cg_pipeline_layer_unparent(cg_node_t *layer)
{
    /* Chain up */
    _cg_pipeline_node_unparent_real(layer);
}

static void
_cg_pipeline_layer_set_parent(cg_pipeline_layer_t *layer,
                              cg_pipeline_layer_t *parent)
{
    /* Chain up */
    _cg_pipeline_node_set_parent_real(
        CG_NODE(layer), CG_NODE(parent), _cg_pipeline_layer_unparent);
}

cg_pipeline_layer_t *
_cg_pipeline_layer_copy(cg_pipeline_layer_t *src)
{
    cg_pipeline_layer_t *layer = c_slice_new(cg_pipeline_layer_t);

    _cg_pipeline_node_init(CG_NODE(layer));

    layer->owner = NULL;
    layer->index = src->index;
    layer->differences = 0;
    layer->has_big_state = false;

    _cg_pipeline_layer_set_parent(layer, src);

    return _cg_pipeline_layer_object_new(layer);
}

/* XXX: This is duplicated logic; the same as for
 * _cg_pipeline_prune_redundant_ancestry it would be nice to find a
 * way to consolidate these functions! */
void
_cg_pipeline_layer_prune_redundant_ancestry(cg_pipeline_layer_t *layer)
{
    cg_pipeline_layer_t *new_parent = _cg_pipeline_layer_get_parent(layer);

    /* walk up past ancestors that are now redundant and potentially
     * reparent the layer. */
    while (_cg_pipeline_layer_get_parent(new_parent) &&
           (new_parent->differences | layer->differences) == layer->differences)
        new_parent = _cg_pipeline_layer_get_parent(new_parent);

    _cg_pipeline_layer_set_parent(layer, new_parent);
}

/* Determine the mask of differences between two layers.
 *
 * XXX: If layers and pipelines could both be cast to a common Tree
 * type of some kind then we could have a unified
 * compare_differences() function.
 */
unsigned long
_cg_pipeline_layer_compare_differences(cg_pipeline_layer_t *layer0,
                                       cg_pipeline_layer_t *layer1)
{
    c_sllist_t *head0 = NULL;
    c_sllist_t *head1 = NULL;
    cg_pipeline_layer_t *node0;
    cg_pipeline_layer_t *node1;
    int len0 = 0;
    int len1 = 0;
    int count;
    c_sllist_t *common_ancestor0;
    c_sllist_t *common_ancestor1;
    unsigned long layers_difference = 0;

    /* Algorithm:
     *
     * 1) Walk the ancestors of each layer to the root node, adding a
     *    pointer to each ancester node to two linked lists
     *
     * 2) Compare the lists to find the nodes where they start to
     *    differ marking the common_ancestor node for each list.
     *
     * 3) For each list now iterate starting after the common_ancestor
     *    nodes ORing each nodes ->difference mask into the final
     *    differences mask.
     */

    for (node0 = layer0; node0; node0 = _cg_pipeline_layer_get_parent(node0)) {
        c_sllist_t *link = alloca(sizeof(c_sllist_t));
        link->next = head0;
        link->data = node0;
        head0 = link;
        len0++;
    }
    for (node1 = layer1; node1; node1 = _cg_pipeline_layer_get_parent(node1)) {
        c_sllist_t *link = alloca(sizeof(c_sllist_t));
        link->next = head1;
        link->data = node1;
        head1 = link;
        len1++;
    }

    /* NB: There's no point looking at the head entries since we know both
     * layers
     * must have the same default layer as their root node. */
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
        layers_difference |= node0->differences;
    }
    for (head1 = common_ancestor1->next; head1; head1 = head1->next) {
        node1 = head1->data;
        layers_difference |= node1->differences;
    }

    return layers_difference;
}

static bool
layer_state_equal(cg_pipeline_layer_state_index_t state_index,
                  cg_pipeline_layer_t **authorities0,
                  cg_pipeline_layer_t **authorities1,
                  cg_pipeline_layer_state_comparitor_t comparitor)
{
    return comparitor(authorities0[state_index], authorities1[state_index]);
}

void
_cg_pipeline_layer_resolve_authorities(cg_pipeline_layer_t *layer,
                                       unsigned long differences,
                                       cg_pipeline_layer_t **authorities)
{
    unsigned long remaining = differences;
    cg_pipeline_layer_t *authority = layer;

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
    } while ((authority = _cg_pipeline_layer_get_parent(authority)));

    c_assert(remaining == 0);
}

bool
_cg_pipeline_layer_equal(cg_pipeline_layer_t *layer0,
                         cg_pipeline_layer_t *layer1,
                         unsigned long differences_mask,
                         cg_pipeline_eval_flags_t flags)
{
    unsigned long layers_difference;
    cg_pipeline_layer_t *authorities0[CG_PIPELINE_LAYER_STATE_SPARSE_COUNT];
    cg_pipeline_layer_t *authorities1[CG_PIPELINE_LAYER_STATE_SPARSE_COUNT];

    if (layer0 == layer1)
        return true;

    layers_difference = _cg_pipeline_layer_compare_differences(layer0, layer1);

    /* Only compare the sparse state groups requested by the caller... */
    layers_difference &= differences_mask;

    _cg_pipeline_layer_resolve_authorities(
        layer0, layers_difference, authorities0);
    _cg_pipeline_layer_resolve_authorities(
        layer1, layers_difference, authorities1);

    if (layers_difference & CG_PIPELINE_LAYER_STATE_TEXTURE_TYPE) {
        cg_pipeline_layer_state_index_t state_index =
            CG_PIPELINE_LAYER_STATE_TEXTURE_TYPE_INDEX;
        if (!_cg_pipeline_layer_texture_type_equal(
                authorities0[state_index], authorities1[state_index], flags))
            return false;
    }

    if (layers_difference & CG_PIPELINE_LAYER_STATE_TEXTURE_DATA) {
        cg_pipeline_layer_state_index_t state_index =
            CG_PIPELINE_LAYER_STATE_TEXTURE_DATA_INDEX;
        if (!_cg_pipeline_layer_texture_data_equal(
                authorities0[state_index], authorities1[state_index], flags))
            return false;
    }

    if (layers_difference & CG_PIPELINE_LAYER_STATE_SAMPLER &&
        !layer_state_equal(CG_PIPELINE_LAYER_STATE_SAMPLER_INDEX,
                           authorities0,
                           authorities1,
                           _cg_pipeline_layer_sampler_equal))
        return false;

    if (layers_difference & CG_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS &&
        !layer_state_equal(CG_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS_INDEX,
                           authorities0,
                           authorities1,
                           _cg_pipeline_layer_point_sprite_coords_equal))
        return false;

    if (layers_difference & CG_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS &&
        !layer_state_equal(CG_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS_INDEX,
                           authorities0,
                           authorities1,
                           _cg_pipeline_layer_vertex_snippets_equal))
        return false;

    if (layers_difference & CG_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS &&
        !layer_state_equal(CG_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS_INDEX,
                           authorities0,
                           authorities1,
                           _cg_pipeline_layer_fragment_snippets_equal))
        return false;

    return true;
}

static void
_cg_pipeline_layer_free(cg_pipeline_layer_t *layer)
{
    _cg_pipeline_layer_unparent(CG_NODE(layer));

    if (layer->differences & CG_PIPELINE_LAYER_STATE_TEXTURE_DATA &&
        layer->texture != NULL)
        cg_object_unref(layer->texture);

    if (layer->differences & CG_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS)
        _cg_pipeline_snippet_list_free(&layer->big_state->vertex_snippets);

    if (layer->differences & CG_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS)
        _cg_pipeline_snippet_list_free(&layer->big_state->fragment_snippets);

    if (layer->differences & CG_PIPELINE_LAYER_STATE_NEEDS_BIG_STATE)
        c_slice_free(cg_pipeline_layer_big_state_t, layer->big_state);

    c_slice_free(cg_pipeline_layer_t, layer);
}

void
_cg_pipeline_init_default_layers(cg_device_t *dev)
{
    cg_pipeline_layer_t *layer = c_slice_new0(cg_pipeline_layer_t);
    cg_pipeline_layer_big_state_t *big_state =
        c_slice_new0(cg_pipeline_layer_big_state_t);
    cg_pipeline_layer_t *new;

    _cg_pipeline_node_init(CG_NODE(layer));

    layer->index = 0;

    layer->differences = CG_PIPELINE_LAYER_STATE_ALL_SPARSE;

    layer->unit_index = 0;

    layer->texture = NULL;
    layer->texture_type = CG_TEXTURE_TYPE_2D;

    layer->sampler_cache_entry =
        _cg_sampler_cache_get_default_entry(dev->sampler_cache);

    layer->big_state = big_state;
    layer->has_big_state = true;

    big_state->point_sprite_coords = false;

    dev->default_layer_0 = _cg_pipeline_layer_object_new(layer);

    dev->default_layer_n = _cg_pipeline_layer_copy(layer);
    new = _cg_pipeline_set_layer_unit(NULL, dev->default_layer_n, 1);
    c_assert(new == dev->default_layer_n);
    /* Since we passed a newly allocated layer we don't expect that
     * _set_layer_unit() will have to allocate *another* layer. */

    /* Finally we create a dummy dependant for ->default_layer_n which
     * effectively ensures that ->default_layer_n and ->default_layer_0
     * remain immutable.
     */
    dev->dummy_layer_dependant = _cg_pipeline_layer_copy(dev->default_layer_n);
}

void
_cg_pipeline_layer_pre_paint(cg_pipeline_layer_t *layer)
{
    cg_pipeline_layer_t *texture_authority;

    texture_authority = _cg_pipeline_layer_get_authority(
        layer, CG_PIPELINE_LAYER_STATE_TEXTURE_DATA);

    if (texture_authority->texture != NULL) {
        cg_texture_pre_paint_flags_t flags = 0;
        cg_pipeline_filter_t min_filter;
        cg_pipeline_filter_t mag_filter;

        _cg_pipeline_layer_get_filters(layer, &min_filter, &mag_filter);

        if (min_filter == CG_PIPELINE_FILTER_NEAREST_MIPMAP_NEAREST ||
            min_filter == CG_PIPELINE_FILTER_LINEAR_MIPMAP_NEAREST ||
            min_filter == CG_PIPELINE_FILTER_NEAREST_MIPMAP_LINEAR ||
            min_filter == CG_PIPELINE_FILTER_LINEAR_MIPMAP_LINEAR)
            flags |= CG_TEXTURE_NEEDS_MIPMAP;

        _cg_texture_pre_paint(texture_authority->texture, flags);
    }
}
