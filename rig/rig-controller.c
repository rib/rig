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
#include "rig-engine.h"

static RutPropertySpec _rig_controller_prop_specs[] = {
  {
    .name = "label",
    .nick = "Label",
    .blurb = "A label for the entity",
    .type = RUT_PROPERTY_TYPE_TEXT,
    .getter.text_type = rig_controller_get_label,
    .setter.text_type = rig_controller_set_label,
    .flags = RUT_PROPERTY_FLAG_READWRITE
  },
  {
    .name = "active",
    .nick = "Active",
    .blurb = "Whether the controller is actively asserting "
      "control over its properties",
    .type = RUT_PROPERTY_TYPE_BOOLEAN,
    .getter.boolean_type = rig_controller_get_active,
    .setter.boolean_type = rig_controller_set_active,
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .animatable = TRUE
  },
  {
    .name = "auto_deactivate",
    .nick = "Auto Deactivate",
    .blurb = "Whether the controller deactivates on reaching a progress of 1.0",
    .type = RUT_PROPERTY_TYPE_BOOLEAN,
    .getter.boolean_type = rig_controller_get_auto_deactivate,
    .setter.boolean_type = rig_controller_set_auto_deactivate,
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .animatable = TRUE
  },
  {
    .name = "loop",
    .nick = "Loop",
    .blurb = "Whether the controller progress loops",
    .type = RUT_PROPERTY_TYPE_BOOLEAN,
    .getter.boolean_type = rig_controller_get_loop,
    .setter.boolean_type = rig_controller_set_loop,
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .animatable = TRUE
  },
  {
    .name = "running",
    .nick = "Running",
    .blurb = "The sequencing position is progressing over time",
    .type = RUT_PROPERTY_TYPE_BOOLEAN,
    .getter.boolean_type = rig_controller_get_running,
    .setter.boolean_type = rig_controller_set_running,
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .animatable = TRUE
  },
  {
    .name = "length",
    .nick = "Length",
    .blurb = "The length over which property changes can be sequenced",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .getter.float_type = rig_controller_get_length,
    .setter.float_type = rig_controller_set_length,
    .flags = RUT_PROPERTY_FLAG_READWRITE,
  },
  {
    .name = "elapsed",
    .nick = "Elapsed",
    .blurb = "The current sequencing position, between 0 and Length",
    .type = RUT_PROPERTY_TYPE_DOUBLE,
    .getter.double_type = rig_controller_get_elapsed,
    .setter.double_type = rig_controller_set_elapsed,
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .animatable = TRUE
  },
  {
    .name = "progress",
    .nick = "Progress",
    .blurb = "The current sequencing position, between 0 and 1",
    .type = RUT_PROPERTY_TYPE_DOUBLE,
    .getter.double_type = rig_controller_get_progress,
    .setter.double_type = rig_controller_set_progress,
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .animatable = TRUE
  },
  { 0 }
};

static void
_rig_controller_free (RutObject *object)
{
  RigController *controller = object;

  rut_closure_list_disconnect_all (&controller->operation_cb_list);

  rut_introspectable_destroy (controller);

  g_hash_table_destroy (controller->properties);

  rut_object_unref (controller->context);

  g_free (controller->label);

  rut_object_unref (controller->timeline);

  rut_object_free (RigController, controller);
}

RutType rig_controller_type;

static void
_rig_controller_type_init (void)
{


  RutType *type = &rig_controller_type;
#define TYPE RigController

  rut_type_init (type, G_STRINGIFY (TYPE), _rig_controller_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_INTROSPECTABLE,
                      offsetof (TYPE, introspectable),
                      NULL); /* no implied vtable */

#undef TYPE
}

static void
free_prop_data_cb (void *user_data)
{
  RigControllerPropData *prop_data = user_data;

  if (prop_data->path)
    rut_object_unref (prop_data->path);

  rut_boxed_destroy (&prop_data->constant_value);

  g_slice_free (RigControllerPropData, prop_data);
}

RigController *
rig_controller_new (RigEngine *engine,
                    const char *label)
{
  RigController *controller = rut_object_alloc0 (RigController,
                                                 &rig_controller_type,
                                                 _rig_controller_type_init);
  RutTimeline *timeline;


  controller->label = g_strdup (label);

  controller->engine = engine;
  controller->context = engine->ctx;
  timeline = rut_timeline_new (engine->ctx, 0);
  rut_timeline_stop (timeline);
  controller->timeline = timeline;

  rut_list_init (&controller->operation_cb_list);

  rut_introspectable_init (controller, _rig_controller_prop_specs, controller->props);

  controller->properties = g_hash_table_new_full (g_direct_hash,
                                                  g_direct_equal,
                                                  NULL, /* key_destroy */
                                                  free_prop_data_cb);

  rut_property_set_copy_binding (&engine->ctx->property_ctx,
                                 &controller->props[RIG_CONTROLLER_PROP_PROGRESS],
                                 rut_introspectable_lookup_property (timeline,
                                                                     "progress"));

  rut_property_set_copy_binding (&engine->ctx->property_ctx,
                                 &controller->props[RIG_CONTROLLER_PROP_ELAPSED],
                                 rut_introspectable_lookup_property (timeline,
                                                                     "elapsed"));

  return controller;
}

void
rig_controller_set_label (RutObject *object, const char *label)
{
  RigController *controller = object;

  if (controller->label &&
      strcmp (controller->label, label) == 0)
    return;

  g_free (controller->label);
  controller->label = g_strdup (label);
  rut_property_dirty (&controller->context->property_ctx,
                      &controller->props[RIG_CONTROLLER_PROP_LABEL]);
}

const char *
rig_controller_get_label (RutObject *object)
{
  RigController *controller = object;

  return controller->label;
}

static void
dummy_binding_cb (RutProperty *property, void *user_data)
{

}

static void
assert_path_value_cb (RutProperty *property, void *user_data)
{
  RigControllerPropData *prop_data = user_data;
  RigController *controller = prop_data->controller;
  RutProperty *progress_prop = &controller->props[RIG_CONTROLLER_PROP_PROGRESS];
  float progress = rut_property_get_double (progress_prop);

  g_return_if_fail (prop_data->method == RIG_CONTROLLER_METHOD_PATH);
  g_return_if_fail (prop_data->path);

  rig_path_lerp_property (prop_data->path,
                          prop_data->property,
                          progress);
}

static void
activate_property_binding (RigControllerPropData *prop_data,
                           void *user_data)
{
  RigController *controller = user_data;

  switch (prop_data->method)
    {
    case RIG_CONTROLLER_METHOD_CONSTANT:
      {
        RutProperty *active_prop;

        /* Even though we are only asserting the property's constant
         * value once on activate, we add a binding for the property
         * so we can block conflicting bindings being set while this
         * controller is active...
         */
        active_prop = &controller->props[RIG_CONTROLLER_PROP_ACTIVE];
        rut_property_set_binding (prop_data->property,
                                  dummy_binding_cb,
                                  prop_data,
                                  active_prop,
                                  NULL); /* sentinal */

        rut_property_set_boxed (&controller->context->property_ctx,
                                prop_data->property,
                                &prop_data->constant_value);
        break;
      }
    case RIG_CONTROLLER_METHOD_PATH:
      {
        RutProperty *progress_prop =
          &controller->props[RIG_CONTROLLER_PROP_PROGRESS];
        rut_property_set_binding (prop_data->property,
                                  assert_path_value_cb,
                                  prop_data,
                                  progress_prop,
                                  NULL); /* sentinal */
        break;
      }
    case RIG_CONTROLLER_METHOD_BINDING:
      /* TODO */
      break;
    }
}

static void
deactivate_property_binding (RigControllerPropData *prop_data,
                             void *user_data)
{
  rut_property_remove_binding (prop_data->property);
}

void
rig_controller_set_active (RutObject *object,
                           bool active)
{
  RigController *controller = object;

  if (controller->active == active)
    return;

  controller->active = active;

  if (active)
    {
      rig_controller_foreach_property (controller,
                                       activate_property_binding,
                                       controller);
    }
  else
    {
      rig_controller_foreach_property (controller,
                                       deactivate_property_binding,
                                       controller);
    }

  rut_property_dirty (&controller->context->property_ctx,
                      &controller->props[RIG_CONTROLLER_PROP_ACTIVE]);
}

bool
rig_controller_get_active (RutObject *object)
{
  RigController *controller = object;

  return controller->active;
}

void
rig_controller_set_auto_deactivate (RutObject *object,
                         bool auto_deactivate)
{
  RigController *controller = object;

  if (controller->auto_deactivate == auto_deactivate)
    return;

  controller->auto_deactivate = auto_deactivate;

  rut_property_dirty (&controller->context->property_ctx,
                      &controller->props[RIG_CONTROLLER_PROP_AUTO_DEACTIVATE]);
}

bool
rig_controller_get_auto_deactivate (RutObject *object)
{
  RigController *controller = object;

  return controller->auto_deactivate;
}

void
rig_controller_set_loop (RutObject *object,
                         bool loop)
{
  RigController *controller = object;

  if (rut_timeline_get_loop_enabled (controller->timeline) == loop)
    return;

  rut_timeline_set_loop_enabled (controller->timeline, loop);

  rut_property_dirty (&controller->context->property_ctx,
                      &controller->props[RIG_CONTROLLER_PROP_LOOP]);
}

bool
rig_controller_get_loop (RutObject *object)
{
  RigController *controller = object;

  return rut_timeline_get_loop_enabled (controller->timeline);
}

void
rig_controller_set_running (RutObject *object,
                            bool running)
{
  RigController *controller = object;

  if (rut_timeline_is_running (controller->timeline) == running)
    return;

  rut_timeline_set_running (controller->timeline, running);

  rut_property_dirty (&controller->context->property_ctx,
                      &controller->props[RIG_CONTROLLER_PROP_RUNNING]);
}

bool
rig_controller_get_running (RutObject *object)
{
  RigController *controller = object;

  return rut_timeline_is_running (controller->timeline);
}

void
rig_controller_set_length (RutObject *object,
                           float length)
{
  RigController *controller = object;

  if (rut_timeline_get_length (controller->timeline) == length)
    return;

  rut_timeline_set_length (controller->timeline, length);

  rut_property_dirty (&controller->context->property_ctx,
                      &controller->props[RIG_CONTROLLER_PROP_LENGTH]);
}

float
rig_controller_get_length (RutObject *object)
{
  RigController *controller = object;

  return rut_timeline_get_length (controller->timeline);
}

void
rig_controller_set_elapsed (RutObject *object,
                            double elapsed)
{
  RigController *controller = object;
  double prev_elapsed;

  if (controller->elapsed == elapsed)
    return;

  prev_elapsed = controller->elapsed;

  rut_timeline_set_elapsed (controller->timeline, elapsed);

  /* NB: the timeline will validate the elapsed value to make sure it
   * isn't out of bounds, considering the length of the timeline.
   */
  controller->elapsed = rut_timeline_get_elapsed (controller->timeline);

  if (controller->elapsed == prev_elapsed)
    return;

  rut_property_dirty (&controller->context->property_ctx,
                      &controller->props[RIG_CONTROLLER_PROP_ELAPSED]);
  rut_property_dirty (&controller->context->property_ctx,
                      &controller->props[RIG_CONTROLLER_PROP_PROGRESS]);
}

double
rig_controller_get_elapsed (RutObject *object)
{
  RigController *controller = object;

  return rut_timeline_get_elapsed (controller->timeline);
}

void
rig_controller_set_progress (RutObject *object,
                             double progress)
{
  RigController *controller = object;

  float length = rig_controller_get_length (controller);
  rig_controller_set_elapsed (object, length * progress);
}

double
rig_controller_get_progress (RutObject *object)
{
  RigController *controller = object;

  return rut_timeline_get_progress (controller->timeline);
}

RigControllerPropData *
rig_controller_find_prop_data_for_property (RigController *controller,
                                            RutProperty *property)
{
  return g_hash_table_lookup (controller->properties, property);
}

RigPath *
rig_controller_find_path (RigController *controller,
                          RutProperty *property)
{
  RigControllerPropData *prop_data;

  prop_data = rig_controller_find_prop_data_for_property (controller, property);

  return prop_data ? prop_data->path : NULL;
}

RigPath *
rig_controller_get_path_for_prop_data (RigController *controller,
                                       RigControllerPropData *prop_data)
{
  if (prop_data->path == NULL)
    {
      RigPath *path = rig_path_new (controller->context,
                                    prop_data->property->spec->type);
      rig_controller_set_property_path (controller,
                                        prop_data->property,
                                        path);
    }

  return prop_data->path;
}

RigPath *
rig_controller_get_path_for_property (RigController *controller,
                                      RutProperty *property)
{
  RigControllerPropData *prop_data =
    rig_controller_find_prop_data_for_property (controller,
                                                property);

  if (!prop_data)
    return NULL;

  return rig_controller_get_path_for_prop_data (controller, prop_data);
}

RigControllerPropData *
rig_controller_find_prop_data (RigController *controller,
                               RutObject *object,
                               const char *property_name)
{
  RutProperty *property =
    rut_introspectable_lookup_property (object, property_name);

  if (!property)
    return NULL;

  return rig_controller_find_prop_data_for_property (controller, property);
}

RigPath *
rig_controller_get_path (RigController *controller,
                         RutObject *object,
                         const char *property_name)
{
  RigControllerPropData *prop_data =
    rig_controller_find_prop_data (controller, object, property_name);

  if (!prop_data)
    return NULL;

  return rig_controller_get_path_for_prop_data (controller, prop_data);
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
rig_controller_add_property (RigController *controller,
                             RutProperty *property)
{
  RigControllerPropData *prop_data =
    rig_controller_find_prop_data_for_property (controller, property);

  if (prop_data)
    return;

  prop_data = g_slice_new0 (RigControllerPropData);
  prop_data->controller = controller;
  prop_data->method = RIG_CONTROLLER_METHOD_CONSTANT;
  prop_data->property = property;

  rut_property_box (property, &prop_data->constant_value);

  g_hash_table_insert (controller->properties,
                       property,
                       prop_data);

  if (controller->active)
    activate_property_binding (prop_data, controller);

  rut_closure_list_invoke (&controller->operation_cb_list,
                           RigControllerOperationCallback,
                           controller,
                           RIG_CONTROLLER_OPERATION_ADDED,
                           prop_data);
}

void
rig_controller_remove_property (RigController *controller,
                                RutProperty *property)
{
  RigControllerPropData *prop_data =
    rig_controller_find_prop_data_for_property (controller, property);

  if (prop_data)
    {
      if (controller->active)
        deactivate_property_binding (prop_data, controller);

      rut_closure_list_invoke (&controller->operation_cb_list,
                               RigControllerOperationCallback,
                               controller,
                               RIG_CONTROLLER_OPERATION_REMOVED,
                               prop_data);

      g_hash_table_remove (controller->properties, property);
    }
}

void
rig_controller_set_property_method (RigController *controller,
                                    RutProperty *property,
                                    RigControllerMethod method)
{
  RigControllerPropData *prop_data =
    rig_controller_find_prop_data_for_property (controller, property);

  g_return_if_fail (prop_data != NULL);

  if (prop_data->method == method)
    return;

  prop_data->method = method;

  if (controller->active)
    {
      deactivate_property_binding (prop_data, controller);
      activate_property_binding (prop_data, controller);
    }

  rut_closure_list_invoke (&controller->operation_cb_list,
                           RigControllerOperationCallback,
                           controller,
                           RIG_CONTROLLER_OPERATION_METHOD_CHANGED,
                           prop_data);
}

void
rig_controller_set_property_constant (RigController *controller,
                                      RutProperty *property,
                                      RutBoxed *boxed_value)
{
  RigControllerPropData *prop_data =
    rig_controller_find_prop_data_for_property (controller, property);

  g_return_if_fail (prop_data != NULL);

  rut_boxed_destroy (&prop_data->constant_value);
  rut_boxed_copy (&prop_data->constant_value, boxed_value);

  if (controller->active &&
      prop_data->method == RIG_CONTROLLER_METHOD_CONSTANT)
    {
      rut_property_set_boxed (&controller->context->property_ctx,
                              prop_data->property, boxed_value);
    }
}

void
rig_controller_set_property_path (RigController *controller,
                                  RutProperty *property,
                                  RigPath *path)
{
  RigControllerPropData *prop_data =
    rig_controller_find_prop_data_for_property (controller, property);

  g_return_if_fail (prop_data != NULL);

  if (prop_data->path)
    {
      rut_closure_disconnect (prop_data->path_change_closure);
      rut_object_unref (prop_data->path);
    }

  prop_data->path = rut_object_ref (path);
#warning "FIXME: what if this changes the length of the controller?"

  if (controller->active &&
      prop_data->method == RIG_CONTROLLER_METHOD_PATH)
    {
      assert_path_value_cb (prop_data->property, prop_data);
    }
}

void
rig_controller_set_property_binding (RigController *controller,
                                     RutProperty *property,
                                     const char *c_expression,
                                     RutProperty **dependencies,
                                     int n_dependencies)
{
  RigControllerPropData *prop_data =
    rig_controller_find_prop_data_for_property (controller, property);

  g_return_if_fail (prop_data != NULL);

  prop_data->dependencies = g_slice_copy (sizeof (void *) * n_dependencies,
                                          dependencies);
  prop_data->n_dependencies = n_dependencies;
  prop_data->c_expression = g_strdup (c_expression);
}

typedef struct _ForeachNodeState
{
  RigControllerNodeCallback callback;
  void *user_data;
} ForeachNodeState;

static void
prop_data_foreach_node (RigControllerPropData *prop_data,
                        void *user_data)
{
  if (prop_data->method == RIG_CONTROLLER_METHOD_PATH)
    {
      ForeachNodeState *state = user_data;
      RigNode *node;

      rut_list_for_each (node, &prop_data->path->nodes, list_node)
        state->callback (node, state->user_data);
    }
}

void
rig_controller_foreach_node (RigController *controller,
                             RigControllerNodeCallback callback,
                             void *user_data)
{
  ForeachNodeState state;

  state.callback = callback;
  state.user_data = user_data;
  rig_controller_foreach_property (controller,
                                   prop_data_foreach_node,
                                   &state);
}

static void
find_max_t_cb (RigNode *node, void *user_data)
{
  float *max_t = user_data;
  if (node->t > *max_t)
    *max_t = node->t;
}

typedef struct _UpdateState
{
  float prev_length;
  float new_length;
} UpdateState;

static void
re_normalize_t_cb (RigNode *node, void *user_data)
{
  UpdateState *state = user_data;
  node->t *= state->prev_length;
  node->t /= state->new_length;
}

static void
zero_t_cb (RigNode *node, void *user_data)
{
  node->t = 0;
}

static void
update_length (RigController *controller, float new_length)
{
  UpdateState state = {
    .prev_length = rig_controller_get_length (controller),
    .new_length = new_length
  };

#warning "fixme: setting a controller's length to 0 destroys any relative positioning of nodes!"
  /* Make sure to avoid divide by zero errors... */
  if (new_length == 0)
    {
      rig_controller_foreach_node (controller, zero_t_cb, &state);
      return;
    }

  rig_controller_foreach_node (controller, re_normalize_t_cb, &state);
  rig_controller_set_length (controller, state.new_length);
}

void
rig_controller_insert_path_value (RigController *controller,
                                  RutProperty *property,
                                  float t,
                                  const RutBoxed *value)
{
  RigControllerPropData *prop_data =
    rig_controller_find_prop_data_for_property (controller, property);
  RigPath *path;
  float length;
  float normalized_t;

  g_return_if_fail (prop_data != NULL);

  path = rig_controller_get_path_for_prop_data (controller, prop_data);

  /* Or should we create a path on demand here? */
  g_return_if_fail (path != NULL);

  length = rig_controller_get_length (controller);

  if (t > length)
    {
      update_length (controller, t);
      length = t;
    }

  normalized_t = length ? t / length : 0;

  rig_path_insert_boxed (path, normalized_t, value);

  if (controller->active &&
      prop_data->method == RIG_CONTROLLER_METHOD_PATH)
    {
      assert_path_value_cb (property, prop_data);
    }
}

void
rig_controller_box_path_value (RigController *controller,
                               RutProperty *property,
                               float t,
                               RutBoxed *out)
{
  RigPath *path =
    rig_controller_get_path_for_property (controller, property);

  float length = rig_controller_get_length (controller);
  float normalized_t;
  RigNode *node;
  bool status;

  g_return_if_fail (path != NULL);

  normalized_t = length ? t / length : 0;

  node = rig_path_find_nearest (path, normalized_t);

  g_return_if_fail (node != NULL);

  status = rig_node_box (path->type, node, out);

  g_return_if_fail (status);
}

void
rig_controller_remove_path_value (RigController *controller,
                                  RutProperty *property,
                                  float t)
{
  RigControllerPropData *prop_data =
    rig_controller_find_prop_data_for_property (controller, property);
  RigPath *path;
  float length;
  float normalized_t;
  RigNode *node;

  g_return_if_fail (prop_data != NULL);

  path = rig_controller_get_path_for_prop_data (controller, prop_data);

  g_return_if_fail (path != NULL);

  length = rig_controller_get_length (controller);

  normalized_t = length ? t / length : 0;

  node = rig_path_find_nearest (path, normalized_t);

  g_return_if_fail (node != NULL);

  normalized_t = node->t;

  rig_path_remove_node (path, node);

  if (t > (length - 1e-3) || t < (length + 1e-3))
    {
      float max_t;

      rig_controller_foreach_node (controller, find_max_t_cb, &max_t);

      if (max_t < normalized_t)
        update_length (controller, max_t * length);
    }

  if (controller->active &&
      prop_data->method == RIG_CONTROLLER_METHOD_PATH)
    {
      assert_path_value_cb (property, prop_data);
    }
}
