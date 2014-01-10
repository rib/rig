/*
 * Rut
 *
 * Copyright (C) 2012 Intel Corporation.
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

#include <cogl/cogl.h>
#include <string.h>
#include <math.h>

#include "rut-inspector.h"
#include "rut-prop-inspector.h"
#include "rut-vec3-slider.h"
#include "rut-number-slider.h"
#include "rut-drop-down.h"
#include "rut-stack.h"
#include "rut-drag-bin.h"
#include "rut-paintable.h"
#include "rut-box-layout.h"
#include "rut-composite-sizable.h"
#include "rut-bin.h"
#include "rut-introspectable.h"

#define RUT_INSPECTOR_EDGE_GAP 5
#define RUT_INSPECTOR_PROPERTY_GAP 5

typedef struct
{
  RutStack *stack;
  RutObject *control;
  RutDragBin *drag_bin;
  RutProperty *source_prop;
  RutProperty *target_prop;

  /* A pointer is stored back to the inspector so that we can use a
   * pointer to this data directly as the callback data for the
   * property binding */
  RutInspector *inspector;
} RutInspectorPropertyData;

struct _RutInspector
{
  RutObjectBase _base;

  RutContext *context;
  GList *objects;

  RutPaintableProps paintable;
  RutGraphableProps graphable;

  RutBoxLayout *vbox;

  int n_props;
  RutInspectorPropertyData *prop_data;

  RutInspectorCallback property_changed_cb;
  RutInspectorControlledCallback controlled_changed_cb;
  void *user_data;

};

RutType rut_inspector_type;

static void
_rut_inspector_free (void *object)
{
  RutInspector *inspector = object;

  g_list_foreach (inspector->objects, (GFunc)rut_object_unref, NULL);
  g_list_free (inspector->objects);
  inspector->objects = NULL;

  g_free (inspector->prop_data);

  rut_graphable_destroy (inspector);

  rut_object_free (RutInspector, inspector);
}

static void
_rut_inspector_init_type (void)
{
  static RutGraphableVTable graphable_vtable = {
      NULL, /* child removed */
      NULL, /* child addded */
      NULL /* parent changed */
  };
  static RutSizableVTable sizable_vtable = {
      rut_composite_sizable_set_size,
      rut_composite_sizable_get_size,
      rut_composite_sizable_get_preferred_width,
      rut_composite_sizable_get_preferred_height,
      rut_composite_sizable_add_preferred_size_callback
  };

  RutType *type = &rut_inspector_type;
#define TYPE RutInspector

  rut_type_init (type, G_STRINGIFY (TYPE), _rut_inspector_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_GRAPHABLE,
                      offsetof (TYPE, graphable),
                      &graphable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_SIZABLE,
                      0, /* no implied properties */
                      &sizable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_COMPOSITE_SIZABLE,
                      offsetof (TYPE, vbox),
                      NULL); /* no vtable */

#undef TYPE
}

static void
property_changed_cb (RutProperty *primary_target_prop,
                     RutProperty *source_prop,
                     void *user_data)
{
  RutInspectorPropertyData *prop_data = user_data;
  RutInspector *inspector = prop_data->inspector;
  GList *l;
  bool mergable;

  g_return_if_fail (primary_target_prop == prop_data->target_prop);

  switch (source_prop->spec->type)
    {
    case RUT_PROPERTY_TYPE_FLOAT:
    case RUT_PROPERTY_TYPE_DOUBLE:
    case RUT_PROPERTY_TYPE_INTEGER:
    case RUT_PROPERTY_TYPE_UINT32:
    case RUT_PROPERTY_TYPE_VEC3:
    case RUT_PROPERTY_TYPE_VEC4:
    case RUT_PROPERTY_TYPE_QUATERNION:
      mergable = true;
      break;
    default:
      mergable = false;
    }

  /* Forward the property change to the corresponding property
   * of all objects being inspected... */
  for (l = inspector->objects; l; l = l->next)
    {
      RutProperty *target_prop =
        rut_introspectable_lookup_property (l->data,
                                            primary_target_prop->spec->name);
      inspector->property_changed_cb (target_prop, /* target */
                                      source_prop,
                                      mergable,
                                      inspector->user_data);
    }
}

static void
controlled_changed_cb (RutProperty *primary_property,
                       CoglBool value,
                       void *user_data)
{
  RutInspectorPropertyData *prop_data = user_data;
  RutInspector *inspector = prop_data->inspector;
  GList *l;

  g_return_if_fail (primary_property == prop_data->target_prop);

  /* Forward the controlled state change to the corresponding property
   * of all objects being inspected... */
  for (l = inspector->objects; l; l = l->next)
    {
      RutProperty *property =
        rut_introspectable_lookup_property (l->data,
                                            primary_property->spec->name);

      inspector->controlled_changed_cb (property,
                                        value,
                                        inspector->user_data);
    }
}

static void
get_all_properties_cb (RutProperty *prop,
                       void *user_data)
{
  GArray *array = user_data;
  RutInspectorPropertyData *prop_data;

  g_array_set_size (array, array->len + 1);
  prop_data = &g_array_index (array,
                              RutInspectorPropertyData,
                              array->len - 1);
  prop_data->target_prop = prop;
}

static void
create_property_controls (RutInspector *inspector)
{
  RutObject *reference_object = inspector->objects->data;
  GArray *props;
  int i;

  props = g_array_new (FALSE, /* not zero terminated */
                       FALSE, /* don't clear */
                       sizeof (RutInspectorPropertyData));

  if (rut_object_is (reference_object, RUT_TRAIT_ID_INTROSPECTABLE))
    rut_introspectable_foreach_property (reference_object,
                                         get_all_properties_cb,
                                         props);

  inspector->n_props = props->len;

  inspector->prop_data = ((RutInspectorPropertyData *)
                          g_array_free (props, FALSE));

  for (i = 0; i < inspector->n_props; i++)
    {
      RutInspectorPropertyData *prop_data = inspector->prop_data + i;
      RutObject *control;
      RutBin *bin;

      prop_data->inspector = inspector;

      prop_data->stack = rut_stack_new (inspector->context, 1, 1);
      rut_box_layout_add (inspector->vbox, FALSE, prop_data->stack);
      rut_object_unref (prop_data->stack);

      prop_data->drag_bin = rut_drag_bin_new (inspector->context);
      rut_graphable_add_child (prop_data->stack, prop_data->drag_bin);
      rut_object_unref (prop_data->drag_bin);

      bin = rut_bin_new (inspector->context);
      rut_bin_set_bottom_padding (bin, 5);
      rut_drag_bin_set_child (prop_data->drag_bin, bin);
      rut_object_unref (bin);

      control = rut_prop_inspector_new (inspector->context,
                                        prop_data->target_prop,
                                        property_changed_cb,
                                        controlled_changed_cb,
                                        true,
                                        prop_data);
      rut_bin_set_child (bin, control);
      rut_object_unref (control);

      /* XXX: It could be better if the payload could represent the selection
       * of multiple properties when an inspector is inspecting multiple
       * selected objects... */
      rut_drag_bin_set_payload (prop_data->drag_bin, control);

      prop_data->control = control;
    }
}

RutInspector *
rut_inspector_new (RutContext *context,
                   GList *objects,
                   RutInspectorCallback user_property_changed_cb,
                   RutInspectorControlledCallback user_controlled_changed_cb,
                   void *user_data)
{
  RutInspector *inspector = rut_object_alloc0 (RutInspector,
                                               &rut_inspector_type,
                                               _rut_inspector_init_type);

  inspector->context = context;
  inspector->objects = g_list_copy (objects);

  g_list_foreach (objects, (GFunc)rut_object_ref, NULL);

  inspector->property_changed_cb = user_property_changed_cb;
  inspector->controlled_changed_cb = user_controlled_changed_cb;
  inspector->user_data = user_data;

  rut_graphable_init (inspector);

  inspector->vbox = rut_box_layout_new (context,
                                        RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM);
  rut_graphable_add_child (inspector, inspector->vbox);
  rut_object_unref (inspector->vbox);

  create_property_controls (inspector);

  rut_sizable_set_size (inspector, 10, 10);

  return inspector;
}

void
rut_inspector_reload_property (RutInspector *inspector,
                               RutProperty *property)
{
  int i;

  for (i = 0; i < inspector->n_props; i++)
    {
      RutInspectorPropertyData *prop_data = inspector->prop_data + i;

      if (prop_data->target_prop == property)
        rut_prop_inspector_reload_property (prop_data->control);
    }
}

void
rut_inspector_set_property_controlled (RutInspector *inspector,
                                       RutProperty *property,
                                       CoglBool controlled)
{
  int i;

  for (i = 0; i < inspector->n_props; i++)
    {
      RutInspectorPropertyData *prop_data = inspector->prop_data + i;

      if (prop_data->target_prop == property)
        rut_prop_inspector_set_controlled (prop_data->control, controlled);
    }
}
