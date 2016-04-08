/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012  Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _RUT_CONTROLLER_H_
#define _RUT_CONTROLLER_H_

#include <rut.h>

#include "rig-path.h"
#include "rig-types.h"
#include "rig-introspectable.h"
#include "rig-binding.h"
#include "rig-timeline.h"

extern rut_type_t rig_controller_type;

typedef struct _rig_controller_t rig_controller_t;

enum {
    RIG_CONTROLLER_PROP_LABEL,
    RIG_CONTROLLER_PROP_ACTIVE,
    RIG_CONTROLLER_PROP_SUSPENDED, /* private */
    RIG_CONTROLLER_PROP_AUTO_DEACTIVATE,
    RIG_CONTROLLER_PROP_LOOP,
    RIG_CONTROLLER_PROP_RUNNING,
    RIG_CONTROLLER_PROP_LENGTH,
    RIG_CONTROLLER_PROP_ELAPSED,
    RIG_CONTROLLER_PROP_PROGRESS,
    RIG_CONTROLLER_N_PROPS
};

typedef enum _rig_controller_method_t {
    RIG_CONTROLLER_METHOD_CONSTANT,
    RIG_CONTROLLER_METHOD_PATH,
    RIG_CONTROLLER_METHOD_BINDING,
} rig_controller_method_t;

/* State for an individual property that the controller is tracking */
typedef struct {
    rig_controller_t *controller;

    rig_property_t *property;

    /* The controller support 3 "methods" of control for any property. One is a
     * constant value, another is a path whereby the property value depends on
     * the progress through the path and lastly there can be a C expression that
     * may update the property based on a number of other dependency properties.
     *
     * Only one of these methods will actually be used depending on the value of
     * the 'method' member. However all the states are retained so that if the
     * user changes the method then information won't be lost.
     */

    rig_controller_method_t method;

    /* path may be NULL */
    rig_path_t *path;
    rut_closure_t *path_change_closure;
    rut_boxed_t constant_value;

    /* depenencies and c_expression may be NULL */
    rig_binding_t *binding;

    unsigned int active : 1;
} rig_controller_prop_data_t;

struct _rig_controller_t {
    rut_object_base_t _base;

    rig_engine_t *engine;
    rut_shell_t *shell;

    char *label;

    bool active;
    bool auto_deactivate;

    bool suspended;

    rig_timeline_t *timeline;
    double elapsed;

    c_hash_table_t *properties;

    c_list_t operation_cb_list;

    rig_property_t props[RIG_CONTROLLER_N_PROPS];
    rig_introspectable_props_t introspectable;
};

typedef enum {
    RIG_CONTROLLER_OPERATION_ADDED,
    RIG_CONTROLLER_OPERATION_REMOVED,
    RIG_CONTROLLER_OPERATION_METHOD_CHANGED
} rig_controller_operation_t;

typedef void (*rig_controller_operation_callback_t)(
    rig_controller_t *controller,
    rig_controller_operation_t op,
    rig_controller_prop_data_t *prop_data,
    void *user_data);

rig_controller_prop_data_t *
rig_controller_find_prop_data_for_property(rig_controller_t *controller,
                                           rig_property_t *property);

rig_path_t *
rig_controller_get_path_for_prop_data(rig_controller_t *controller,
                                      rig_controller_prop_data_t *prop_data);

rig_path_t *rig_controller_get_path_for_property(rig_controller_t *controller,
                                                 rig_property_t *property);

rig_path_t *rig_controller_get_path(rig_controller_t *controller,
                                    rut_object_t *object,
                                    const char *property_name);

rig_binding_t *
rig_controller_get_binding_for_prop_data(rig_controller_t *controller,
                                         rig_controller_prop_data_t *prop_data);

rig_controller_t *rig_controller_new(rig_engine_t *engine, const char *name);

void rig_controller_set_label(rut_object_t *object, const char *label);

const char *rig_controller_get_label(rut_object_t *object);

void rig_controller_set_active(rut_object_t *controller, bool active);

bool rig_controller_get_active(rut_object_t *controller);

/* Note: The suspended state overrides the active state and is
 * intended to be used by the editor as a way of disabling controllers
 * when in edit-mode but without inadvertently triggering any bindings
 * which could happen by directly touching the active property
 */
void rig_controller_set_suspended(rut_object_t *object, bool suspended);

bool rig_controller_get_suspended(rut_object_t *object);

void rig_controller_set_auto_deactivate(rut_object_t *controller,
                                        bool auto_deactivate);

bool rig_controller_get_auto_deactivate(rut_object_t *controller);

void rig_controller_set_loop(rut_object_t *controller, bool loop);

bool rig_controller_get_loop(rut_object_t *controller);

void rig_controller_set_running(rut_object_t *controller, bool running);

bool rig_controller_get_running(rut_object_t *controller);

void rig_controller_set_length(rut_object_t *controller, float length);

float rig_controller_get_length(rut_object_t *controller);

void rig_controller_set_elapsed(rut_object_t *controller, double elapsed);

double rig_controller_get_elapsed(rut_object_t *controller);

void rig_controller_set_progress(rut_object_t *controller, double progress);

double rig_controller_get_progress(rut_object_t *controller);

void rig_controller_add_property(rig_controller_t *controller,
                                 rig_property_t *property);

void rig_controller_remove_property(rig_controller_t *controller,
                                    rig_property_t *property);

void rig_controller_set_property_constant(rig_controller_t *controller,
                                          rig_property_t *property,
                                          rut_boxed_t *boxed_value);

void rig_controller_set_property_path(rig_controller_t *controller,
                                      rig_property_t *property,
                                      rig_path_t *path);

typedef void (*rig_controller_property_iter_func_t)(
    rig_controller_prop_data_t *prop_data, void *user_data);

void rig_controller_foreach_property(rig_controller_t *controller,
                                     rig_controller_property_iter_func_t callback,
                                     void *user_data);

rut_closure_t *rig_controller_add_operation_callback(
    rig_controller_t *controller,
    rig_controller_operation_callback_t callback,
    void *user_data,
    rut_closure_destroy_callback_t destroy_cb);

void rig_controller_set_property_method(rig_controller_t *controller,
                                        rig_property_t *property,
                                        rig_controller_method_t method);
void rig_controller_set_property_binding(rig_controller_t *controller,
                                         rig_property_t *property,
                                         rig_binding_t *binding);

typedef void (*rig_controller_node_callback_t)(rig_node_t *node,
                                               void *user_data);

void rig_controller_foreach_node(rig_controller_t *controller,
                                 rig_controller_node_callback_t callback,
                                 void *user_data);

void rig_controller_insert_path_value(rig_controller_t *controller,
                                      rig_property_t *property,
                                      float t,
                                      const rut_boxed_t *value);

void rig_controller_box_path_value(rig_controller_t *controller,
                                   rig_property_t *property,
                                   float t,
                                   rut_boxed_t *out);

void rig_controller_remove_path_value(rig_controller_t *controller,
                                      rig_property_t *property,
                                      float t);

void rig_controller_reap(rig_controller_t *controller, rig_engine_t *engine);

#endif /* _RUT_CONTROLLER_H_ */
