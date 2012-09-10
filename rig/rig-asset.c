/*
 * Rig
 *
 * Copyright (C) 2012  Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <glib.h>

#include <cogl/cogl.h>

#include "rig-context.h"
#include "rig-interfaces.h"
#include "rig-asset.h"

#if 0
enum {
  ASSET_N_PROPS
};
#endif

typedef struct _RigAsset
{
  RigObjectProps _parent;

  int ref_count;

#if 0
  RigSimpleIntrospectableProps introspectable;
  RigProperty props[ASSET_N_PROPS];
#endif

  RigAssetType type;

  char *path;
  CoglTexture *texture;

} RigAsset;

#if 0
static RigPropertySpec _asset_prop_specs[] = {
  { 0 }
};
#endif

#if 0
static RigIntrospectableVTable _asset_introspectable_vtable = {
  rig_simple_introspectable_lookup_property,
  rig_simple_introspectable_foreach_property
};
#endif

static void
_rig_asset_free (void *object)
{
  RigAsset *asset = object;

  if (asset->texture)
    cogl_object_unref (asset->texture);

  if (asset->path)
    g_free (asset->path);

  //rig_simple_introspectable_destroy (asset);

  g_slice_free (RigAsset, asset);
}

static RigRefCountableVTable _rig_asset_ref_countable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_asset_free
};

RigType rig_asset_type;

void
_rig_asset_type_init (void)
{
  rig_type_init (&rig_asset_type);
  rig_type_add_interface (&rig_asset_type,
                           RIG_INTERFACE_ID_REF_COUNTABLE,
                           offsetof (RigAsset, ref_count),
                           &_rig_asset_ref_countable);

#if 0
  rig_type_add_interface (&_asset_type,
                          RIG_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_asset_introspectable_vtable);
  rig_type_add_interface (&_asset_type,
                          RIG_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (Asset, introspectable),
                          NULL); /* no implied vtable */
#endif
}

RigAsset *
rig_asset_new_texture (RigContext *ctx,
                       const char *path)
{
  RigAsset *asset = g_slice_new (RigAsset);
  char *full_path;
  CoglError *error = NULL;

  rig_object_init (&asset->_parent, &rig_asset_type);

  asset->ref_count = 1;

  asset->type = RIG_ASSET_TYPE_TEXTURE;

#ifndef __ANDROID__
  /* TODO: move this non-android logic into rig_load_texture() */
  full_path = g_build_filename (ctx->assets_location, path, NULL);
  asset->texture = rig_load_texture (ctx, full_path, &error);
  g_free (full_path);
#else
  asset->texture = rig_load_texture (ctx, path, &error);
#endif

  if (!asset->texture)
    {
      g_slice_free (RigAsset, asset);
      g_error ("Failed to load asset texture: %s", error->message);
      g_error_free (error);
      return NULL;
    }

  asset->path = g_strdup (path);

  //rig_simple_introspectable_init (asset);

  return asset;
}

RigAssetType
rig_asset_get_type (RigAsset *asset)
{
  return asset->type;
}

const char *
rig_asset_get_path (RigAsset *asset)
{
  return asset->path;
}

CoglTexture *
rig_asset_get_texture (RigAsset *asset)
{
  return asset->texture;
}
