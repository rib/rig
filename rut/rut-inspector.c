/*
 * Rut
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

#include "rut.h"
#include "rut-inspector.h"
#include "rut-prop-inspector.h"
#include "rut-vec3-slider.h"
#include "rut-number-slider.h"
#include "rut-drop-down.h"

#define RUT_INSPECTOR_EDGE_GAP 5
#define RUT_INSPECTOR_PROPERTY_GAP 5

#define RUT_INSPECTOR_N_COLUMNS 1

typedef struct
{
  RutObject *control;
  RutTransform *transform;
  RutProperty *source_prop;
  RutProperty *target_prop;

  /* A pointer is stored back to the inspector so that we can use a
   * pointer to this data directly as the callback data for the
   * property binding */
  RutInspector *inspector;
} RutInspectorPropertyData;

struct _RutInspector
{
  RutObjectProps _parent;

  RutContext *context;
  RutObject *object;

  RutPaintableProps paintable;
  RutGraphableProps graphable;

  int n_props;
  int n_rows;
  RutInspectorPropertyData *prop_data;

  int width;
  int height;

  RutInspectorCallback property_changed_cb;
  void *user_data;

  int ref_count;
};

RutType rut_inspector_type;

static void
_rut_inspector_free (void *object)
{
  RutInspector *inspector = object;
  int i;

  rut_ref_countable_unref (inspector->context);

  if (rut_object_is (inspector->object, RUT_INTERFACE_ID_REF_COUNTABLE))
    rut_ref_countable_unref (inspector->object);

  for (i = 0; i < inspector->n_props; i++)
    {
      RutInspectorPropertyData *prop_data = inspector->prop_data + i;

      rut_graphable_remove_child (prop_data->control);
      rut_graphable_remove_child (prop_data->transform);
      rut_ref_countable_unref (prop_data->control);
      rut_ref_countable_unref (prop_data->transform);
    }

  g_free (inspector->prop_data);

  g_slice_free (RutInspector, inspector);
}

RutRefCountableVTable _rut_inspector_ref_countable_vtable = {
  rut_ref_countable_simple_ref,
  rut_ref_countable_simple_unref,
  _rut_inspector_free
};

static RutGraphableVTable _rut_inspector_graphable_vtable = {
  NULL, /* child removed */
  NULL, /* child addded */
  NULL /* parent changed */
};

static void
_rut_inspector_paint (RutObject *object,
                      RutPaintContext *paint_ctx)
{
  /* NOP */
}

RutPaintableVTable _rut_inspector_paintable_vtable = {
  _rut_inspector_paint
};

static void
rut_inspector_set_size (void *object,
                        float width_float,
                        float height_float)
{
  int width = width_float;
  int height = height_float;
  RutInspector *inspector = RUT_INSPECTOR (object);
  float total_width = width - RUT_INSPECTOR_EDGE_GAP * 2;
  float slider_width =
    (total_width - ((RUT_INSPECTOR_N_COLUMNS - 1.0f) *
                    RUT_INSPECTOR_PROPERTY_GAP)) /
    RUT_INSPECTOR_N_COLUMNS;
  float pixel_slider_width = floorf (slider_width);
  float y_pos = RUT_INSPECTOR_EDGE_GAP;
  float row_height = 0.0f;
  int i;

  inspector->width = width;
  inspector->height = height;

  for (i = 0; i < inspector->n_props; i++)
    {
      RutInspectorPropertyData *prop_data = inspector->prop_data + i;
      float preferred_height;

      rut_transform_init_identity (prop_data->transform);
      rut_transform_translate (prop_data->transform,
                               RUT_INSPECTOR_EDGE_GAP +
                               nearbyintf ((slider_width +
                                            RUT_INSPECTOR_PROPERTY_GAP) *
                                           (i % RUT_INSPECTOR_N_COLUMNS)),
                               y_pos,
                               0.0f);

      rut_sizable_get_preferred_height (prop_data->control,
                                        pixel_slider_width,
                                        NULL,
                                        &preferred_height);

      rut_sizable_set_size (prop_data->control,
                            pixel_slider_width,
                            preferred_height);

      if (preferred_height > row_height)
        row_height = preferred_height;

      if ((i + 1) % RUT_INSPECTOR_N_COLUMNS == 0)
        {
          y_pos += row_height + RUT_INSPECTOR_PROPERTY_GAP;
          row_height = 0.0f;
        }
    }
}

static void
rut_inspector_get_preferred_width (void *object,
                                   float for_height,
                                   float *min_width_p,
                                   float *natural_width_p)
{
  RutInspector *inspector = RUT_INSPECTOR (object);
  float max_natural_width = 0.0f;
  float max_min_width = 0.0f;
  int i;

  /* Convert the for_height into a height for each row */
  if (for_height >= 0)
    for_height = ((for_height -
                   RUT_INSPECTOR_EDGE_GAP * 2 -
                   (inspector->n_rows - 1) *
                   RUT_INSPECTOR_PROPERTY_GAP) /
                  inspector->n_rows);

  for (i = 0; i < inspector->n_props; i++)
    {
      RutInspectorPropertyData *prop_data = inspector->prop_data + i;
      float min_width;
      float natural_width;

      rut_sizable_get_preferred_width (prop_data->control,
                                       for_height,
                                       &min_width,
                                       &natural_width);

      if (min_width > max_min_width)
        max_min_width = min_width;
      if (natural_width > max_natural_width)
        max_natural_width = natural_width;
    }

  if (min_width_p)
    *min_width_p = (max_min_width * RUT_INSPECTOR_N_COLUMNS +
                    (RUT_INSPECTOR_N_COLUMNS - 1) *
                    RUT_INSPECTOR_PROPERTY_GAP +
                    RUT_INSPECTOR_EDGE_GAP * 2);
  if (natural_width_p)
    *natural_width_p = (max_natural_width * RUT_INSPECTOR_N_COLUMNS +
                        (RUT_INSPECTOR_N_COLUMNS - 1) *
                        RUT_INSPECTOR_PROPERTY_GAP +
                        RUT_INSPECTOR_EDGE_GAP * 2);
}

static void
rut_inspector_get_preferred_height (void *object,
                                    float for_width,
                                    float *min_height_p,
                                    float *natural_height_p)
{
  RutInspector *inspector = RUT_INSPECTOR (object);
  float total_height = 0.0f;
  float row_height = 0.0f;
  int i;

  /* Convert the for_width to the width that each slider will actually
   * get */
  if (for_width >= 0.0f)
    {
      float total_width = for_width - RUT_INSPECTOR_EDGE_GAP * 2;
      float slider_width =
        (total_width - ((RUT_INSPECTOR_N_COLUMNS - 1.0f) *
                        RUT_INSPECTOR_PROPERTY_GAP)) /
        RUT_INSPECTOR_N_COLUMNS;

      for_width = floorf (slider_width);
    }

  for (i = 0; i < inspector->n_props; i++)
    {
      RutInspectorPropertyData *prop_data = inspector->prop_data + i;
      float natural_height;

      rut_sizable_get_preferred_height (prop_data->control,
                                        for_width,
                                        NULL, /* min_height */
                                        &natural_height);

      if (natural_height > row_height)
        row_height = natural_height;

      if ((i + 1) % RUT_INSPECTOR_N_COLUMNS == 0 ||
          i == inspector->n_props - 1)
        {
          total_height += row_height;
          row_height = 0.0f;
        }
    }

  total_height += (RUT_INSPECTOR_EDGE_GAP * 2 +
                   RUT_INSPECTOR_PROPERTY_GAP *
                   (inspector->n_rows - 1));

  if (min_height_p)
    *min_height_p = total_height;
  if (natural_height_p)
    *natural_height_p = total_height;
}

static void
rut_inspector_get_size (void *object,
                        float *width,
                        float *height)
{
  RutInspector *inspector = RUT_INSPECTOR (object);

  *width = inspector->width;
  *height = inspector->height;
}

static RutSizableVTable _rut_inspector_sizable_vtable = {
  rut_inspector_set_size,
  rut_inspector_get_size,
  rut_inspector_get_preferred_width,
  rut_inspector_get_preferred_height
};

static void
_rut_inspector_init_type (void)
{
  rut_type_init (&rut_inspector_type);
  rut_type_add_interface (&rut_inspector_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RutInspector, ref_count),
                          &_rut_inspector_ref_countable_vtable);
  rut_type_add_interface (&rut_inspector_type,
                          RUT_INTERFACE_ID_PAINTABLE,
                          offsetof (RutInspector, paintable),
                          &_rut_inspector_paintable_vtable);
  rut_type_add_interface (&rut_inspector_type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (RutInspector, graphable),
                          &_rut_inspector_graphable_vtable);
  rut_type_add_interface (&rut_inspector_type,
                          RUT_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &_rut_inspector_sizable_vtable);
}

static void
property_changed_cb (RutProperty *target_prop,
                     RutProperty *source_prop,
                     void *user_data)
{
  RutInspectorPropertyData *prop_data = user_data;
  RutInspector *inspector = prop_data->inspector;

  g_return_if_fail (target_prop == prop_data->target_prop);

  inspector->property_changed_cb (target_prop,
                                  source_prop,
                                  inspector->user_data);
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
  GArray *props;
  int i;

  props = g_array_new (FALSE, /* not zero terminated */
                       FALSE, /* don't clear */
                       sizeof (RutInspectorPropertyData));

  if (rut_object_is (inspector->object, RUT_INTERFACE_ID_INTROSPECTABLE))
    rut_introspectable_foreach_property (inspector->object,
                                         get_all_properties_cb,
                                         props);

  inspector->n_props = props->len;
  inspector->n_rows = ((inspector->n_props + RUT_INSPECTOR_N_COLUMNS - 1) /
                       RUT_INSPECTOR_N_COLUMNS);
  inspector->prop_data = ((RutInspectorPropertyData *)
                          g_array_free (props, FALSE));

  for (i = 0; i < inspector->n_props; i++)
    {
      RutInspectorPropertyData *prop_data = inspector->prop_data + i;
      RutObject *control;

      prop_data->inspector = inspector;

      control = rut_prop_inspector_new (inspector->context,
                                        prop_data->target_prop,
                                        property_changed_cb,
                                        prop_data);

      prop_data->control = control;
      prop_data->transform = rut_transform_new (inspector->context, NULL);
      rut_graphable_add_child (prop_data->transform, control);
      rut_graphable_add_child (inspector, prop_data->transform);
    }
}

RutInspector *
rut_inspector_new (RutContext *context,
                   RutObject *object,
                   RutInspectorCallback user_property_changed_cb,
                   void *user_data)
{
  RutInspector *inspector = g_slice_new0 (RutInspector);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rut_init ();
      _rut_inspector_init_type ();

      initialized = TRUE;
    }

  inspector->ref_count = 1;
  inspector->context = rut_ref_countable_ref (context);
  if (rut_object_is (object, RUT_INTERFACE_ID_REF_COUNTABLE))
    inspector->object = rut_ref_countable_ref (object);
  else
    inspector->object = object;
  inspector->property_changed_cb = user_property_changed_cb;
  inspector->user_data = user_data;

  rut_object_init (&inspector->_parent, &rut_inspector_type);

  rut_paintable_init (RUT_OBJECT (inspector));
  rut_graphable_init (RUT_OBJECT (inspector));

  create_property_controls (inspector);

  rut_inspector_set_size (inspector, 10, 10);

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
