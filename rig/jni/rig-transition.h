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

#ifndef _RUT_TRANSITION_H_
#define _RUT_TRANSITION_H_

#include <rut.h>
#include "rig-path.h"
#include "rut-list.h"

extern RutType rig_transition_type;

typedef struct _RigTransition RigTransition;

enum {
  RUT_TRANSITION_PROP_PROGRESS,
  RUT_TRANSITION_N_PROPS
};

/* State for an individual property that the transition is tracking */
typedef struct
{
  RutProperty *property;

  /* The transition maintains two sets of state for each property. One
   * is a constant value that is used throughout the entire transition
   * and the other is a path whose actual property value depends on
   * the progress of the timeline. Only one of these states will
   * actually be used depending on whether the property is animated.
   * However both states are retained so that if the user toggles the
   * animated button for a property information won't be lost. */

  CoglBool animated;

  /* path may be NULL */
  RigPath *path;
  RutBoxed constant_value;
} RigTransitionPropData;

struct _RigTransition
{
  RutObjectProps _parent;

  int ref_count;

  char *name;

  float progress;

  /* Hash table of RigTransitionProperties. The key is a pointer to
   * the RutProperty indexed using g_direct_hash and the value is the
   * RigTransitionPropData struct */
  GHashTable *properties;

  RutContext *context;

  RutList operation_cb_list;

  RutProperty props[RUT_TRANSITION_N_PROPS];
  RutSimpleIntrospectableProps introspectable;
};

typedef enum
{
  RIG_TRANSITION_OPERATION_ADDED,
  RIG_TRANSITION_OPERATION_REMOVED,
  RIG_TRANSITION_OPERATION_ANIMATED_CHANGED
} RigTransitionOperation;

typedef void
(* RigTransitionOperationCallback) (RigTransition *transition,
                                    RigTransitionOperation op,
                                    RigTransitionPropData *prop_data,
                                    void *user_data);

void
rig_transition_set_progress (RigTransition *transition,
                             float progress);

RigTransitionPropData *
rig_transition_find_prop_data_for_property (RigTransition *transition,
                                            RutProperty *property);

RigTransitionPropData *
rig_transition_get_prop_data_for_property (RigTransition *transition,
                                           RutProperty *property);

RigTransitionPropData *
rig_transition_get_prop_data (RigTransition *transition,
                              RutObject *object,
                              const char *property_name);

RigPath *
rig_transition_get_path_for_property (RigTransition *transition,
                                      RutProperty *property);

RigPath *
rig_transition_get_path (RigTransition *transition,
                         RutObject *object,
                         const char *property_name);

RigTransition *
rig_transition_new (RutContext *context, const char *name);

void
rig_transition_set_name (RigTransition *transition,
                         const char *name);

typedef void
(* RigTransitionForeachPropertyCb) (RigTransitionPropData *prop_data,
                                    void *user_data);

void
rig_transition_foreach_property (RigTransition *transition,
                                 RigTransitionForeachPropertyCb callback,
                                 void *user_data);

void
rig_transition_free (RigTransition *transition);

void
rig_transition_update_property (RigTransition *transition,
                                RutProperty *property);

RutClosure *
rig_transition_add_operation_callback (RigTransition *transition,
                                       RigTransitionOperationCallback callback,
                                       void *user_data,
                                       RutClosureDestroyCallback destroy_cb);

void
rig_transition_set_property_animated (RigTransition *transition,
                                      RutProperty *property,
                                      CoglBool animated);

void
rig_transition_remove_property (RigTransition *transition,
                                RutProperty *property);

#endif /* _RUT_TRANSITION_H_ */
