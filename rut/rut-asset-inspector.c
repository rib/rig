/*
 * Rig
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
 */

#include <config.h>

#include <glib.h>

#include "rut-shim.h"
#include "rut-asset-inspector.h"

enum {
  RUT_ASSET_INSPECTOR_PROP_ASSET,
  RUT_ASSET_INSPECTOR_N_PROPS
};

struct _RutAssetInspector
{
  RutObjectProps _parent;

  RutContext *ctx;

  RutAssetType asset_type;
  RutAsset *asset;
  RutShim *shim;
  RutStack *stack;

  RutGraphableProps graphable;

  RutSimpleIntrospectableProps introspectable;
  RutProperty properties[RUT_ASSET_INSPECTOR_N_PROPS];

  int ref_count;
};

static RutPropertySpec _rut_asset_inspector_prop_specs[] = {
  {
    .name = "asset",
    .nick = "Asset",
    .type = RUT_PROPERTY_TYPE_ASSET,
    .getter.object_type = rut_asset_inspector_get_asset,
    .setter.object_type = rut_asset_inspector_set_asset,
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .animatable = FALSE
  },

  { NULL }
};

static void
_rut_asset_inspector_free (void *object)
{
  RutAssetInspector *asset_inspector = object;

  rut_asset_inspector_set_asset (asset_inspector, NULL);

  rut_graphable_destroy (asset_inspector);

  rut_simple_introspectable_destroy (asset_inspector);

  g_slice_free (RutAssetInspector, asset_inspector);
}

RutType rut_asset_inspector_type;

static void
_rut_asset_inspector_init_type (void)
{
  static RutRefCountableVTable refable_vtable = {
      rut_refable_simple_ref,
      rut_refable_simple_unref,
      _rut_asset_inspector_free
  };
  static RutGraphableVTable graphable_vtable = {
      NULL, /* child removed */
      NULL, /* child added */
      NULL /* parent changed */
  };
  static RutSizableVTable sizable_vtable = {
      rut_composite_sizable_set_size,
      rut_composite_sizable_get_size,
      rut_composite_sizable_get_preferred_width,
      rut_composite_sizable_get_preferred_height,
      rut_composite_sizable_add_preferred_size_callback
  };
  static RutIntrospectableVTable introspectable_vtable = {
      rut_simple_introspectable_lookup_property,
      rut_simple_introspectable_foreach_property
  };

  RutType *type = &rut_asset_inspector_type;
#define TYPE RutAssetInspector

  rut_type_init (type, G_STRINGIFY (TYPE));

  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (TYPE, ref_count),
                          &refable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (TYPE, graphable),
                          &graphable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &sizable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_COMPOSITE_SIZABLE,
                          offsetof (TYPE, shim),
                          NULL); /* no vtable */
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &introspectable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (TYPE, introspectable),
                          NULL); /* no implied vtable */

#undef TYPE
}

#if 0
static RutInputEventStatus
input_cb (RutInputRegion *region,
          RutInputEvent *event,
          void *user_data)
{
  RutAssetInspector *asset_inspector = user_data;

  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION &&
      rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_UP)
    {
      return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}
#endif

RutAssetInspector *
rut_asset_inspector_new (RutContext *ctx, RutAssetType asset_type)
{
  RutAssetInspector *asset_inspector = g_slice_new0 (RutAssetInspector);
  static CoglBool initialized = FALSE;
  RutShim *shim;
  RutStack *stack;

  if (initialized == FALSE)
    {
      _rut_asset_inspector_init_type ();

      initialized = TRUE;
    }

  asset_inspector->ref_count = 1;
  asset_inspector->ctx = ctx;

  rut_object_init (&asset_inspector->_parent, &rut_asset_inspector_type);

  rut_simple_introspectable_init (asset_inspector,
                                  _rut_asset_inspector_prop_specs,
                                  asset_inspector->properties);

  rut_graphable_init (asset_inspector);

  asset_inspector->asset_type = asset_type;

  shim = rut_shim_new (asset_inspector->ctx, 100, 100);
  rut_graphable_add_child (asset_inspector, shim);
  asset_inspector->shim = shim;
  rut_refable_unref (shim);

  stack = rut_stack_new (asset_inspector->ctx, 0, 0);
  rut_shim_set_child (shim, stack);
  asset_inspector->stack = stack;
  rut_refable_unref (stack);

#if 0
  asset_inspector->input_region =
    rut_input_region_new_rectangle (0, 0, 0, 0,
                                    input_cb,
                                    asset_inspector);
  rut_stack_add (stack, asset_inspector->input_region);
  rut_refable_unref (asset_inspector->input_region);
#endif

  return asset_inspector;
}

RutObject *
rut_asset_inspector_get_asset (RutObject *object)
{
  RutAssetInspector *asset_inspector = object;
  return asset_inspector->asset;
}

void
rut_asset_inspector_set_asset (RutObject *object, RutObject *asset_object)
{
  RutAssetInspector *asset_inspector = object;
  RutAsset *asset = asset_object;
  CoglTexture *texture;

  if (asset_inspector->asset == asset)
    return;

  if (asset_inspector->asset)
    rut_refable_unref (asset_inspector->asset);
  asset_inspector->asset = rut_refable_ref (asset);

  texture = rut_asset_get_texture (asset);
  if (texture)
    {
      RutImage *image = rut_image_new (asset_inspector->ctx, texture);
      rut_stack_add (asset_inspector->stack, image);
      rut_refable_unref (image);
    }

  rut_property_dirty (&asset_inspector->ctx->property_ctx,
                      &asset_inspector->properties[RUT_ASSET_INSPECTOR_PROP_ASSET]);
}
