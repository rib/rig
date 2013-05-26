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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "rig-controller.h"

static RutPropertySpec _rig_controller_prop_specs[] = {
  {
    .name = "progress",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RigController, progress)
  },
  { 0 }
};

static void
_rig_controller_free (RigController *controller)
{
  rut_closure_list_disconnect_all (&controller->operation_cb_list);

  rut_simple_introspectable_destroy (controller);

  g_hash_table_destroy (controller->properties);

  rut_refable_unref (controller->context);

  g_free (controller->name);

  g_slice_free (RigController, controller);
}

RutType rig_controller_type;

static void
_rig_controller_type_init (void)
{
  static RutRefCountableVTable refable_vtable = {
      rut_refable_simple_ref,
      rut_refable_simple_unref,
      _rig_controller_free
  };

  static RutIntrospectableVTable introspectable_vtable = {
      rut_simple_introspectable_lookup_property,
      rut_simple_introspectable_foreach_property
  };

  RutType *type = &rig_controller_type;
#define TYPE RigController

  rut_type_init (type, G_STRINGIFY (TYPE));
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (TYPE, ref_count),
                          &refable_vtable);
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

static void
free_prop_data_cb (void *user_data)
{
  RigControllerPropData *prop_data = user_data;

  if (prop_data->path)
    rut_refable_unref (prop_data->path);

  rut_boxed_destroy (&prop_data->constant_value);

  g_slice_free (RigControllerPropData, prop_data);
}

RigController *
rig_controller_new (RutContext *context,
                    const char *name)
{
  RigController *controller;
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rig_controller_type_init ();

      initialized = TRUE;
    }

  controller = g_slice_new0 (RigController);

  controller->ref_count = 1;

  controller->name = g_strdup (name);

  controller->context = rut_refable_ref (context);

  rut_object_init (&controller->_parent, &rig_controller_type);

  rut_list_init (&controller->operation_cb_list);

  rut_simple_introspectable_init (controller, _rig_controller_prop_specs, controller->props);

  controller->progress = 0;

  controller->properties = g_hash_table_new_full (g_direct_hash,
                                                  g_direct_equal,
                                                  NULL, /* key_destroy */
                                                  free_prop_data_cb);

  return controller;
}

void
rig_controller_set_name (RigController *controller,
                         const char *name)
{

}

RigControllerPropData *
rig_controller_find_prop_data_for_property (RigController *controller,
                                            RutProperty *property)
{
  return g_hash_table_lookup (controller->properties, property);
}

RigControllerPropData *
rig_controller_get_prop_data_for_property (RigController *controller,
                                           RutProperty *property)
{
  RigControllerPropData *prop_data;

  prop_data = rig_controller_find_prop_data_for_property (controller, property);

  if (prop_data == NULL)
    {
      prop_data = g_slice_new (RigControllerPropData);
      prop_data->animated = FALSE;
      prop_data->property = property;
      prop_data->path = NULL;

      rut_property_box (property, &prop_data->constant_value);

      g_hash_table_insert (controller->properties,
                           property,
                           prop_data);

      rut_closure_list_invoke (&controller->operation_cb_list,
                               RigControllerOperationCallback,
                               controller,
                               RIG_TRANSITION_OPERATION_ADDED,
                               prop_data);
    }

  return prop_data;
}

RigControllerPropData *
rig_controller_get_prop_data (RigController *controller,
                              RutObject *object,
                              const char *property_name)
{
  RutProperty *property =
    rut_introspectable_lookup_property (object, property_name);

  if (!property)
    return NULL;

  return rig_controller_get_prop_data_for_property (controller, property);
}

RigPath *
rig_controller_find_path (RigController *controller,
                          RutProperty *property)
{
  RigControllerPropData *prop_data;

  prop_data = rig_controller_find_prop_data_for_property (controller, property);

  return prop_data ? prop_data->path : NULL;
}

static RigPath *
rig_controller_get_path_for_prop_data (RigController *controller,
                                       RigControllerPropData *prop_data)
{
  if (prop_data->path == NULL)
      prop_data->path = rig_path_new (controller->context,
                                      prop_data->property->spec->type);

  return prop_data->path;
}

RigPath *
rig_controller_get_path_for_property (RigController *controller,
                                      RutProperty *property)
{
  RigControllerPropData *prop_data =
    rig_controller_get_prop_data_for_property (controller,
                                               property);

  if (!prop_data)
    return NULL;

  return rig_controller_get_path_for_prop_data (controller, prop_data);
}

RigPath *
rig_controller_get_path (RigController *controller,
                         RutObject *object,
                         const char *property_name)
{
  RigControllerPropData *prop_data =
    rig_controller_get_prop_data (controller, object, property_name);

  if (!prop_data)
    return NULL;

  return rig_controller_get_path_for_prop_data (controller, prop_data);
}

static void
update_progress_cb (RigControllerPropData *prop_data,
                    void *user_data)
{
  RigController *controller = user_data;

  if (prop_data->animated && prop_data->path)
    rig_path_lerp_property (prop_data->path,
                            prop_data->property,
                            controller->progress);
}

void
rig_controller_set_progress (RigController *controller,
                             float progress)
{
  controller->progress = progress;
  rut_property_dirty (&controller->context->property_ctx,
                      &controller->props[RUT_TRANSITION_PROP_PROGRESS]);

  rig_controller_foreach_property (controller,
                                   update_progress_cb,
                                   controller);
}

typedef struct
{
  RigControllerForeachPropertyCb callback;
  void *user_data;
} ForeachPathData;

static void
foreach_path_cb (void *key,
                 void *value,
                 void *user_data)
{
  RigControllerPropData *prop_data = value;
  ForeachPathData *foreach_data = user_data;

  foreach_data->callback (prop_data, foreach_data->user_data);
}

void
rig_controller_foreach_property (RigController *controller,
                                 RigControllerForeachPropertyCb callback,
                                 void *user_data)
{
  ForeachPathData foreach_data;

  foreach_data.callback = callback;
  foreach_data.user_data = user_data;

  g_hash_table_foreach (controller->properties,
                        foreach_path_cb,
                        &foreach_data);
}

void
rig_controller_update_property (RigController *controller,
                                RutProperty *property)
{
  RigControllerPropData *prop_data =
    rig_controller_find_prop_data_for_property (controller, property);

  /* Update the given property depending on what the controller thinks
   * it should currently be. This will either be calculated by
   * interpolating the path for the property or by using the constant
   * value depending on whether the property is animated */

  if (prop_data)
    {
      if (prop_data->animated)
        {
          if (prop_data->path)
            rig_path_lerp_property (prop_data->path,
                                    property,
                                    controller->progress);
        }
      else
        rut_property_set_boxed (&controller->context->property_ctx,
                                property,
                                &prop_data->constant_value);
    }
}

RutClosure *
rig_controller_add_operation_callback (RigController *controller,
                                       RigControllerOperationCallback callback,
                                       void *user_data,
                                       RutClosureDestroyCallback destroy_cb)
{
  return rut_closure_list_add (&controller->operation_cb_list,
                               callback,
                               user_data,
                               destroy_cb);
}

void
rig_controller_set_property_animated (RigController *controller,
                                      RutProperty *property,
                                      CoglBool animated)
{
  RigControllerPropData *prop_data;

  if (animated)
    {
      prop_data =
        rig_controller_get_prop_data_for_property (controller, property);
    }
  else
    {
      /* If the animated state is being disabled then we don't want to
       * create the property state if doesn't already exist */
      prop_data =
        rig_controller_find_prop_data_for_property (controller, property);

      if (prop_data == NULL)
        return;
    }

  if (animated != prop_data->animated)
    {
      prop_data->animated = animated;
      rut_closure_list_invoke (&controller->operation_cb_list,
                               RigControllerOperationCallback,
                               controller,
                               RIG_TRANSITION_OPERATION_ANIMATED_CHANGED,
                               prop_data);
    }
}

void
rig_controller_remove_property (RigController *controller,
                                RutProperty *property)
{
  RigControllerPropData *prop_data =
    rig_controller_find_prop_data_for_property (controller, property);

  if (prop_data)
    {
      rut_closure_list_invoke (&controller->operation_cb_list,
                               RigControllerOperationCallback,
                               controller,
                               RIG_TRANSITION_OPERATION_REMOVED,
                               prop_data);

      g_hash_table_remove (controller->properties, property);
    }
}
