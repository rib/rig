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
#include "rig-prop-inspector.h"
#include "rig-vec3-slider.h"
#include "rig-number-slider.h"
#include "rig-drop-down.h"

/* A RigPropInspector represents a control to manipulate a property.
 * It can internally be composed of multiple extra controls for
 * example to toggle whether the property is animatable or not */

#define RIG_PROP_INSPECTOR_MAX_N_CONTROLS 2

typedef struct
{
  RigTransform *transform;
  RigObject *control;

  /* If TRUE, any extra space allocated to the inspector will be
   * equally divided between this control and all other controls that
   * have this set to TRUE */
  CoglBool expand;
} RigPropInspectorControl;

struct _RigPropInspector
{
  RigObjectProps _parent;

  float width, height;

  RigContext *context;

  RigPaintableProps paintable;
  RigGraphableProps graphable;

  RigPropInspectorControl controls[RIG_PROP_INSPECTOR_MAX_N_CONTROLS];
  int n_controls;

  RigProperty *source_prop;
  RigProperty *target_prop;

  /* This dummy property is used so that we can listen to changes on
   * the source property without having to directly make the target
   * property depend on it. The target property can only have one
   * dependent property so we don't want to steal that slot */
  RigProperty dummy_prop;

  RigPropInspectorCallback property_changed_cb;
  void *user_data;

  int ref_count;
};

static RigPropertySpec
dummy_property_spec =
  {
    .name = "dummy",
    .type = RIG_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RigPropInspector, ref_count),
    .setter = abort,
    .getter = abort
  };

RigType rig_prop_inspector_type;

static void
_rig_prop_inspector_free (void *object)
{
  RigPropInspector *inspector = object;
  int i;

  rig_ref_countable_unref (inspector->context);

  for (i = 0; i < inspector->n_controls; i++)
    {
      RigPropInspectorControl *control = inspector->controls + i;

      rig_graphable_remove_child (control->control);
      rig_graphable_remove_child (control->transform);
      rig_ref_countable_unref (control->control);
      rig_ref_countable_unref (control->transform);
    }

  g_slice_free (RigPropInspector, inspector);
}

RigRefCountableVTable _rig_prop_inspector_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_prop_inspector_free
};

static RigGraphableVTable _rig_prop_inspector_graphable_vtable = {
  NULL, /* child removed */
  NULL, /* child addded */
  NULL /* parent changed */
};

static void
_rig_prop_inspector_paint (RigObject *object,
                           RigPaintContext *paint_ctx)
{
  /* NOP */
}

RigPaintableVTable _rig_prop_inspector_paintable_vtable = {
  _rig_prop_inspector_paint
};

static void
rig_prop_inspector_set_size (void *object,
                             float total_width,
                             float total_height)
{
  RigPropInspector *inspector = RIG_PROP_INSPECTOR (object);
  float preferred_widths[RIG_PROP_INSPECTOR_MAX_N_CONTROLS];
  float total_preferred_width = 0.0f;
  float total_expandable_width = 0.0f;
  float x_pos = 0.0f;
  float extra_space;
  int i;

  /* Get all of the preferred widths */
  for (i = 0; i < inspector->n_controls; i++)
    {
      rig_sizable_get_preferred_width (inspector->controls[i].control,
                                       -1, /* for_height */
                                       NULL, /* min_width */
                                       preferred_widths + i);
      total_preferred_width += preferred_widths[i];

      /* Keep track of the total width of expandable controls so that
       * we can work out the fraction of the extra width that each
       * control should receive */
      if (inspector->controls[i].expand)
        total_expandable_width += preferred_widths[i];
    }

  extra_space = total_width - total_preferred_width;

  /* If we've been allocated a space that's too small there's not much
   * we can do so we'll just go off the end */
  if (extra_space < 0.0f)
    extra_space = 0.0f;

  for (i = 0; i < inspector->n_controls; i++)
    {
      RigPropInspectorControl *control = inspector->controls + i;
      float width;
      float height;

      width = preferred_widths[i];

      if (control->expand)
        width += extra_space * width / total_expandable_width;

      rig_sizable_get_preferred_height (control->control,
                                        width,
                                        NULL,
                                        &height);

      rig_transform_init_identity (control->transform);
      rig_transform_translate (control->transform,
                               nearbyintf (x_pos),
                               nearbyintf (total_height / 2.0f -
                                           height / 2.0f),
                               0.0f);

      rig_sizable_set_size (control->control,
                            nearbyintf (width),
                            nearbyintf (height));

      x_pos += width;
    }
}

static void
rig_prop_inspector_get_preferred_width (void *object,
                                        float for_height,
                                        float *min_width_p,
                                        float *natural_width_p)
{
  RigPropInspector *inspector = RIG_PROP_INSPECTOR (object);
  float total_natural_width = 0.0f;
  float total_min_width = 0.0f;
  int i;

  for (i = 0; i < inspector->n_controls; i++)
    {
      RigPropInspectorControl *control = inspector->controls + i;
      float min_width;
      float natural_width;

      rig_sizable_get_preferred_width (control->control,
                                       for_height,
                                       &min_width,
                                       &natural_width);

      total_min_width += min_width;
      total_natural_width += natural_width;
    }

  if (min_width_p)
    *min_width_p = total_min_width;
  if (natural_width_p)
    *natural_width_p = total_natural_width;
}

static void
rig_prop_inspector_get_preferred_height (void *object,
                                         float for_width,
                                         float *min_height_p,
                                         float *natural_height_p)
{
  RigPropInspector *inspector = RIG_PROP_INSPECTOR (object);
  float max_min_height = 0.0f;
  float max_natural_height = 0.0f;
  int i;

  for (i = 0; i < inspector->n_controls; i++)
    {
      RigPropInspectorControl *control = inspector->controls + i;
      float min_height;
      float natural_height;

      rig_sizable_get_preferred_height (control->control,
                                        -1, /* for_width */
                                        &min_height,
                                        &natural_height);

      if (min_height > max_min_height)
        max_min_height = min_height;
      if (natural_height > max_natural_height)
        max_natural_height = natural_height;
    }

  if (min_height_p)
    *min_height_p = max_min_height;
  if (natural_height_p)
    *natural_height_p = max_natural_height;
}

static void
rig_prop_inspector_get_size (void *object,
                             float *width,
                             float *height)
{
  RigPropInspector *inspector = RIG_PROP_INSPECTOR (object);

  *width = inspector->width;
  *height = inspector->height;
}

static RigSizableVTable _rig_prop_inspector_sizable_vtable = {
  rig_prop_inspector_set_size,
  rig_prop_inspector_get_size,
  rig_prop_inspector_get_preferred_width,
  rig_prop_inspector_get_preferred_height
};

static void
_rig_prop_inspector_init_type (void)
{
  rig_type_init (&rig_prop_inspector_type);
  rig_type_add_interface (&rig_prop_inspector_type,
                          RIG_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigPropInspector, ref_count),
                          &_rig_prop_inspector_ref_countable_vtable);
  rig_type_add_interface (&rig_prop_inspector_type,
                          RIG_INTERFACE_ID_PAINTABLE,
                          offsetof (RigPropInspector, paintable),
                          &_rig_prop_inspector_paintable_vtable);
  rig_type_add_interface (&rig_prop_inspector_type,
                          RIG_INTERFACE_ID_GRAPHABLE,
                          offsetof (RigPropInspector, graphable),
                          &_rig_prop_inspector_graphable_vtable);
  rig_type_add_interface (&rig_prop_inspector_type,
                          RIG_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &_rig_prop_inspector_sizable_vtable);
}

static RigObject *
rig_prop_inspector_create_control_for_property (RigContext *context,
                                                RigProperty *prop,
                                                RigProperty **control_prop)
{
  const RigPropertySpec *spec = prop->spec;
  const char *name;
  RigText *label;

  if (spec->nick)
    name = spec->nick;
  else
    name = spec->name;

  switch ((RigPropertyType) spec->type)
    {
    case RIG_PROPERTY_TYPE_BOOLEAN:
      {
        RigToggle *toggle = rig_toggle_new (context, name);

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

        rig_vec3_slider_set_name (slider, name);

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

        rig_number_slider_set_name (slider, name);

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

  rig_text_set_text (label, name);

  *control_prop = NULL;

  return label;
}

static void
property_changed_cb (RigProperty *target_prop,
                     RigProperty *source_prop,
                     void *user_data)
{
  RigPropInspector *inspector = user_data;

  g_return_if_fail (target_prop == &inspector->dummy_prop);
  g_return_if_fail (source_prop == inspector->source_prop);

  inspector->property_changed_cb (inspector->target_prop,
                                  inspector->source_prop,
                                  inspector->user_data);
}

static void
animated_toggle_cb (RigToggle *toggle,
                    CoglBool value,
                    void *user_data)
{
  RigPropInspector *inspector = user_data;

  rig_property_set_animated (&inspector->context->property_ctx,
                             inspector->target_prop,
                             value);
}

static void
add_animatable_toggle (RigPropInspector *inspector,
                       RigProperty *prop)
{
  const RigPropertySpec *spec = prop->spec;

  if (spec->animatable)
    {
      RigObject *control = rig_toggle_new (inspector->context, "");
      RigPropInspectorControl *control_data =
        inspector->controls + inspector->n_controls++;

      rig_toggle_set_tick (control, "•");
      rig_toggle_set_tick_color (control, &(RigColor) { 1, 0, 0, 1 });

      rig_toggle_set_state (control, prop->animated);

      rig_toggle_add_on_toggle_callback (control,
                                         animated_toggle_cb,
                                         inspector,
                                         NULL /* destroy_cb */);

      control_data->control = control;
      control_data->transform = rig_transform_new (inspector->context, NULL);
      rig_graphable_add_child (control_data->transform, control);
      rig_graphable_add_child (inspector, control_data->transform);
    }
}

static void
add_control (RigPropInspector *inspector,
             RigProperty *prop)
{
  RigProperty *control_prop;
  RigObject *control;

  control = rig_prop_inspector_create_control_for_property (inspector->context,
                                                            prop,
                                                            &control_prop);

  if (control)
    {
      RigPropInspectorControl *control_data =
        inspector->controls + inspector->n_controls++;

      control_data->control = control;

      control_data->control = control;

      control_data->transform = rig_transform_new (inspector->context, NULL);
      rig_graphable_add_child (control_data->transform, control);
      rig_graphable_add_child (inspector, control_data->transform);

      if (control_prop)
        {
          rig_property_set_binding (&inspector->dummy_prop,
                                    property_changed_cb,
                                    inspector,
                                    control_prop,
                                    NULL);
          inspector->source_prop = control_prop;
        }
    }
}

RigPropInspector *
rig_prop_inspector_new (RigContext *ctx,
                        RigProperty *property,
                        RigPropInspectorCallback user_property_changed_cb,
                        void *user_data)
{
  RigPropInspector *inspector = g_slice_new0 (RigPropInspector);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rig_init ();
      _rig_prop_inspector_init_type ();

      initialized = TRUE;
    }

  inspector->ref_count = 1;
  inspector->context = rig_ref_countable_ref (ctx);
  inspector->target_prop = property;
  inspector->property_changed_cb = user_property_changed_cb;
  inspector->user_data = user_data;

  rig_object_init (&inspector->_parent, &rig_prop_inspector_type);

  rig_paintable_init (RIG_OBJECT (inspector));
  rig_graphable_init (RIG_OBJECT (inspector));

  inspector->n_controls = 0;

  rig_property_init (&inspector->dummy_prop,
                     &dummy_property_spec,
                     inspector);

  if (property->spec->animatable)
    add_animatable_toggle (inspector, property);

  add_control (inspector, property);

  rig_prop_inspector_set_size (inspector, 10, 10);

  return inspector;
}
