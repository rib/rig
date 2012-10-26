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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "rig-transition.h"

static RutPropertySpec _rig_transition_prop_specs[] = {
  {
    .name = "progress",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RigTransition, progress)
  },
  { 0 }
};

static RutIntrospectableVTable _rig_transition_introspectable_vtable = {
  rut_simple_introspectable_lookup_property,
  rut_simple_introspectable_foreach_property
};

RutType rig_transition_type;

static void
_rig_transition_type_init (void)
{
  rut_type_init (&rig_transition_type);
  rut_type_add_interface (&rig_transition_type,
                          RUT_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_rig_transition_introspectable_vtable);
  rut_type_add_interface (&rig_transition_type,
                          RUT_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RigTransition, introspectable),
                          NULL); /* no implied vtable */
}

static void
free_prop_data_cb (void *data)
{
  RigTransitionPropData *prop_data = data;

  if (prop_data->path)
    rut_refable_unref (prop_data->path);

  rut_boxed_destroy (&prop_data->constant_value);

  g_slice_free (RigTransitionPropData, prop_data);
}

RigTransition *
rig_transition_new (RutContext *context,
                    uint32_t id)
{
  //CoglError *error = NULL;
  RigTransition *transition;
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rig_transition_type_init ();

      initialized = TRUE;
    }

  transition = g_slice_new0 (RigTransition);

  transition->id = id;
  transition->context = rut_refable_ref (context);

  rut_object_init (&transition->_parent, &rig_transition_type);

  rut_simple_introspectable_init (transition, _rig_transition_prop_specs, transition->props);

  transition->progress = 0;

  transition->properties = g_hash_table_new_full (g_direct_hash,
                                                  g_direct_equal,
                                                  NULL, /* key_destroy */
                                                  free_prop_data_cb);

#if 0
  rut_property_set_binding (&transition->props[RUT_TRANSITION_PROP_PROGRESS],
                            update_transition_progress_cb,
                            data,
                            data->timeline_elapsed,
                            NULL);
#endif

  return transition;
}

void
rig_transition_free (RigTransition *transition)
{
  rut_simple_introspectable_destroy (transition);

  g_hash_table_destroy (transition->properties);

  rut_refable_unref (transition->context);

  g_slice_free (RigTransition, transition);
}

RigTransitionPropData *
rig_transition_find_prop_data (RigTransition *transition,
                               RutProperty *property)
{
  return g_hash_table_lookup (transition->properties, property);
}

RigTransitionPropData *
rig_transition_get_prop_data_for_property (RigTransition *transition,
                                           RutProperty *property)
{
  RigTransitionPropData *prop_data;

  prop_data = rig_transition_find_prop_data (transition, property);

  if (prop_data == NULL)
    {
      prop_data = g_slice_new (RigTransitionPropData);
      prop_data->property = property;
      prop_data->path = NULL;

      rut_property_box (property, &prop_data->constant_value);

      g_hash_table_insert (transition->properties,
                           property,
                           prop_data);
    }

  return prop_data;
}

RigTransitionPropData *
rig_transition_get_prop_data (RigTransition *transition,
                              RutObject *object,
                              const char *property_name)
{
  RutProperty *property =
    rut_introspectable_lookup_property (object, property_name);

  if (!property)
    return NULL;

  return rig_transition_get_prop_data_for_property (transition, property);
}

RigPath *
rig_transition_find_path (RigTransition *transition,
                          RutProperty *property)
{
  RigTransitionPropData *prop_data;

  prop_data = rig_transition_find_prop_data (transition, property);

  return prop_data ? prop_data->path : NULL;
}

static RigPath *
rig_transition_get_path_for_prop_data (RigTransition *transition,
                                       RigTransitionPropData *prop_data)
{
  if (prop_data->path == NULL)
      prop_data->path = rig_path_new (transition->context,
                                      prop_data->property->spec->type);

  return prop_data->path;
}

RigPath *
rig_transition_get_path_for_property (RigTransition *transition,
                                      RutProperty *property)
{
  RigTransitionPropData *prop_data =
    rig_transition_get_prop_data_for_property (transition,
                                               property);

  if (!prop_data)
    return NULL;

  return rig_transition_get_path_for_prop_data (transition, prop_data);
}

RigPath *
rig_transition_get_path (RigTransition *transition,
                         RutObject *object,
                         const char *property_name)
{
  RigTransitionPropData *prop_data =
    rig_transition_get_prop_data (transition, object, property_name);

  if (!prop_data)
    return NULL;

  return rig_transition_get_path_for_prop_data (transition, prop_data);
}

static void
update_progress_cb (RutProperty *property,
                    RigPath *path,
                    const RutBoxed *constant_value,
                    void *user_data)
{
  RigTransition *transition = user_data;

  if (property->animated && path)
    rig_path_lerp_property (path, property, transition->progress);
}

void
rig_transition_set_progress (RigTransition *transition,
                             float progress)
{
  transition->progress = progress;
  rut_property_dirty (&transition->context->property_ctx,
                      &transition->props[RUT_TRANSITION_PROP_PROGRESS]);

  rig_transition_foreach_property (transition,
                                   update_progress_cb,
                                   transition);
}

typedef struct
{
  RigTransitionForeachPropertyCb callback;
  void *user_data;
} ForeachPathData;

static void
foreach_path_cb (void *key,
                 void *value,
                 void *user_data)
{
  RigTransitionPropData *prop_data = value;
  ForeachPathData *data = user_data;

  data->callback (prop_data->property,
                  prop_data->path,
                  &prop_data->constant_value,
                  data->user_data);
}

void
rig_transition_foreach_property (RigTransition *transition,
                                 RigTransitionForeachPropertyCb callback,
                                 void *user_data)
{
  ForeachPathData data;

  data.callback = callback;
  data.user_data = user_data;

  g_hash_table_foreach (transition->properties,
                        foreach_path_cb,
                        &data);
}

void
rig_transition_update_property (RigTransition *transition,
                                RutProperty *property)
{
  RigTransitionPropData *prop_data =
    rig_transition_find_prop_data (transition, property);

  /* Update the given property depending on what the transition thinks
   * it should currently be. This will either be calculated by
   * interpolating the path for the property or by using the constant
   * value depending on whether the property is animated */

  if (prop_data)
    {
      if (property->animated)
        {
          if (prop_data->path)
            rig_path_lerp_property (prop_data->path,
                                    property,
                                    transition->progress);
        }
      else
        rut_property_set_boxed (&transition->context->property_ctx,
                                property,
                                &prop_data->constant_value);
    }
}
