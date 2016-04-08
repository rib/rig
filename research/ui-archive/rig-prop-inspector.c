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

#include <cglib/cglib.h>
#include <string.h>
#include <math.h>

#include <rut.h>

#include "rig-prop-inspector.h"
#include "rig-asset-inspector.h"

typedef enum _disabled_state_t {
    DISABLED_STATE_NONE,
    DISABLED_STATE_FULLY,
    DISABLED_STATE_WIDGET,
} disabled_state_t;

struct _rig_prop_inspector_t {
    rut_object_base_t _base;

    float width, height;

    rut_shell_t *shell;

    rut_graphable_props_t graphable;

    rut_stack_t *top_stack;
    rut_box_layout_t *top_hbox;

    rut_stack_t *widget_stack;
    rut_box_layout_t *widget_hbox;
    rig_property_t *widget_prop; /* the inspector's widget property */
    rig_property_t *target_prop; /* property being inspected */

    rut_icon_toggle_t *controlled_toggle;

    disabled_state_t disabled_state;
    rut_rectangle_t *disabled_overlay;
    rut_input_region_t *input_region;

    rig_property_closure_t *inspector_prop_closure;
    rig_prop_inspector_callback_t inspector_property_changed_cb;
    rig_prop_inspector_controlled_callback_t controlled_changed_cb;
    void *user_data;

    rig_property_closure_t *target_prop_closure;

    /* This is set while the property is being reloaded. This will make
     * it avoid forwarding on property changes that were just caused by
     * reading the already current value. */
    bool reloading_property;
};

rut_type_t rig_prop_inspector_type;

static void
_rig_prop_inspector_free(void *object)
{
    rig_prop_inspector_t *inspector = object;

    if (inspector->inspector_prop_closure)
        rig_property_closure_destroy(inspector->inspector_prop_closure);
    if (inspector->target_prop_closure)
        rig_property_closure_destroy(inspector->target_prop_closure);

    rut_graphable_destroy(inspector);

    rut_object_unref(inspector->disabled_overlay);
    rut_object_unref(inspector->input_region);

    rut_object_free(rig_prop_inspector_t, inspector);
}

static void
_rig_prop_inspector_init_type(void)
{
    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child removed */
                                                       NULL, /* child addded */
                                                       NULL /* parent changed */
    };
    static rut_sizable_vtable_t sizable_vtable = {
        rut_composite_sizable_set_size,
        rut_composite_sizable_get_size,
        rut_composite_sizable_get_preferred_width,
        rut_composite_sizable_get_preferred_height,
        rut_composite_sizable_add_preferred_size_callback
    };

    rut_type_t *type = &rig_prop_inspector_type;
#define TYPE rig_prop_inspector_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_prop_inspector_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_GRAPHABLE,
                       offsetof(TYPE, graphable),
                       &graphable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_SIZABLE,
                       0, /* no implied properties */
                       &sizable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_COMPOSITE_SIZABLE,
                       offsetof(TYPE, top_stack),
                       NULL); /* no vtable */

#undef TYPE
}

static void
set_disabled(rig_prop_inspector_t *inspector,
             disabled_state_t state)
{
    if (inspector->disabled_state == state)
        return;

    if (inspector->disabled_state == DISABLED_STATE_FULLY) {
        rut_graphable_remove_child(inspector->input_region);
        rut_graphable_remove_child(inspector->disabled_overlay);
    } else if (inspector->disabled_state == DISABLED_STATE_WIDGET) {
        rut_graphable_remove_child(inspector->input_region);
        rut_graphable_remove_child(inspector->disabled_overlay);
    }

    if (state == DISABLED_STATE_FULLY) {
        rut_stack_add(inspector->top_stack, inspector->input_region);
        rut_stack_add(inspector->top_stack, inspector->disabled_overlay);
    } else if (state == DISABLED_STATE_WIDGET) {
        rut_stack_add(inspector->widget_stack, inspector->input_region);
        rut_stack_add(inspector->widget_stack, inspector->disabled_overlay);
    }
}

static rut_object_t *
create_widget_for_property(rut_shell_t *shell,
                           rig_property_t *prop,
                           rig_property_t **control_prop,
                           const char **label_text)
{
    const rig_property_spec_t *spec = prop->spec;
    const char *name;
    rut_text_t *label;

    *label_text = NULL;

    if (spec->nick)
        name = spec->nick;
    else
        name = spec->name;

    switch ((rig_property_type_t)spec->type) {
    case RUT_PROPERTY_TYPE_BOOLEAN: {
        char *unselected_icon = rut_find_data_file("toggle-unselected.png");
        char *selected_icon = rut_find_data_file("toggle-selected.png");
        rut_toggle_t *toggle = rut_toggle_new_with_icons(
            shell, unselected_icon, selected_icon, name);

        *control_prop = rut_introspectable_lookup_property(toggle, "state");
        return toggle;
    }

    case RUT_PROPERTY_TYPE_VEC3: {
        rut_vec3_slider_t *slider = rut_vec3_slider_new(shell);
        float min = -G_MAXFLOAT, max = G_MAXFLOAT;

        if ((spec->flags & RUT_PROPERTY_FLAG_VALIDATE)) {
            const rig_property_validation_vec3_t *validation =
                &spec->validation.vec3_range;

            min = validation->min;
            max = validation->max;
        }

        rut_vec3_slider_set_min_value(slider, min);
        rut_vec3_slider_set_max_value(slider, max);

        rut_vec3_slider_set_decimal_places(slider, 2);

        *control_prop = rut_introspectable_lookup_property(slider, "value");

        return slider;
    }

    case RUT_PROPERTY_TYPE_QUATERNION: {
        rut_rotation_inspector_t *inspector =
            rut_rotation_inspector_new(shell);

        *control_prop = rut_introspectable_lookup_property(inspector, "value");

        return inspector;
    }

    case RUT_PROPERTY_TYPE_DOUBLE:
    case RUT_PROPERTY_TYPE_FLOAT:
    case RUT_PROPERTY_TYPE_INTEGER: {
        rut_number_slider_t *slider = rut_number_slider_new(shell);
        float min = -G_MAXFLOAT, max = G_MAXFLOAT;
        char *label = c_strconcat(name, ": ", NULL);

        rut_number_slider_set_markup_label(slider, label);
        c_free(label);

        if (spec->type == RUT_PROPERTY_TYPE_INTEGER) {
            rut_number_slider_set_decimal_places(slider, 0);
            rut_number_slider_set_step(slider, 1.0);

            if ((spec->flags & RUT_PROPERTY_FLAG_VALIDATE)) {
                const rig_property_validation_integer_t *validation =
                    &spec->validation.int_range;

                min = validation->min;
                max = validation->max;
            }
        } else {
            rut_number_slider_set_decimal_places(slider, 2);
            rut_number_slider_set_step(slider, 0.1);

            if ((spec->flags & RUT_PROPERTY_FLAG_VALIDATE)) {
                const rig_property_validation_float_t *validation =
                    &spec->validation.float_range;

                min = validation->min;
                max = validation->max;
            }
        }

        rut_number_slider_set_min_value(slider, min);
        rut_number_slider_set_max_value(slider, max);

        *control_prop = rut_introspectable_lookup_property(slider, "value");

        return slider;
    }

    case RUT_PROPERTY_TYPE_ENUM:
        /* If the enum isn't validated then we can't get the value
         * names so we can't make a useful control */
        if ((spec->flags & RUT_PROPERTY_FLAG_VALIDATE)) {
            rut_drop_down_t *drop = rut_drop_down_new(shell);
            int n_values, i;
            const rut_ui_enum_t *ui_enum = spec->validation.ui_enum;
            rut_drop_down_value_t *values;

            for (n_values = 0; ui_enum->values[n_values].nick; n_values++)
                ;

            values = c_alloca(sizeof(rut_drop_down_value_t) * n_values);

            for (i = 0; i < n_values; i++) {
                values[i].name =
                    (ui_enum->values[i].blurb ? ui_enum->values[i].blurb
                     : ui_enum->values[i].nick);
                values[i].value = ui_enum->values[i].value;
            }

            rut_drop_down_set_values_array(drop, values, n_values);

            *control_prop = rut_introspectable_lookup_property(drop, "value");
            *label_text = name;

            return drop;
        }
        break;

    case RUT_PROPERTY_TYPE_TEXT: {
        rut_entry_t *entry = rut_entry_new(shell);
        rut_text_t *text = rut_entry_get_text(entry);

        rut_text_set_single_line_mode(text, true);
        *control_prop = rut_introspectable_lookup_property(text, "text");
        *label_text = name;

        return entry;
    } break;

    case RUT_PROPERTY_TYPE_COLOR: {
        rut_color_button_t *button = rut_color_button_new(shell);

        *control_prop = rut_introspectable_lookup_property(button, "color");
        *label_text = name;

        return button;
    } break;

    case RUT_PROPERTY_TYPE_ASSET: {
        rig_asset_inspector_t *asset_inspector =
            rig_asset_inspector_new(shell, spec->validation.asset.type);

        *control_prop =
            rut_introspectable_lookup_property(asset_inspector, "asset");
        *label_text = name;

        return asset_inspector;
    } break;

    default:
        break;
    }

    label = rut_text_new(shell);

    rut_text_set_text(label, name);

    *control_prop = NULL;

    return label;
}

static void
inspector_property_changed_cb(rig_property_t *inspector_prop,
                              void *user_data)
{
    rig_prop_inspector_t *inspector = user_data;

    /* If the property change was only triggered because we are
     * rereading the existing value then we won't bother notifying
     * anyone */
    if (inspector->reloading_property)
        return;

    inspector->inspector_property_changed_cb(
        inspector->target_prop, inspector->widget_prop, inspector->user_data);
}

static void
controlled_toggle_cb(rut_icon_toggle_t *toggle, bool value, void *user_data)
{
    rig_prop_inspector_t *inspector = user_data;

    /* If the change was only triggered because we are rereading the
     * existing value then we won't bother updating the state */
    if (inspector->reloading_property)
        return;

    inspector->controlled_changed_cb(
        inspector->target_prop, value, inspector->user_data);
}

static void
add_controlled_toggle(rig_prop_inspector_t *inspector,
                      rig_property_t *prop)
{
    rut_bin_t *bin;
    rut_icon_toggle_t *toggle;

    bin = rut_bin_new(inspector->shell);
    rut_bin_set_right_padding(bin, 5);
    rut_box_layout_add(inspector->top_hbox, false, bin);
    rut_object_unref(bin);

    toggle = rut_icon_toggle_new(
        inspector->shell, "record-button-selected.png", "record-button.png");

    rut_icon_toggle_set_state(toggle, false);

    rut_icon_toggle_add_on_toggle_callback(
        toggle, controlled_toggle_cb, inspector, NULL /* destroy_cb */);

    rut_bin_set_child(bin, toggle);
    rut_object_unref(toggle);

    inspector->controlled_toggle = toggle;
}

static void
add_control(rig_prop_inspector_t *inspector,
            rig_property_t *prop,
            bool with_label)
{
    rig_property_t *widget_prop;
    rut_object_t *widget;
    const char *label_text;

    widget = create_widget_for_property(
        inspector->shell, prop, &widget_prop, &label_text);

    if (!widget)
        return;

    if (with_label && label_text) {
        rut_text_t *label = rut_text_new_with_text(inspector->shell,
                                                   NULL, /* font_name */
                                                   label_text);
        rut_text_set_selectable(label, false);
        rut_box_layout_add(inspector->widget_hbox, false, label);
        rut_object_unref(label);
    }

    if (!(inspector->target_prop->spec->flags & RUT_PROPERTY_FLAG_WRITABLE))
        set_disabled(inspector, DISABLED_STATE_WIDGET);

    rut_box_layout_add(inspector->widget_hbox, true, widget);
    rut_object_unref(widget);

    if (widget_prop) {
        inspector->inspector_prop_closure = rig_property_connect_callback(
            widget_prop, inspector_property_changed_cb, inspector);
        inspector->widget_prop = widget_prop;
    }
}

static void
target_property_changed_cb(rig_property_t *target_prop,
                           void *user_data)
{
    rig_prop_inspector_t *inspector = user_data;

    /* XXX: We temporarily stop listening for changes to the
     * target_property to ignore any intermediate changes might be made
     * while re-loading the property...
     */

    rig_property_closure_destroy(inspector->target_prop_closure);
    inspector->target_prop_closure = NULL;

    rig_prop_inspector_reload_property(inspector);

    inspector->target_prop_closure = rig_property_connect_callback(
        inspector->target_prop, target_property_changed_cb, inspector);
}

static rut_input_event_status_t
block_input_cb(rut_input_region_t *region,
               rut_input_event_t *event,
               void *user_data)
{
    return RUT_INPUT_EVENT_STATUS_HANDLED;
}

rig_prop_inspector_t *
rig_prop_inspector_new(
    rut_shell_t *shell,
    rig_property_t *property,
    rig_prop_inspector_callback_t inspector_property_changed_cb,
    rig_prop_inspector_controlled_callback_t inspector_controlled_cb,
    bool with_label,
    void *user_data)
{
    rig_prop_inspector_t *inspector =
        rut_object_alloc0(rig_prop_inspector_t,
                          &rig_prop_inspector_type,
                          _rig_prop_inspector_init_type);
    rut_bin_t *grab_padding;

    inspector->shell = shell;

    rut_graphable_init(inspector);

    inspector->target_prop = property;
    inspector->inspector_property_changed_cb = inspector_property_changed_cb;
    inspector->controlled_changed_cb = inspector_controlled_cb;
    inspector->user_data = user_data;

    inspector->top_stack = rut_stack_new(shell, 1, 1);
    rut_graphable_add_child(inspector, inspector->top_stack);
    rut_object_unref(inspector->top_stack);

    inspector->top_hbox =
        rut_box_layout_new(shell, RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);
    rut_stack_add(inspector->top_stack, inspector->top_hbox);
    rut_object_unref(inspector->top_hbox);

    /* XXX: Hack for now, to make sure its possible to drag and drop any
     * property without inadvertanty manipulating the property value...
     */
    grab_padding = rut_bin_new(inspector->shell);
    rut_bin_set_right_padding(grab_padding, 15);
    rut_box_layout_add(inspector->top_hbox, false, grab_padding);
    rut_object_unref(grab_padding);

    if (inspector->controlled_changed_cb && property->spec->animatable)
        add_controlled_toggle(inspector, property);

    inspector->widget_stack = rut_stack_new(shell, 1, 1);
    rut_box_layout_add(inspector->top_hbox, true, inspector->widget_stack);
    rut_object_unref(inspector->widget_stack);

    inspector->widget_hbox = rut_box_layout_new(
        inspector->shell, RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);
    rut_stack_add(inspector->widget_stack, inspector->widget_hbox);
    rut_object_unref(inspector->widget_hbox);

    inspector->disabled_overlay =
        rut_rectangle_new4f(shell, 1, 1, 0.5, 0.5, 0.5, 0.5);
    inspector->input_region =
        rut_input_region_new_rectangle(0, 0, 1, 1, block_input_cb, NULL);

    add_control(inspector, property, with_label);

    rig_prop_inspector_reload_property(inspector);

    rut_sizable_set_size(inspector, 10, 10);

    inspector->target_prop_closure = rig_property_connect_callback(
        property, target_property_changed_cb, inspector);

    return inspector;
}

void
rig_prop_inspector_reload_property(rig_prop_inspector_t *inspector)
{
    if (inspector->target_prop) {
        bool old_reloading = inspector->reloading_property;

        inspector->reloading_property = true;

        if (inspector->widget_prop) {
            if (inspector->target_prop->spec->type !=
                inspector->widget_prop->spec->type) {
                rig_property_cast_scalar_value(
                    &inspector->shell->property_ctx,
                    inspector->widget_prop,
                    inspector->target_prop);
            } else
                rig_property_copy_value(&inspector->shell->property_ctx,
                                        inspector->widget_prop,
                                        inspector->target_prop);
        }

        inspector->reloading_property = old_reloading;
    }
}

void
rig_prop_inspector_set_controlled(rig_prop_inspector_t *inspector,
                                  bool controlled)
{
    if (inspector->controlled_toggle) {
        bool old_reloading = inspector->reloading_property;

        inspector->reloading_property = true;

        rut_icon_toggle_set_state(inspector->controlled_toggle, controlled);

        inspector->reloading_property = old_reloading;
    }
}

rig_property_t *
rig_prop_inspector_get_property(rig_prop_inspector_t *inspector)
{
    return inspector->target_prop;
}
