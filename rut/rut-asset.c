/*
 * Rut
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

#include "rut-context.h"
#include "rut-interfaces.h"
#include "rut-asset.h"

#if 0
enum {
  ASSET_N_PROPS
};
#endif

typedef struct _RutAsset
{
  RutObjectProps _parent;

  int ref_count;

#if 0
  RutSimpleIntrospectableProps introspectable;
  RutProperty props[ASSET_N_PROPS];
#endif

  RutAssetType type;

  char *path;
  CoglTexture *texture;

  GList *directory_tags;

} RutAsset;

#if 0
static RutPropertySpec _asset_prop_specs[] = {
  { 0 }
};
#endif

#if 0
static RutIntrospectableVTable _asset_introspectable_vtable = {
  rut_simple_introspectable_lookup_property,
  rut_simple_introspectable_foreach_property
};
#endif

static void
_rut_asset_free (void *object)
{
  RutAsset *asset = object;

  if (asset->texture)
    cogl_object_unref (asset->texture);

  if (asset->path)
    g_free (asset->path);

  //rut_simple_introspectable_destroy (asset);

  g_slice_free (RutAsset, asset);
}

static RutRefCountableVTable _rut_asset_ref_countable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rut_asset_free
};

RutType rut_asset_type;

void
_rut_asset_type_init (void)
{
  rut_type_init (&rut_asset_type);
  rut_type_add_interface (&rut_asset_type,
                           RUT_INTERFACE_ID_REF_COUNTABLE,
                           offsetof (RutAsset, ref_count),
                           &_rut_asset_ref_countable);

#if 0
  rut_type_add_interface (&_asset_type,
                          RUT_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_asset_introspectable_vtable);
  rut_type_add_interface (&_asset_type,
                          RUT_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (Asset, introspectable),
                          NULL); /* no implied vtable */
#endif
}

RutAsset *
rut_asset_new_texture_full (RutContext *ctx,
                            const char *path,
                            RutAssetType type)
{
  RutAsset *asset = g_slice_new (RutAsset);
  char *full_path;
  CoglError *error = NULL;

  rut_object_init (&asset->_parent, &rut_asset_type);

  asset->ref_count = 1;

  asset->type = type;

#ifndef __ANDROID__
  /* TODO: move this non-android logic into rut_load_texture() */
  full_path = g_build_filename (ctx->assets_location, path, NULL);
  asset->texture = rut_load_texture (ctx, full_path, &error);
  g_free (full_path);
#else
  asset->texture = rut_load_texture (ctx, path, &error);
#endif

  if (!asset->texture)
    {
      g_slice_free (RutAsset, asset);
      g_error ("Failed to load asset texture: %s", error->message);
      g_error_free (error);
      return NULL;
    }

  asset->path = g_strdup (path);

  //rut_simple_introspectable_init (asset);

  return asset;
}

RutAsset *
rut_asset_new_texture (RutContext *ctx,
                       const char *path)
{
  return rut_asset_new_texture_full (ctx, path, RUT_ASSET_TYPE_TEXTURE);
}

RutAsset *
rut_asset_new_normal_map (RutContext *ctx,
                          const char *path)
{
  return rut_asset_new_texture_full (ctx, path, RUT_ASSET_TYPE_NORMAL_MAP);
}

RutAssetType
rut_asset_get_type (RutAsset *asset)
{
  return asset->type;
}

const char *
rut_asset_get_path (RutAsset *asset)
{
  return asset->path;
}

CoglTexture *
rut_asset_get_texture (RutAsset *asset)
{
  return asset->texture;
}

void
rut_asset_set_directory_tags (RutAsset *asset,
                              GList *directory_tags)
{
  asset->directory_tags = directory_tags;
}

CoglBool
rut_asset_has_tag (RutAsset *asset, const char *tag)
{
  GList *l;

  for (l = asset->directory_tags; l; l = l->next)
    if (strcmp (tag, l->data) == 0)
      return TRUE;
  return FALSE;
}
