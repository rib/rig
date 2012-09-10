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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "rig-transition.h"

static RigPropertySpec _rig_transition_prop_specs[] = {
  {
    .name = "progress",
    .type = RIG_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RigTransition, progress)
  },
  { 0 }
};

static RigIntrospectableVTable _rig_transition_introspectable_vtable = {
  rig_simple_introspectable_lookup_property,
  rig_simple_introspectable_foreach_property
};

RigType rig_transition_type;

static void
_rig_transition_type_init (void)
{
  rig_type_init (&rig_transition_type);
  rig_type_add_interface (&rig_transition_type,
                          RIG_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_rig_transition_introspectable_vtable);
  rig_type_add_interface (&rig_transition_type,
                          RIG_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RigTransition, introspectable),
                          NULL); /* no implied vtable */
}

RigTransition *
rig_transition_new (RigContext *context,
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
  transition->context = rig_ref_countable_ref (context);

  rig_object_init (&transition->_parent, &rig_transition_type);

  rig_simple_introspectable_init (transition, _rig_transition_prop_specs, transition->props);

  transition->progress = 0;
  transition->paths = NULL;

#if 0
  rig_property_set_binding (&transition->props[RIG_TRANSITION_PROP_PROGRESS],
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
  GList *l;

  rig_simple_introspectable_destroy (transition);

  for (l = transition->paths; l; l = l->next)
    {
      RigPath *path = l->data;
      rig_path_free (path);
    }

  rig_ref_countable_unref (transition->context);

  g_slice_free (RigTransition, transition);
}

void
rig_transition_add_path (RigTransition *transition,
                         RigPath *path)
{
  transition->paths = g_list_prepend (transition->paths, path);
}

RigPath *
rig_transition_find_path (RigTransition *transition,
                          RigProperty *property)
{
  GList *l;

  for (l = transition->paths; l; l = l->next)
    {
      RigPath *path = l->data;

      if (path->prop == property)
        return path;
    }

  return NULL;
}

RigPath *
rig_transition_get_path (RigTransition *transition,
                         RigObject *object,
                         const char *property_name)
{
  RigProperty *property =
    rig_introspectable_lookup_property (object, property_name);
  RigPath *path;

  if (!property)
    return NULL;

  path = rig_transition_find_path (transition, property);
  if (path)
    return path;

  path = rig_path_new_for_property (transition->context,
                                    &transition->props[RIG_TRANSITION_PROP_PROGRESS],
                                    property);

  rig_transition_add_path (transition, path);

  return path;
}

void
rig_transition_set_progress (RigTransition *transition,
                             float progress)
{
  transition->progress = progress;
  rig_property_dirty (&transition->context->property_ctx,
                      &transition->props[RIG_TRANSITION_PROP_PROGRESS]);
}
