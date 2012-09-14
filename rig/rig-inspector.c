/*
 * Rig
 *
 * A tiny toolkit
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

#include <cogl/cogl.h>
#include <string.h>
#include <math.h>

#include "rig.h"
#include "rig-inspector.h"
#include "rig-prop-inspector.h"
#include "rig-vec3-slider.h"
#include "rig-number-slider.h"
#include "rig-drop-down.h"

#define RIG_INSPECTOR_EDGE_GAP 5
#define RIG_INSPECTOR_PROPERTY_GAP 5

#define RIG_INSPECTOR_N_COLUMNS 1

typedef struct
{
  RigObject *control;
  RigTransform *transform;
  RigProperty *source_prop;
  RigProperty *target_prop;

  /* A pointer is stored back to the inspector so that we can use a
   * pointer to this data directly as the callback data for the
   * property binding */
  RigInspector *inspector;
} RigInspectorPropertyData;

struct _RigInspector
{
  RigObjectProps _parent;

  RigContext *context;
  RigObject *object;

  RigPaintableProps paintable;
  RigGraphableProps graphable;

  int n_props;
  int n_rows;
  RigInspectorPropertyData *prop_data;

  int width;
  int height;

  RigInspectorCallback property_changed_cb;
  void *user_data;

  int ref_count;
};

RigType rig_inspector_type;

static void
_rig_inspector_free (void *object)
{
  RigInspector *inspector = object;
  int i;

  rig_ref_countable_unref (inspector->context);
  rig_ref_countable_unref (inspector->object);

  for (i = 0; i < inspector->n_props; i++)
    {
      RigInspectorPropertyData *prop_data = inspector->prop_data + i;

      rig_graphable_remove_child (prop_data->control);
      rig_graphable_remove_child (prop_data->transform);
      rig_ref_countable_unref (prop_data->control);
      rig_ref_countable_unref (prop_data->transform);
    }

  g_free (inspector->prop_data);

  g_slice_free (RigInspector, inspector);
}

RigRefCountableVTable _rig_inspector_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_inspector_free
};

static RigGraphableVTable _rig_inspector_graphable_vtable = {
  NULL, /* child removed */
  NULL, /* child addded */
  NULL /* parent changed */
};

static void
_rig_inspector_paint (RigObject *object,
                      RigPaintContext *paint_ctx)
{
  /* NOP */
}

RigPaintableVTable _rig_inspector_paintable_vtable = {
  _rig_inspector_paint
};

static void
rig_inspector_set_size (void *object,
                        float width_float,
                        float height_float)
{
  int width = width_float;
  int height = height_float;
  RigInspector *inspector = RIG_INSPECTOR (object);
  float total_width = width - RIG_INSPECTOR_EDGE_GAP * 2;
  float slider_width =
    (total_width - ((RIG_INSPECTOR_N_COLUMNS - 1.0f) *
                    RIG_INSPECTOR_PROPERTY_GAP)) /
    RIG_INSPECTOR_N_COLUMNS;
  float pixel_slider_width = floorf (slider_width);
  float y_pos = RIG_INSPECTOR_EDGE_GAP;
  float row_height = 0.0f;
  int i;

  inspector->width = width;
  inspector->height = height;

  for (i = 0; i < inspector->n_props; i++)
    {
      RigInspectorPropertyData *prop_data = inspector->prop_data + i;
      float preferred_height;

      rig_transform_init_identity (prop_data->transform);
      rig_transform_translate (prop_data->transform,
                               RIG_INSPECTOR_EDGE_GAP +
                               nearbyintf ((slider_width +
                                            RIG_INSPECTOR_PROPERTY_GAP) *
                                           (i % RIG_INSPECTOR_N_COLUMNS)),
                               y_pos,
                               0.0f);

      rig_sizable_get_preferred_height (prop_data->control,
                                        pixel_slider_width,
                                        NULL,
                                        &preferred_height);

      rig_sizable_set_size (prop_data->control,
                            pixel_slider_width,
                            preferred_height);

      if (preferred_height > row_height)
        row_height = preferred_height;

      if ((i + 1) % RIG_INSPECTOR_N_COLUMNS == 0)
        {
          y_pos += row_height + RIG_INSPECTOR_PROPERTY_GAP;
          row_height = 0.0f;
        }
    }
}

static void
rig_inspector_get_preferred_width (void *object,
                                   float for_height,
                                   float *min_width_p,
                                   float *natural_width_p)
{
  RigInspector *inspector = RIG_INSPECTOR (object);
  float max_natural_width = 0.0f;
  float max_min_width = 0.0f;
  int i;

  /* Convert the for_height into a height for each row */
  if (for_height >= 0)
    for_height = ((for_height -
                   RIG_INSPECTOR_EDGE_GAP * 2 -
                   (inspector->n_rows - 1) *
                   RIG_INSPECTOR_PROPERTY_GAP) /
                  inspector->n_rows);

  for (i = 0; i < inspector->n_props; i++)
    {
      RigInspectorPropertyData *prop_data = inspector->prop_data + i;
      float min_width;
      float natural_width;

      rig_sizable_get_preferred_width (prop_data->control,
                                       for_height,
                                       &min_width,
                                       &natural_width);

      if (min_width > max_min_width)
        max_min_width = min_width;
      if (natural_width > max_natural_width)
        max_natural_width = natural_width;
    }

  if (min_width_p)
    *min_width_p = (max_min_width * RIG_INSPECTOR_N_COLUMNS +
                    (RIG_INSPECTOR_N_COLUMNS - 1) *
                    RIG_INSPECTOR_PROPERTY_GAP +
                    RIG_INSPECTOR_EDGE_GAP * 2);
  if (natural_width_p)
    *natural_width_p = (max_natural_width * RIG_INSPECTOR_N_COLUMNS +
                        (RIG_INSPECTOR_N_COLUMNS - 1) *
                        RIG_INSPECTOR_PROPERTY_GAP +
                        RIG_INSPECTOR_EDGE_GAP * 2);
}

static void
rig_inspector_get_preferred_height (void *object,
                                    float for_width,
                                    float *min_height_p,
                                    float *natural_height_p)
{
  RigInspector *inspector = RIG_INSPECTOR (object);
  float total_height = 0.0f;
  float row_height = 0.0f;
  int i;

  /* Convert the for_width to the width that each slider will actually
   * get */
  if (for_width >= 0.0f)
    {
      float total_width = for_width - RIG_INSPECTOR_EDGE_GAP * 2;
      float slider_width =
        (total_width - ((RIG_INSPECTOR_N_COLUMNS - 1.0f) *
                        RIG_INSPECTOR_PROPERTY_GAP)) /
        RIG_INSPECTOR_N_COLUMNS;

      for_width = floorf (slider_width);
    }

  for (i = 0; i < inspector->n_props; i++)
    {
      RigInspectorPropertyData *prop_data = inspector->prop_data + i;
      float natural_height;

      rig_sizable_get_preferred_height (prop_data->control,
                                        for_width,
                                        NULL, /* min_height */
                                        &natural_height);

      if (natural_height > row_height)
        row_height = natural_height;

      if ((i + 1) % RIG_INSPECTOR_N_COLUMNS == 0 ||
          i == inspector->n_props - 1)
        {
          total_height += row_height;
          row_height = 0.0f;
        }
    }

  total_height += (RIG_INSPECTOR_EDGE_GAP * 2 +
                   RIG_INSPECTOR_PROPERTY_GAP *
                   (inspector->n_rows - 1));

  if (min_height_p)
    *min_height_p = total_height;
  if (natural_height_p)
    *natural_height_p = total_height;
}

static void
rig_inspector_get_size (void *object,
                        float *width,
                        float *height)
{
  RigInspector *inspector = RIG_INSPECTOR (object);

  *width = inspector->width;
  *height = inspector->height;
}

static RigSizableVTable _rig_inspector_sizable_vtable = {
  rig_inspector_set_size,
  rig_inspector_get_size,
  rig_inspector_get_preferred_width,
  rig_inspector_get_preferred_height
};

static void
_rig_inspector_init_type (void)
{
  rig_type_init (&rig_inspector_type);
  rig_type_add_interface (&rig_inspector_type,
                          RIG_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigInspector, ref_count),
                          &_rig_inspector_ref_countable_vtable);
  rig_type_add_interface (&rig_inspector_type,
                          RIG_INTERFACE_ID_PAINTABLE,
                          offsetof (RigInspector, paintable),
                          &_rig_inspector_paintable_vtable);
  rig_type_add_interface (&rig_inspector_type,
                          RIG_INTERFACE_ID_GRAPHABLE,
                          offsetof (RigInspector, graphable),
                          &_rig_inspector_graphable_vtable);
  rig_type_add_interface (&rig_inspector_type,
                          RIG_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &_rig_inspector_sizable_vtable);
}

static void
property_changed_cb (RigProperty *target_prop,
                     RigProperty *source_prop,
                     void *user_data)
{
  RigInspectorPropertyData *prop_data = user_data;
  RigInspector *inspector = prop_data->inspector;

  g_return_if_fail (target_prop == prop_data->target_prop);

  inspector->property_changed_cb (target_prop,
                                  source_prop,
                                  inspector->user_data);
}

static void
get_all_properties_cb (RigProperty *prop,
                       void *user_data)
{
  GArray *array = user_data;
  RigInspectorPropertyData *prop_data;

  g_array_set_size (array, array->len + 1);
  prop_data = &g_array_index (array,
                              RigInspectorPropertyData,
                              array->len - 1);
  prop_data->target_prop = prop;
}

static void
create_property_controls (RigInspector *inspector)
{
  GArray *props;
  int i;

  props = g_array_new (FALSE, /* not zero terminated */
                       FALSE, /* don't clear */
                       sizeof (RigInspectorPropertyData));

  rig_introspectable_foreach_property (inspector->object,
                                       get_all_properties_cb,
                                       props);

  inspector->n_props = props->len;
  inspector->n_rows = ((inspector->n_props + RIG_INSPECTOR_N_COLUMNS - 1) /
                       RIG_INSPECTOR_N_COLUMNS);
  inspector->prop_data = ((RigInspectorPropertyData *)
                          g_array_free (props, FALSE));

  for (i = 0; i < inspector->n_props; i++)
    {
      RigInspectorPropertyData *prop_data = inspector->prop_data + i;
      RigObject *control;

      prop_data->inspector = inspector;

      control = rig_prop_inspector_new (inspector->context,
                                        prop_data->target_prop,
                                        property_changed_cb,
                                        prop_data);

      prop_data->control = control;
      prop_data->transform = rig_transform_new (inspector->context, NULL);
      rig_graphable_add_child (prop_data->transform, control);
      rig_graphable_add_child (inspector, prop_data->transform);
    }
}

RigInspector *
rig_inspector_new (RigContext *context,
                   RigObject *object,
                   RigInspectorCallback user_property_changed_cb,
                   void *user_data)
{
  RigInspector *inspector = g_slice_new0 (RigInspector);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rig_init ();
      _rig_inspector_init_type ();

      initialized = TRUE;
    }

  inspector->ref_count = 1;
  inspector->context = rig_ref_countable_ref (context);
  inspector->object = rig_ref_countable_ref (object);
  inspector->property_changed_cb = user_property_changed_cb;
  inspector->user_data = user_data;

  rig_object_init (&inspector->_parent, &rig_inspector_type);

  rig_paintable_init (RIG_OBJECT (inspector));
  rig_graphable_init (RIG_OBJECT (inspector));

  create_property_controls (inspector);

  rig_inspector_set_size (inspector, 10, 10);

  return inspector;
}

void
rig_inspector_reload_property (RigInspector *inspector,
                               RigProperty *property)
{
  int i;

  for (i = 0; i < inspector->n_props; i++)
    {
      RigInspectorPropertyData *prop_data = inspector->prop_data + i;

      if (prop_data->target_prop == property)
        rig_prop_inspector_reload_property (prop_data->control);
    }
}
