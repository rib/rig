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

static void
_cogl_atlas_set_free (CoglAtlasSet *set);

COGL_OBJECT_DEFINE (AtlasSet, atlas_set);

static CoglUserDataKey atlas_private_key;

static void
dissociate_atlases (CoglAtlasSet *set)
{
  USList *l;

  /* NB: The set doesn't maintain a reference on the atlases since we don't
   * want to keep them alive if they become empty. */
  for (l = set->atlases; l; l = l->next)
    cogl_object_set_user_data (l->data, &atlas_private_key, NULL, NULL);

  u_slist_free (set->atlases);
  set->atlases = NULL;
}

static void
_cogl_atlas_set_free (CoglAtlasSet *set)
{
  dissociate_atlases (set);

  _cogl_closure_list_disconnect_all (&set->atlas_closures);

  u_slice_free (CoglAtlasSet, set);
}

static void
_update_internal_format (CoglAtlasSet *set)
{
  set->internal_format = _cogl_texture_derive_format (set->context,
                                                      COGL_PIXEL_FORMAT_ANY,
                                                      set->components,
                                                      set->premultiplied);
}

CoglAtlasSet *
cogl_atlas_set_new (CoglContext *context)
{
  CoglAtlasSet *set = u_slice_new0 (CoglAtlasSet);

  set->context = context;
  set->atlases = NULL;

  set->components = COGL_TEXTURE_COMPONENTS_RGBA;
  set->premultiplied = TRUE;
  _update_internal_format (set);

  set->clear_enabled = FALSE;
  set->migration_enabled = TRUE;

  _cogl_list_init (&set->atlas_closures);

  return _cogl_atlas_set_object_new (set);
}

void
cogl_atlas_set_set_components (CoglAtlasSet *set,
                               CoglTextureComponents components)
{
  u_return_if_fail (set->atlases == NULL);

  set->components = components;
  _update_internal_format (set);
}

CoglTextureComponents
cogl_atlas_set_get_components (CoglAtlasSet *set)
{
  return set->components;
}

void
cogl_atlas_set_set_premultiplied (CoglAtlasSet *set,
                                  CoglBool premultiplied)
{
  u_return_if_fail (set->atlases == NULL);

  set->premultiplied = premultiplied;
  _update_internal_format (set);
}

CoglBool
cogl_atlas_set_get_premultiplied (CoglAtlasSet *set)
{
  return set->premultiplied;
}

void
cogl_atlas_set_set_clear_enabled (CoglAtlasSet *set,
                                  CoglBool clear_enabled)
{
  _COGL_RETURN_IF_FAIL (set->atlases == NULL);

  set->clear_enabled = clear_enabled;
}

CoglBool
cogl_atlas_set_get_clear_enabled (CoglAtlasSet *set,
                                  CoglBool clear_enabled)
{
  return set->clear_enabled;
}

void
cogl_atlas_set_set_migration_enabled (CoglAtlasSet *set,
                                      CoglBool migration_enabled)
{
  _COGL_RETURN_IF_FAIL (set->atlases == NULL);

  set->migration_enabled = migration_enabled;
}

CoglBool
cogl_atlas_set_get_migration_enabled (CoglAtlasSet *set)
{
  return set->migration_enabled;
}

CoglAtlasSetAtlasClosure *
cogl_atlas_set_add_atlas_callback (CoglAtlasSet *set,
                                   CoglAtlasSetAtlasCallback callback,
                                   void *user_data,
                                   CoglUserDataDestroyCallback destroy)
{
  return _cogl_closure_list_add (&set->atlas_closures,
                                 callback,
                                 user_data,
                                 destroy);
}

void
cogl_atlas_set_remove_atlas_callback (CoglAtlasSet *set,
                                      CoglAtlasSetAtlasClosure *closure)
{
  _cogl_closure_disconnect (closure);
}

static void
atlas_destroyed_cb (void *user_data, void *instance)
{
  CoglAtlasSet *set = user_data;
  CoglAtlas *atlas = instance;

  set->atlases = u_slist_remove (set->atlases, atlas);
}

CoglAtlas *
cogl_atlas_set_allocate_space (CoglAtlasSet *set,
                               int width,
                               int height,
                               void *allocation_data)
{
  USList *l;
  CoglAtlasFlags flags = 0;
  CoglAtlas *atlas;

  /* Look for an existing atlas that can hold the texture */
  for (l = set->atlases; l; l = l->next)
    {
      if (_cogl_atlas_allocate_space (l->data, width, height, allocation_data))
        return l->data;
    }

  if (set->clear_enabled)
    flags |= COGL_ATLAS_CLEAR_TEXTURE;

  if (!set->migration_enabled)
    flags |= COGL_ATLAS_DISABLE_MIGRATION;

  atlas = _cogl_atlas_new (set->context,
                           set->internal_format,
                           flags);

  _cogl_closure_list_invoke (&set->atlas_closures,
                             CoglAtlasSetAtlasCallback,
                             set,
                             atlas,
                             COGL_ATLAS_SET_EVENT_ADDED);

  COGL_NOTE (ATLAS, "Created new atlas for textures: %p", atlas);
  if (!_cogl_atlas_allocate_space (atlas, width, height, allocation_data))
    {
      _cogl_closure_list_invoke (&set->atlas_closures,
                                 CoglAtlasSetAtlasCallback,
                                 set,
                                 atlas,
                                 COGL_ATLAS_SET_EVENT_REMOVED);

      /* Ok, this means we really can't add it to an atlas */
      cogl_object_unref (atlas);

      return NULL;
    }

  set->atlases = u_slist_prepend (set->atlases, atlas);

  /* Set some data on the atlas so we can get notification when it is
     destroyed in order to remove it from the list. set->atlases
     effectively holds a weak reference. We don't need a strong
     reference because the atlas textures take a reference on the
     atlas so it will stay alive */
  _cogl_object_set_user_data ((CoglObject *)atlas,
                              &atlas_private_key,
                              set,
                              atlas_destroyed_cb);

  /* XXX: whatever allocates space in an atlas set is responsible for
   * taking a reference on the corresponding atlas for the allocation
   * otherwise.
   *
   * We want the lifetime of an atlas to be tied to the lifetime of
   * the allocations within the atlas so we don't keep a reference
   * ourselves.
   */
  u_warn_if_fail (atlas->_parent.ref_count != 1);

  cogl_object_unref (atlas);

  return atlas;
}

void
cogl_atlas_set_foreach (CoglAtlasSet *atlas_set,
                        CoglAtlasSetForeachCallback callback,
                        void *user_data)
{
  USList *l;

  for (l = atlas_set->atlases; l; l = l->next)
    callback (l->data, user_data);
}
