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

#ifndef _RUT_CONTROLLER_H_
#define _RUT_CONTROLLER_H_

#include <rut.h>
#include "rig-path.h"
#include "rig-types.h"
#include "rut-list.h"

extern RutType rig_controller_type;

typedef struct _RigController RigController;

enum {
  RIG_CONTROLLER_PROP_LABEL,
  RIG_CONTROLLER_PROP_ACTIVE,
  RIG_CONTROLLER_PROP_AUTO_DEACTIVATE,
  RIG_CONTROLLER_PROP_LOOP,
  RIG_CONTROLLER_PROP_RUNNING,
  RIG_CONTROLLER_PROP_LENGTH,
  RIG_CONTROLLER_PROP_ELAPSED,
  RIG_CONTROLLER_PROP_PROGRESS,
  RIG_CONTROLLER_N_PROPS
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
  RigController *controller;

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
  RutClosure *path_change_closure;
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

  RigEngine *engine;
  RutContext *context;

  char *label;

  bool active;
  bool auto_deactivate;

  RutTimeline *timeline;
  double elapsed;

  /* Hash table of RigControllerProperties. The key is a pointer to
   * the RutProperty indexed using g_direct_hash and the value is the
   * RigControllerPropData struct */
  GHashTable *properties;

  RutList operation_cb_list;

  RutProperty props[RIG_CONTROLLER_N_PROPS];
  RutSimpleIntrospectableProps introspectable;
};

typedef enum
{
  RIG_CONTROLLER_OPERATION_ADDED,
  RIG_CONTROLLER_OPERATION_REMOVED,
  RIG_CONTROLLER_OPERATION_METHOD_CHANGED
} RigControllerOperation;

typedef void
(* RigControllerOperationCallback) (RigController *controller,
                                    RigControllerOperation op,
                                    RigControllerPropData *prop_data,
                                    void *user_data);

RigControllerPropData *
rig_controller_find_prop_data_for_property (RigController *controller,
                                            RutProperty *property);

RigPath *
rig_controller_get_path_for_prop_data (RigController *controller,
                                       RigControllerPropData *prop_data);

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
rig_controller_set_label (RutObject *object,
                          const char *label);

const char *
rig_controller_get_label (RutObject *object);

void
rig_controller_set_active (RutObject *controller,
                           bool active);

bool
rig_controller_get_active (RutObject *controller);

void
rig_controller_set_auto_deactivate (RutObject *controller,
                                    bool auto_deactivate);

bool
rig_controller_get_auto_deactivate (RutObject *controller);

void
rig_controller_set_loop (RutObject *controller,
                         bool loop);

bool
rig_controller_get_loop (RutObject *controller);

void
rig_controller_set_running (RutObject *controller,
                            bool running);

bool
rig_controller_get_running (RutObject *controller);

void
rig_controller_set_length (RutObject *controller,
                           float length);

float
rig_controller_get_length (RutObject *controller);

void
rig_controller_set_elapsed (RutObject *controller,
                            double elapsed);

double
rig_controller_get_elapsed (RutObject *controller);

void
rig_controller_set_progress (RutObject *controller,
                             double progress);

double
rig_controller_get_progress (RutObject *controller);

void
rig_controller_add_property (RigController *controller,
                             RutProperty *property);

void
rig_controller_remove_property (RigController *controller,
                                RutProperty *property);

void
rig_controller_set_property_constant (RigController *controller,
                                      RutProperty *property,
                                      RutBoxed *boxed_value);

void
rig_controller_set_property_path (RigController *controller,
                                  RutProperty *property,
                                  RigPath *path);

typedef void
(* RigControllerForeachPropertyCb) (RigControllerPropData *prop_data,
                                    void *user_data);

void
rig_controller_foreach_property (RigController *controller,
                                 RigControllerForeachPropertyCb callback,
                                 void *user_data);

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

typedef void (*RigControllerNodeCallback) (RigNode *node, void *user_data);

void
rig_controller_foreach_node (RigController *controller,
                             RigControllerNodeCallback callback,
                             void *user_data);

void
rig_controller_insert_path_value (RigController *controller,
                                  RutProperty *property,
                                  float t,
                                  const RutBoxed *value);

void
rig_controller_box_path_value (RigController *controller,
                               RutProperty *property,
                               float t,
                               RutBoxed *out);

void
rig_controller_remove_path_value (RigController *controller,
                                  RutProperty *property,
                                  float t);

#endif /* _RUT_CONTROLLER_H_ */
