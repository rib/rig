/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012 Intel Corporation.
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
 *
 */

#include <config.h>

#include <cogl/cogl.h>
#include <string.h>
#include <math.h>

#include <rut.h>

#include "rig-prop-inspector.h"
#include "rig-asset-inspector.h"

typedef enum _DisabledState {
  DISABLED_STATE_NONE,
  DISABLED_STATE_FULLY,
  DISABLED_STATE_WIDGET,
} DisabledState;

struct _RigPropInspector
{
  RutObjectBase _base;

  float width, height;

  RutContext *context;

  RutGraphableProps graphable;

  RutStack *top_stack;
  RutBoxLayout *top_hbox;

  RutStack *widget_stack;
  RutBoxLayout *widget_hbox;
  RutProperty *widget_prop; /* the inspector's widget property */
  RutProperty *target_prop; /* property being inspected */

  RutIconToggle *controlled_toggle;

  DisabledState disabled_state;
  RutRectangle *disabled_overlay;
  RutInputRegion *input_region;

  RutPropertyClosure *inspector_prop_closure;
  RigPropInspectorCallback inspector_property_changed_cb;
  RigPropInspectorControlledCallback controlled_changed_cb;
  void *user_data;

  RutPropertyClosure *target_prop_closure;

  /* This is set while the property is being reloaded. This will make
   * it avoid forwarding on property changes that were just caused by
   * reading the already current value. */
  bool reloading_property;

};

RutType rig_prop_inspector_type;

static void
_rig_prop_inspector_free (void *object)
{
  RigPropInspector *inspector = object;

  if (inspector->inspector_prop_closure)
    rut_property_closure_destroy (inspector->inspector_prop_closure);
  if (inspector->target_prop_closure)
    rut_property_closure_destroy (inspector->target_prop_closure);

  rut_graphable_destroy (inspector);

  rut_object_unref (inspector->disabled_overlay);
  rut_object_unref (inspector->input_region);

  rut_object_free (RigPropInspector, inspector);
}

static void
_rig_prop_inspector_init_type (void)
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


  RutType *type = &rig_prop_inspector_type;
#define TYPE RigPropInspector

  rut_type_init (type, G_STRINGIFY (TYPE), _rig_prop_inspector_free);
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
                      offsetof (TYPE, top_stack),
                      NULL); /* no vtable */

#undef TYPE
}

static void
set_disabled (RigPropInspector *inspector, DisabledState state)
{
  if (inspector->disabled_state == state)
    return;

  if (inspector->disabled_state == DISABLED_STATE_FULLY)
    {
      rut_graphable_remove_child (inspector->input_region);
      rut_graphable_remove_child (inspector->disabled_overlay);
    }
  else if (inspector->disabled_state == DISABLED_STATE_WIDGET)
    {
      rut_graphable_remove_child (inspector->input_region);
      rut_graphable_remove_child (inspector->disabled_overlay);
    }

  if (state == DISABLED_STATE_FULLY)
    {
      rut_stack_add (inspector->top_stack, inspector->input_region);
      rut_stack_add (inspector->top_stack, inspector->disabled_overlay);
    }
  else if (state == DISABLED_STATE_WIDGET)
    {
      rut_stack_add (inspector->widget_stack, inspector->input_region);
      rut_stack_add (inspector->widget_stack, inspector->disabled_overlay);
    }
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
        RigAssetInspector *asset_inspector =
          rig_asset_inspector_new (context, spec->validation.asset.type);

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
inspector_property_changed_cb (RutProperty *inspector_prop,
                               void *user_data)
{
  RigPropInspector *inspector = user_data;

  /* If the property change was only triggered because we are
   * rereading the existing value then we won't bother notifying
   * anyone */
  if (inspector->reloading_property)
    return;

  inspector->inspector_property_changed_cb (inspector->target_prop,
                                            inspector->widget_prop,
                                            inspector->user_data);
}

static void
controlled_toggle_cb (RutIconToggle *toggle,
                      bool value,
                      void *user_data)
{
  RigPropInspector *inspector = user_data;

  /* If the change was only triggered because we are rereading the
   * existing value then we won't bother updating the state */
  if (inspector->reloading_property)
    return;

  inspector->controlled_changed_cb (inspector->target_prop,
                                    value,
                                    inspector->user_data);
}

static void
add_controlled_toggle (RigPropInspector *inspector,
                       RutProperty *prop)
{
  RutBin *bin;
  RutIconToggle *toggle;

  bin = rut_bin_new (inspector->context);
  rut_bin_set_right_padding (bin, 5);
  rut_box_layout_add (inspector->top_hbox, false, bin);
  rut_object_unref (bin);

  toggle = rut_icon_toggle_new (inspector->context,
                                "record-button-selected.png",
                                "record-button.png");

  rut_icon_toggle_set_state (toggle, false);

  rut_icon_toggle_add_on_toggle_callback (toggle,
                                          controlled_toggle_cb,
                                          inspector,
                                          NULL /* destroy_cb */);

  rut_bin_set_child (bin, toggle);
  rut_object_unref (toggle);

  inspector->controlled_toggle = toggle;
}

static void
add_control (RigPropInspector *inspector,
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
      rut_box_layout_add (inspector->widget_hbox, false, label);
      rut_object_unref (label);
    }

  if (!(inspector->target_prop->spec->flags & RUT_PROPERTY_FLAG_WRITABLE))
    set_disabled (inspector, DISABLED_STATE_WIDGET);

  rut_box_layout_add (inspector->widget_hbox, true, widget);
  rut_object_unref (widget);

  if (widget_prop)
    {
      inspector->inspector_prop_closure =
        rut_property_connect_callback (widget_prop,
                                       inspector_property_changed_cb,
                                       inspector);
      inspector->widget_prop = widget_prop;
    }
}

static void
target_property_changed_cb (RutProperty *target_prop,
                            void *user_data)
{
  RigPropInspector *inspector = user_data;

  /* XXX: We temporarily stop listening for changes to the
   * target_property to ignore any intermediate changes might be made
   * while re-loading the property...
   */

  rut_property_closure_destroy (inspector->target_prop_closure);
  inspector->target_prop_closure = NULL;

  rig_prop_inspector_reload_property (inspector);

  inspector->target_prop_closure =
    rut_property_connect_callback (inspector->target_prop,
                                   target_property_changed_cb,
                                   inspector);
}

static RutInputEventStatus
block_input_cb (RutInputRegion *region,
                RutInputEvent *event,
                void *user_data)
{
  return RUT_INPUT_EVENT_STATUS_HANDLED;
}

RigPropInspector *
rig_prop_inspector_new (RutContext *ctx,
                        RutProperty *property,
                        RigPropInspectorCallback inspector_property_changed_cb,
                        RigPropInspectorControlledCallback inspector_controlled_cb,
                        bool with_label,
                        void *user_data)
{
  RigPropInspector *inspector =
    rut_object_alloc0 (RigPropInspector,
                       &rig_prop_inspector_type,
                       _rig_prop_inspector_init_type);
  RutBin *grab_padding;

  inspector->context = ctx;

  rut_graphable_init (inspector);

  inspector->target_prop = property;
  inspector->inspector_property_changed_cb = inspector_property_changed_cb;
  inspector->controlled_changed_cb = inspector_controlled_cb;
  inspector->user_data = user_data;

  inspector->top_stack = rut_stack_new (ctx, 1, 1);
  rut_graphable_add_child (inspector, inspector->top_stack);
  rut_object_unref (inspector->top_stack);

  inspector->top_hbox = rut_box_layout_new (ctx, RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);
  rut_stack_add (inspector->top_stack, inspector->top_hbox);
  rut_object_unref (inspector->top_hbox);

  /* XXX: Hack for now, to make sure its possible to drag and drop any
   * property without inadvertanty manipulating the property value...
   */
  grab_padding = rut_bin_new (inspector->context);
  rut_bin_set_right_padding (grab_padding, 15);
  rut_box_layout_add (inspector->top_hbox, false, grab_padding);
  rut_object_unref (grab_padding);

  if (inspector->controlled_changed_cb && property->spec->animatable)
    add_controlled_toggle (inspector, property);

  inspector->widget_stack = rut_stack_new (ctx, 1, 1);
  rut_box_layout_add (inspector->top_hbox, true, inspector->widget_stack);
  rut_object_unref (inspector->widget_stack);

  inspector->widget_hbox =
    rut_box_layout_new (inspector->context, RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);
  rut_stack_add (inspector->widget_stack, inspector->widget_hbox);
  rut_object_unref (inspector->widget_hbox);

  inspector->disabled_overlay =
    rut_rectangle_new4f (ctx, 1, 1, 0.5, 0.5, 0.5, 0.5);
  inspector->input_region =
    rut_input_region_new_rectangle (0, 0, 1, 1, block_input_cb, NULL);

  add_control (inspector, property, with_label);

  rig_prop_inspector_reload_property (inspector);

  rut_sizable_set_size (inspector, 10, 10);

  inspector->target_prop_closure =
    rut_property_connect_callback (property,
                                   target_property_changed_cb,
                                   inspector);

  return inspector;
}

void
rig_prop_inspector_reload_property (RigPropInspector *inspector)
{
  if (inspector->target_prop)
    {
      bool old_reloading = inspector->reloading_property;

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
rig_prop_inspector_set_controlled (RigPropInspector *inspector,
                                   bool controlled)
{
  if (inspector->controlled_toggle)
    {
      bool old_reloading = inspector->reloading_property;

      inspector->reloading_property = TRUE;

      rut_icon_toggle_set_state (inspector->controlled_toggle,
                                 controlled);

      inspector->reloading_property = old_reloading;
    }
}

RutProperty *
rig_prop_inspector_get_property (RigPropInspector *inspector)
{
  return inspector->target_prop;
}
