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
  CoglPipeline *background_pipeline;

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

  cogl_object_unref (inspector->background_pipeline);

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
  RigInspector *inspector = (RigInspector *) object;
  RigCamera *camera = paint_ctx->camera;
  CoglFramebuffer *fb = rig_camera_get_framebuffer (camera);

  cogl_framebuffer_draw_rectangle (fb,
                                   inspector->background_pipeline,
                                   0.0f, 0.0f, /* x1/y1 */
                                   inspector->width,
                                   inspector->height);
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

static RigObject *
rig_inspector_create_control_for_property (RigContext *context,
                                           RigObject *object,
                                           RigProperty *prop,
                                           RigProperty **control_prop)
{
  const RigPropertySpec *spec = prop->spec;
  RigText *label;

  switch ((RigPropertyType) spec->type)
    {
    case RIG_PROPERTY_TYPE_BOOLEAN:
      {
        RigToggle *toggle = rig_toggle_new (context,
                                            spec->name);

        *control_prop = rig_introspectable_lookup_property (toggle, "state");
        rig_property_copy_value (&context->property_ctx,
                                 *control_prop,
                                 prop);

        return toggle;
      }

    case RIG_PROPERTY_TYPE_VEC3:
      {
        RigVec3Slider *slider = rig_vec3_slider_new (context);
        float min = -G_MAXFLOAT, max = G_MAXFLOAT;

        if (spec->nick)
          rig_vec3_slider_set_name (slider, spec->nick);
        else
          rig_vec3_slider_set_name (slider, spec->name);

        if ((spec->flags & RIG_PROPERTY_FLAG_VALIDATE))
          {
            const RigPropertyValidationVec3 *validation =
              &spec->validation.vec3_range;

            min = validation->min;
            max = validation->max;
          }

        rig_vec3_slider_set_min_value (slider, min);
        rig_vec3_slider_set_max_value (slider, max);

        rig_vec3_slider_set_decimal_places (slider, 2);

        *control_prop = rig_introspectable_lookup_property (slider, "value");
        rig_property_copy_value (&context->property_ctx,
                                 *control_prop,
                                 prop);

        return slider;
      }

    case RIG_PROPERTY_TYPE_FLOAT:
    case RIG_PROPERTY_TYPE_INTEGER:
      {
        RigNumberSlider *slider = rig_number_slider_new (context);
        float min = -G_MAXFLOAT, max = G_MAXFLOAT;

        if (spec->nick)
          rig_number_slider_set_name (slider, spec->nick);
        else
          rig_number_slider_set_name (slider, spec->name);

        if (spec->type == RIG_PROPERTY_TYPE_INTEGER)
          {
            rig_number_slider_set_decimal_places (slider, 0);
            rig_number_slider_set_step (slider, 1.0);

            if ((spec->flags & RIG_PROPERTY_FLAG_VALIDATE))
              {
                const RigPropertyValidationFloat *validation =
                  &spec->validation.float_range;

                min = validation->min;
                max = validation->max;
              }
          }
        else
          {
            rig_number_slider_set_decimal_places (slider, 2);
            rig_number_slider_set_step (slider, 0.1);

            if ((spec->flags & RIG_PROPERTY_FLAG_VALIDATE))
              {
                const RigPropertyValidationInteger *validation =
                  &spec->validation.int_range;

                min = validation->min;
                max = validation->max;
              }
          }

        rig_number_slider_set_min_value (slider, min);
        rig_number_slider_set_max_value (slider, max);

        *control_prop = rig_introspectable_lookup_property (slider, "value");

        if (spec->type == RIG_PROPERTY_TYPE_INTEGER)
          {
            int value = rig_property_get_integer (prop);
            rig_number_slider_set_value (slider, value);
          }
        else
          rig_property_copy_value (&context->property_ctx,
                                   *control_prop,
                                   prop);

        return slider;
      }

    case RIG_PROPERTY_TYPE_ENUM:
      /* If the enum isn't validated then we can't get the value
       * names so we can't make a useful control */
      if ((spec->flags & RIG_PROPERTY_FLAG_VALIDATE))
        {
          RigDropDown *drop = rig_drop_down_new (context);
          int value = rig_property_get_enum (prop);
          int n_values, i;
          const RigUIEnum *ui_enum = spec->validation.ui_enum;
          RigDropDownValue *values;

          for (n_values = 0; ui_enum->values[n_values].nick; n_values++);

          values = g_alloca (sizeof (RigDropDownValue) * n_values);

          for (i = 0; i < n_values; i++)
            {
              values[i].name = (ui_enum->values[i].blurb ?
                                ui_enum->values[i].blurb :
                                ui_enum->values[i].nick);
              values[i].value = ui_enum->values[i].value;
            }

          rig_drop_down_set_values_array (drop, values, n_values);
          rig_drop_down_set_value (drop, value);

          *control_prop = rig_introspectable_lookup_property (drop, "value");

          return drop;
        }
      break;

    default:
      break;
    }

  label = rig_text_new (context);

  if (prop->spec->nick)
    rig_text_set_text (label, prop->spec->nick);
  else
    rig_text_set_text (label, prop->spec->name);

  *control_prop = NULL;

  return label;
}

typedef struct
{
  RigInspector *inspector;
  GArray *props;
} GetPropertyState;

static void
property_changed_cb (RigProperty *target_prop,
                     void *user_data)
{
  RigInspectorPropertyData *prop_data = user_data;
  RigInspector *inspector = prop_data->inspector;

  g_return_if_fail (target_prop == prop_data->target_prop);

  inspector->property_changed_cb (target_prop,
                                  prop_data->source_prop,
                                  inspector->user_data);
}

static void
add_property (RigProperty *prop,
              void *user_data)
{
  GetPropertyState *state = user_data;
  RigInspector *inspector = state->inspector;
  RigProperty *control_prop;
  RigObject *control;

  control = rig_inspector_create_control_for_property (inspector->context,
                                                       inspector->object,
                                                       prop,
                                                       &control_prop);

  if (control)
    {
      RigInspectorPropertyData *prop_data;

      g_array_set_size (state->props, state->props->len + 1);
      prop_data = &g_array_index (state->props,
                                  RigInspectorPropertyData,
                                  state->props->len - 1);

      prop_data->control = control;
      prop_data->source_prop = control_prop;
      prop_data->target_prop = prop;
      prop_data->inspector = inspector;

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
  GetPropertyState state;
  int i;

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

  inspector->background_pipeline = cogl_pipeline_new (context->cogl_context);
  cogl_pipeline_set_color4ub (inspector->background_pipeline,
                              64, 64, 128, 128);

  rig_object_init (&inspector->_parent, &rig_inspector_type);

  rig_paintable_init (RIG_OBJECT (inspector));
  rig_graphable_init (RIG_OBJECT (inspector));

  state.inspector = inspector;
  state.props = g_array_new (FALSE, /* not zero terminated */
                             FALSE, /* don't clear */
                             sizeof (RigInspectorPropertyData));

  rig_introspectable_foreach_property (object,
                                       add_property,
                                       &state);


  inspector->n_props = state.props->len;
  inspector->n_rows = ((inspector->n_props + RIG_INSPECTOR_N_COLUMNS - 1) /
                       RIG_INSPECTOR_N_COLUMNS);
  inspector->prop_data = ((RigInspectorPropertyData *)
                          g_array_free (state.props, FALSE));

  for (i = 0; i < inspector->n_props; i++)
    {
      RigInspectorPropertyData *data = inspector->prop_data + i;

      if (data->source_prop)
        rig_property_set_binding (data->target_prop,
                                  property_changed_cb,
                                  data,
                                  data->source_prop,
                                  NULL);
    }

  rig_inspector_set_size (inspector, 10, 10);

  return inspector;
}
