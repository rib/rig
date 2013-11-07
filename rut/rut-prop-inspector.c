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

#include "rut-prop-inspector.h"
#include "rut-vec3-slider.h"
#include "rut-rotation-inspector.h"
#include "rut-number-slider.h"
#include "rut-drop-down.h"
#include "rut-color-button.h"
#include "rut-interfaces.h"
#include "rut-box-layout.h"
#include "rut-icon-toggle.h"
#include "rut-composite-sizable.h"
#include "rut-text.h"
#include "rut-toggle.h"
#include "rut-entry.h"
#include "rut-asset-inspector.h"
#include "rut-bin.h"

struct _RutPropInspector
{
  RutObjectProps _parent;

  float width, height;

  RutContext *context;

  RutGraphableProps graphable;

  RutBoxLayout *hbox;

  RutProperty *widget_prop; /* the inspector's widget property */
  RutProperty *target_prop; /* property being inspected */

  RutIconToggle *controlled_toggle;

  RutPropInspectorCallback property_changed_cb;
  RutPropInspectorControlledCallback controlled_changed_cb;
  void *user_data;

  /* This is set while the property is being reloaded. This will make
   * it avoid forwarding on property changes that were just caused by
   * reading the already current value. */
  CoglBool reloading_property;

  int ref_count;
};

RutType rut_prop_inspector_type;

static void
_rut_prop_inspector_free (void *object)
{
  RutPropInspector *inspector = object;

  rut_graphable_destroy (inspector);

  g_slice_free (RutPropInspector, inspector);
}

static void
_rut_prop_inspector_init_type (void)
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


  RutType *type = &rut_prop_inspector_type;
#define TYPE RutPropInspector

  rut_type_init (type, G_STRINGIFY (TYPE));
  rut_type_add_refable (type, ref_count, _rut_prop_inspector_free);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (TYPE, graphable),
                          &graphable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &sizable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_COMPOSITE_SIZABLE,
                          offsetof (TYPE, hbox),
                          NULL); /* no vtable */

#undef TYPE
}

static RutObject *
create_widget_for_property (RutContext *context,
                            RutProperty *prop,
                            RutProperty **control_prop,
                            const char **label_text)
{
  const RutPropertySpec *spec = prop->spec;
  const char *name;
  RutText *label;

  *label_text = NULL;

  if (spec->nick)
    name = spec->nick;
  else
    name = spec->name;

  switch ((RutPropertyType) spec->type)
    {
    case RUT_PROPERTY_TYPE_BOOLEAN:
      {
        char *unselected_icon = rut_find_data_file ("toggle-unselected.png");
        char *selected_icon = rut_find_data_file ("toggle-selected.png");
        RutToggle *toggle = rut_toggle_new_with_icons (context,
                                                       unselected_icon,
                                                       selected_icon,
                                                       name);

        *control_prop = rut_introspectable_lookup_property (toggle, "state");
        return toggle;
      }

    case RUT_PROPERTY_TYPE_VEC3:
      {
        RutVec3Slider *slider = rut_vec3_slider_new (context);
        float min = -G_MAXFLOAT, max = G_MAXFLOAT;

        if ((spec->flags & RUT_PROPERTY_FLAG_VALIDATE))
          {
            const RutPropertyValidationVec3 *validation =
              &spec->validation.vec3_range;

            min = validation->min;
            max = validation->max;
          }

        rut_vec3_slider_set_min_value (slider, min);
        rut_vec3_slider_set_max_value (slider, max);

        rut_vec3_slider_set_decimal_places (slider, 2);

        *control_prop = rut_introspectable_lookup_property (slider, "value");

        return slider;
      }

    case RUT_PROPERTY_TYPE_QUATERNION:
      {
        RutRotationInspector *inspector = rut_rotation_inspector_new (context);

        *control_prop = rut_introspectable_lookup_property (inspector, "value");

        return inspector;
      }

    case RUT_PROPERTY_TYPE_DOUBLE:
    case RUT_PROPERTY_TYPE_FLOAT:
    case RUT_PROPERTY_TYPE_INTEGER:
      {
        RutNumberSlider *slider = rut_number_slider_new (context);
        float min = -G_MAXFLOAT, max = G_MAXFLOAT;
        char *label = g_strconcat (name, ": ", NULL);

        rut_number_slider_set_markup_label (slider, label);
        g_free (label);

        if (spec->type == RUT_PROPERTY_TYPE_INTEGER)
          {
            rut_number_slider_set_decimal_places (slider, 0);
            rut_number_slider_set_step (slider, 1.0);

            if ((spec->flags & RUT_PROPERTY_FLAG_VALIDATE))
              {
                const RutPropertyValidationInteger *validation =
                  &spec->validation.int_range;

                min = validation->min;
                max = validation->max;
              }
          }
        else
          {
            rut_number_slider_set_decimal_places (slider, 2);
            rut_number_slider_set_step (slider, 0.1);

            if ((spec->flags & RUT_PROPERTY_FLAG_VALIDATE))
              {
                const RutPropertyValidationFloat *validation =
                  &spec->validation.float_range;

                min = validation->min;
                max = validation->max;
              }
          }

        rut_number_slider_set_min_value (slider, min);
        rut_number_slider_set_max_value (slider, max);

        *control_prop = rut_introspectable_lookup_property (slider, "value");

        return slider;
      }

    case RUT_PROPERTY_TYPE_ENUM:
      /* If the enum isn't validated then we can't get the value
       * names so we can't make a useful control */
      if ((spec->flags & RUT_PROPERTY_FLAG_VALIDATE))
        {
          RutDropDown *drop = rut_drop_down_new (context);
          int n_values, i;
          const RutUIEnum *ui_enum = spec->validation.ui_enum;
          RutDropDownValue *values;

          for (n_values = 0; ui_enum->values[n_values].nick; n_values++);

          values = g_alloca (sizeof (RutDropDownValue) * n_values);

          for (i = 0; i < n_values; i++)
            {
              values[i].name = (ui_enum->values[i].blurb ?
                                ui_enum->values[i].blurb :
                                ui_enum->values[i].nick);
              values[i].value = ui_enum->values[i].value;
            }

          rut_drop_down_set_values_array (drop, values, n_values);

          *control_prop = rut_introspectable_lookup_property (drop, "value");
          *label_text = name;

          return drop;
        }
      break;

    case RUT_PROPERTY_TYPE_TEXT:
      {
        RutEntry *entry = rut_entry_new (context);
        RutText *text = rut_entry_get_text (entry);

        rut_text_set_single_line_mode (text, TRUE);
        *control_prop = rut_introspectable_lookup_property (text, "text");
        *label_text = name;

        return entry;
      }
      break;

    case RUT_PROPERTY_TYPE_COLOR:
      {
        RutColorButton *button = rut_color_button_new (context);

        *control_prop = rut_introspectable_lookup_property (button, "color");
        *label_text = name;

        return button;
      }
      break;

    case RUT_PROPERTY_TYPE_ASSET:
      {
        RutAssetInspector *asset_inspector =
          rut_asset_inspector_new (context, spec->validation.asset.type);

        *control_prop = rut_introspectable_lookup_property (asset_inspector, "asset");
        *label_text = name;

        return asset_inspector;
      }
      break;

    default:
      break;
    }

  label = rut_text_new (context);

  rut_text_set_text (label, name);

  *control_prop = NULL;

  return label;
}

static void
property_changed_cb (RutProperty *target_prop,
                     void *user_data)
{
  RutPropInspector *inspector = user_data;

  /* If the property change was only triggered because we are
   * rereading the existing value then we won't bother notifying
   * anyone */
  if (inspector->reloading_property)
    return;

  inspector->property_changed_cb (inspector->target_prop,
                                  inspector->widget_prop,
                                  inspector->user_data);
}

static void
controlled_toggle_cb (RutIconToggle *toggle,
                      bool value,
                      void *user_data)
{
  RutPropInspector *inspector = user_data;

  /* If the change was only triggered because we are rereading the
   * existing value then we won't bother updating the state */
  if (inspector->reloading_property)
    return;

  inspector->controlled_changed_cb (inspector->target_prop,
                                    value,
                                    inspector->user_data);
}

static void
add_controlled_toggle (RutPropInspector *inspector,
                       RutProperty *prop)
{
  const RutPropertySpec *spec = prop->spec;

  if (spec->animatable)
    {
      RutBin *bin;
      RutIconToggle *toggle;

      bin = rut_bin_new (inspector->context);
      rut_bin_set_right_padding (bin, 5);
      rut_box_layout_add (inspector->hbox, false, bin);
      rut_refable_unref (bin);

      toggle = rut_icon_toggle_new (inspector->context,
                                    "record-button-selected.png",
                                    "record-button.png");

      rut_icon_toggle_set_state (toggle, false);

      rut_icon_toggle_add_on_toggle_callback (toggle,
                                              controlled_toggle_cb,
                                              inspector,
                                              NULL /* destroy_cb */);

      rut_bin_set_child (bin, toggle);
      rut_refable_unref (toggle);

      inspector->controlled_toggle = toggle;
    }
}

static void
add_control (RutPropInspector *inspector,
             RutProperty *prop,
             bool with_label)
{
  RutProperty *widget_prop;
  RutObject *widget;
  const char *label_text;

  widget = create_widget_for_property (inspector->context,
                                       prop,
                                       &widget_prop,
                                       &label_text);

  if (!widget)
    return;

  if (with_label && label_text)
    {
      RutText *label =
        rut_text_new_with_text (inspector->context,
                                NULL, /* font_name */
                                label_text);
      rut_text_set_selectable (label, FALSE);
      rut_box_layout_add (inspector->hbox, false, label);
      rut_refable_unref (label);
    }

  rut_box_layout_add (inspector->hbox, true, widget);
  rut_refable_unref (widget);

  if (widget_prop)
    {
      rut_property_connect_callback (widget_prop,
                                     property_changed_cb,
                                     inspector);
      inspector->widget_prop = widget_prop;
    }
}

RutPropInspector *
rut_prop_inspector_new (RutContext *ctx,
                        RutProperty *property,
                        RutPropInspectorCallback inspector_property_changed_cb,
                        RutPropInspectorControlledCallback inspector_controlled_cb,
                        bool with_label,
                        void *user_data)
{
  RutPropInspector *inspector =
    rut_object_alloc0 (RutPropInspector,
                       &rut_prop_inspector_type,
                       _rut_prop_inspector_init_type);

  inspector->ref_count = 1;
  inspector->context = ctx;

  rut_graphable_init (inspector);

  inspector->target_prop = property;
  inspector->property_changed_cb = inspector_property_changed_cb;
  inspector->controlled_changed_cb = inspector_controlled_cb;
  inspector->user_data = user_data;

  inspector->hbox = rut_box_layout_new (ctx, RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);
  rut_graphable_add_child (inspector, inspector->hbox);
  rut_refable_unref (inspector->hbox);

  if (inspector->controlled_changed_cb && property->spec->animatable)
    add_controlled_toggle (inspector, property);

  add_control (inspector, property, with_label);

  rut_prop_inspector_reload_property (inspector);

  rut_sizable_set_size (inspector, 10, 10);

  return inspector;
}

void
rut_prop_inspector_reload_property (RutPropInspector *inspector)
{
  if (inspector->target_prop)
    {
      CoglBool old_reloading = inspector->reloading_property;

      inspector->reloading_property = TRUE;

      if (inspector->widget_prop)
        {
          if (inspector->target_prop->spec->type !=
              inspector->widget_prop->spec->type)
            {
              rut_property_cast_scalar_value (&inspector->context->property_ctx,
                                              inspector->widget_prop,
                                              inspector->target_prop);
            }
          else
            rut_property_copy_value (&inspector->context->property_ctx,
                                     inspector->widget_prop,
                                     inspector->target_prop);
        }

      inspector->reloading_property = old_reloading;
    }
}

void
rut_prop_inspector_set_controlled (RutPropInspector *inspector,
                                   CoglBool controlled)
{
  if (inspector->controlled_toggle)
    {
      CoglBool old_reloading = inspector->reloading_property;

      inspector->reloading_property = TRUE;

      rut_icon_toggle_set_state (inspector->controlled_toggle,
                                 controlled);

      inspector->reloading_property = old_reloading;
    }
}
