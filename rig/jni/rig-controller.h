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
#include "rig-types.h"
#include "rut-list.h"

extern RutType rig_controller_type;

typedef struct _RigController RigController;

enum {
  RUT_TRANSITION_PROP_PROGRESS,
  RUT_TRANSITION_N_PROPS
};

typedef enum _RigControllerMethod
{
  RIG_CONTROLLER_METHOD_CONSTANT,
  RIG_CONTROLLER_METHOD_PATH,
  RIG_CONTROLLER_METHOD_BINDING,
} RigControllerMethod;

/* State for an individual property that the controller is tracking */
typedef struct
{
  RutProperty *property;

  /* The controller support 3 "methods" of control for any property. One is a
   * constant value, another is a path whereby the property value depends on
   * the progress through the path and lastly there can be a C expression that
   * may update the property based on a number of other dependency properties.
   *
   * Only one of these methods will actually be used depending on the value of
   * the 'method' member. However all the states are retained so that if the
   * user changes the method then information won't be lost.
   */

  RigControllerMethod method;

  /* path may be NULL */
  RigPath *path;
  RutBoxed constant_value;

  /* depenencies and c_expression may be NULL */
  RutProperty **dependencies;
  int n_dependencies;
  char *c_expression;
} RigControllerPropData;

struct _RigController
{
  RutObjectProps _parent;

  int ref_count;

  char *name;

  float progress;

  /* Hash table of RigControllerProperties. The key is a pointer to
   * the RutProperty indexed using g_direct_hash and the value is the
   * RigControllerPropData struct */
  GHashTable *properties;

  RigEngine *engine;
  RutContext *context;

  RutList operation_cb_list;

  RutProperty props[RUT_TRANSITION_N_PROPS];
  RutSimpleIntrospectableProps introspectable;
};

typedef enum
{
  RIG_TRANSITION_OPERATION_ADDED,
  RIG_TRANSITION_OPERATION_REMOVED,
  RIG_TRANSITION_OPERATION_METHOD_CHANGED
} RigControllerOperation;

typedef void
(* RigControllerOperationCallback) (RigController *controller,
                                    RigControllerOperation op,
                                    RigControllerPropData *prop_data,
                                    void *user_data);

void
rig_controller_set_progress (RigController *controller,
                             float progress);

RigControllerPropData *
rig_controller_find_prop_data_for_property (RigController *controller,
                                            RutProperty *property);

RigControllerPropData *
rig_controller_get_prop_data_for_property (RigController *controller,
                                           RutProperty *property);

RigControllerPropData *
rig_controller_get_prop_data (RigController *controller,
                              RutObject *object,
                              const char *property_name);

RigPath *
rig_controller_get_path_for_property (RigController *controller,
                                      RutProperty *property);

RigPath *
rig_controller_get_path (RigController *controller,
                         RutObject *object,
                         const char *property_name);

RigController *
rig_controller_new (RigEngine *engine, const char *name);

void
rig_controller_set_name (RigController *controller,
                         const char *name);

typedef void
(* RigControllerForeachPropertyCb) (RigControllerPropData *prop_data,
                                    void *user_data);

void
rig_controller_foreach_property (RigController *controller,
                                 RigControllerForeachPropertyCb callback,
                                 void *user_data);

void
rig_controller_free (RigController *controller);

void
rig_controller_update_property (RigController *controller,
                                RutProperty *property);

RutClosure *
rig_controller_add_operation_callback (RigController *controller,
                                       RigControllerOperationCallback callback,
                                       void *user_data,
                                       RutClosureDestroyCallback destroy_cb);

void
rig_controller_set_property_method (RigController *controller,
                                    RutProperty *property,
                                    RigControllerMethod method);

void
rig_controller_set_property_binding (RigController *controller,
                                     RutProperty *property,
                                     const char *c_expression,
                                     RutProperty **dependencies,
                                     int n_dependencies);

void
rig_controller_remove_property (RigController *controller,
                                RutProperty *property);

#endif /* _RUT_TRANSITION_H_ */
