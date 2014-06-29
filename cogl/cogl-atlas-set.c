/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2013 Intel Corporation.
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
 */

#include <config.h>

#include "cogl-atlas-set.h"
#include "cogl-atlas-set-private.h"
#include "cogl-atlas-private.h"
#include "cogl-closure-list-private.h"
#include "cogl-texture-private.h"
#include "cogl-error-private.h"

static void _cg_atlas_set_free(cg_atlas_set_t *set);

CG_OBJECT_DEFINE(AtlasSet, atlas_set);

static cg_user_data_key_t atlas_private_key;

static void
dissociate_atlases(cg_atlas_set_t *set)
{
    c_slist_t *l;

    /* NB: The set doesn't maintain a reference on the atlases since we don't
     * want to keep them alive if they become empty. */
    for (l = set->atlases; l; l = l->next)
        cg_object_set_user_data(l->data, &atlas_private_key, NULL, NULL);

    c_slist_free(set->atlases);
    set->atlases = NULL;
}

static void
_cg_atlas_set_free(cg_atlas_set_t *set)
{
    dissociate_atlases(set);

    _cg_closure_list_disconnect_all(&set->atlas_closures);

    c_slice_free(cg_atlas_set_t, set);
}

static void
_update_internal_format(cg_atlas_set_t *set)
{
    set->internal_format = _cg_texture_derive_format(
        set->context, CG_PIXEL_FORMAT_ANY, set->components, set->premultiplied);
}

cg_atlas_set_t *
cg_atlas_set_new(cg_context_t *context)
{
    cg_atlas_set_t *set = c_slice_new0(cg_atlas_set_t);

    set->context = context;
    set->atlases = NULL;

    set->components = CG_TEXTURE_COMPONENTS_RGBA;
    set->premultiplied = true;
    _update_internal_format(set);

    set->clear_enabled = false;
    set->migration_enabled = true;

    _cg_list_init(&set->atlas_closures);

    return _cg_atlas_set_object_new(set);
}

void
cg_atlas_set_set_components(cg_atlas_set_t *set,
                            cg_texture_components_t components)
{
    c_return_if_fail(set->atlases == NULL);

    set->components = components;
    _update_internal_format(set);
}

cg_texture_components_t
cg_atlas_set_get_components(cg_atlas_set_t *set)
{
    return set->components;
}

void
cg_atlas_set_set_premultiplied(cg_atlas_set_t *set, bool premultiplied)
{
    c_return_if_fail(set->atlases == NULL);

    set->premultiplied = premultiplied;
    _update_internal_format(set);
}

bool
cg_atlas_set_get_premultiplied(cg_atlas_set_t *set)
{
    return set->premultiplied;
}

void
cg_atlas_set_set_clear_enabled(cg_atlas_set_t *set, bool clear_enabled)
{
    _CG_RETURN_IF_FAIL(set->atlases == NULL);

    set->clear_enabled = clear_enabled;
}

bool
cg_atlas_set_get_clear_enabled(cg_atlas_set_t *set, bool clear_enabled)
{
    return set->clear_enabled;
}

void
cg_atlas_set_set_migration_enabled(cg_atlas_set_t *set,
                                   bool migration_enabled)
{
    _CG_RETURN_IF_FAIL(set->atlases == NULL);

    set->migration_enabled = migration_enabled;
}

bool
cg_atlas_set_get_migration_enabled(cg_atlas_set_t *set)
{
    return set->migration_enabled;
}

cg_atlas_set_atlas_closure_t *
cg_atlas_set_add_atlas_callback(cg_atlas_set_t *set,
                                cg_atlas_set_atlas_callback_t callback,
                                void *user_data,
                                cg_user_data_destroy_callback_t destroy)
{
    return _cg_closure_list_add(
        &set->atlas_closures, callback, user_data, destroy);
}

void
cg_atlas_set_remove_atlas_callback(cg_atlas_set_t *set,
                                   cg_atlas_set_atlas_closure_t *closure)
{
    _cg_closure_disconnect(closure);
}

static void
atlas_destroyed_cb(void *user_data, void *instance)
{
    cg_atlas_set_t *set = user_data;
    cg_atlas_t *atlas = instance;

    set->atlases = c_slist_remove(set->atlases, atlas);
}

cg_atlas_t *
cg_atlas_set_allocate_space(cg_atlas_set_t *set,
                            int width,
                            int height,
                            void *allocation_data)
{
    c_slist_t *l;
    cg_atlas_flags_t flags = 0;
    cg_atlas_t *atlas;

    /* Look for an existing atlas that can hold the texture */
    for (l = set->atlases; l; l = l->next) {
        if (_cg_atlas_allocate_space(l->data, width, height, allocation_data))
            return l->data;
    }

    if (set->clear_enabled)
        flags |= CG_ATLAS_CLEAR_TEXTURE;

    if (!set->migration_enabled)
        flags |= CG_ATLAS_DISABLE_MIGRATION;

    atlas = _cg_atlas_new(set->context, set->internal_format, flags);

    _cg_closure_list_invoke(&set->atlas_closures,
                            cg_atlas_set_atlas_callback_t,
                            set,
                            atlas,
                            CG_ATLAS_SET_EVENT_ADDED);

    CG_NOTE(ATLAS, "Created new atlas for textures: %p", atlas);
    if (!_cg_atlas_allocate_space(atlas, width, height, allocation_data)) {
        _cg_closure_list_invoke(&set->atlas_closures,
                                cg_atlas_set_atlas_callback_t,
                                set,
                                atlas,
                                CG_ATLAS_SET_EVENT_REMOVED);

        /* Ok, this means we really can't add it to an atlas */
        cg_object_unref(atlas);

        return NULL;
    }

    set->atlases = c_slist_prepend(set->atlases, atlas);

    /* Set some data on the atlas so we can get notification when it is
       destroyed in order to remove it from the list. set->atlases
       effectively holds a weak reference. We don't need a strong
       reference because the atlas textures take a reference on the
       atlas so it will stay alive */
    _cg_object_set_user_data(
        (cg_object_t *)atlas, &atlas_private_key, set, atlas_destroyed_cb);

    /* XXX: whatever allocates space in an atlas set is responsible for
     * taking a reference on the corresponding atlas for the allocation
     * otherwise.
     *
     * We want the lifetime of an atlas to be tied to the lifetime of
     * the allocations within the atlas so we don't keep a reference
     * ourselves.
     */
    c_warn_if_fail(atlas->_parent.ref_count != 1);

    cg_object_unref(atlas);

    return atlas;
}

void
cg_atlas_set_foreach(cg_atlas_set_t *atlas_set,
                     cg_atlas_set_foreach_callback_t callback,
                     void *user_data)
{
    c_slist_t *l;

    for (l = atlas_set->atlases; l; l = l->next)
        callback(l->data, user_data);
}
