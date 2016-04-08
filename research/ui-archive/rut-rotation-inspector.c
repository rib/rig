/*
 * Rut
 *
 * Rig Utilities
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

#include "rut-number-slider.h"
#include "rut-rotation-inspector.h"
#include "rut-text.h"
#include "rut-box-layout.h"
#include "rut-composite-sizable.h"
#include "rut-util.h"

enum {
    RUT_ROTATION_INSPECTOR_PROP_VALUE,
    RUT_ROTATION_INSPECTOR_N_PROPS
};

typedef struct {
    rut_number_slider_t *slider;
    rig_property_t *property;
} rut_rotation_inspector_component_t;

struct _rut_rotation_inspector_t {
    rut_object_base_t _base;

    rut_shell_t *shell;

    rut_graphable_props_t graphable;

    rut_box_layout_t *hbox;

    rut_rotation_inspector_component_t components[4];

    float user_values[4];
    float user_axis_magnitude;

    c_quaternion_t value;

    rut_introspectable_props_t introspectable;
    rig_property_t properties[RUT_ROTATION_INSPECTOR_N_PROPS];
};

rut_type_t rut_rotation_inspector_type;

static void
rut_rotation_inspector_property_changed_cb(rig_property_t *target_property,
                                           void *user_data);

static rig_property_spec_t _rut_rotation_inspector_prop_specs[] = {
    { .name = "value",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_QUATERNION,
      .data_offset = offsetof(rut_rotation_inspector_t, value),
      .setter.quaternion_type = rut_rotation_inspector_set_value, },
    { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rut_rotation_inspector_free(void *object)
{
    rut_rotation_inspector_t *inspector = object;

    rut_introspectable_destroy(inspector);
    rut_graphable_destroy(inspector);

    rut_object_free(rut_rotation_inspector_t, inspector);
}

static void
_rut_rotation_inspector_init_type(void)
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

    rut_type_t *type = &rut_rotation_inspector_type;
#define TYPE rut_rotation_inspector_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rut_rotation_inspector_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_GRAPHABLE,
                       offsetof(TYPE, graphable),
                       &graphable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INTROSPECTABLE,
                       offsetof(TYPE, introspectable),
                       NULL); /* no implied vtable */
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_SIZABLE,
                       0, /* no implied properties */
                       &sizable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_COMPOSITE_SIZABLE,
                       offsetof(TYPE, hbox),
                       NULL); /* no vtable */

#undef TYPE
}

static void
enable_value_binding(rut_rotation_inspector_t *inspector)
{
    rig_property_t *rotation_prop =
        &inspector->properties[RUT_ROTATION_INSPECTOR_PROP_VALUE];

    rig_property_set_binding(rotation_prop,
                             rut_rotation_inspector_property_changed_cb,
                             inspector,
                             inspector->components[0].property,
                             inspector->components[1].property,
                             inspector->components[2].property,
                             inspector->components[3].property,
                             NULL);
}

static void
disable_value_binding(rut_rotation_inspector_t *inspector)
{
    rig_property_t *value_prop =
        &inspector->properties[RUT_ROTATION_INSPECTOR_PROP_VALUE];
    rig_property_remove_binding(value_prop);
}

void
set_value(rut_rotation_inspector_t *inspector,
          const c_quaternion_t *value,
          bool user_edit)
{
    if (memcmp(&inspector->value, value, sizeof(c_quaternion_t)) == 0)
        return;

    inspector->value = *value;

    if (!user_edit) {
        float axis[3];
        float angle;

        c_quaternion_get_rotation_axis(value, axis);
        angle = c_quaternion_get_rotation_angle(value);

        /* With an angle of 0 or 360, the axis is arbitrary so it's
         * better for editing continuity to use the users last specified
         * axis... */
        if ((angle == 0 || angle == 360) && axis[0] == 1.0) {
            axis[0] = inspector->user_values[0];
            axis[1] = inspector->user_values[1];
            axis[2] = inspector->user_values[2];
        }

        /* Normally we update inspector->value[] based on notifications from
         * the per-component inspector controls, but since we are manually
         * updating the controls here we need to temporarily ignore
         * the notifications so we avoid any recursion
         *
         * Note: If we change property notifications to be deferred to the
         * mainloop then this mechanism will become redundant
         */
        disable_value_binding(inspector);

        /* The axis we get for a quaternion will always be normalized but if
         * the user has been entering axis components with a certain scale
         * we want to keep the slider values in a similar scale... */
        rut_number_slider_set_value(inspector->components[0].slider,
                                    axis[0] * inspector->user_axis_magnitude);
        rut_number_slider_set_value(inspector->components[1].slider,
                                    axis[1] * inspector->user_axis_magnitude);
        rut_number_slider_set_value(inspector->components[2].slider,
                                    axis[2] * inspector->user_axis_magnitude);
        rut_number_slider_set_value(inspector->components[3].slider, angle);

        enable_value_binding(inspector);
    }

    rig_property_dirty(
        &inspector->shell->property_ctx,
        &inspector->properties[RUT_ROTATION_INSPECTOR_PROP_VALUE]);
}

static void
rut_rotation_inspector_property_changed_cb(rig_property_t *target_property,
                                           void *user_data)
{
    rut_rotation_inspector_t *inspector = user_data;
    c_quaternion_t value;
    float axis[3];
    float angle;
    int i;

    for (i = 0; i < 3; i++)
        axis[i] = rut_number_slider_get_value(inspector->components[i].slider);
    angle = rut_number_slider_get_value(inspector->components[3].slider);

    c_quaternion_init(&value, angle, axis[0], axis[1], axis[2]);

    inspector->user_values[0] = axis[0];
    inspector->user_values[1] = axis[1];
    inspector->user_values[2] = axis[2];
    inspector->user_values[3] = angle;
    inspector->user_axis_magnitude = c_vector3_magnitude(axis);

    rut_rotation_inspector_set_value(inspector, &value);

    set_value(inspector, &value, true);
}

rut_rotation_inspector_t *
rut_rotation_inspector_new(rut_shell_t *shell)
{
    rut_rotation_inspector_t *inspector =
        rut_object_alloc0(rut_rotation_inspector_t,
                          &rut_rotation_inspector_type,
                          _rut_rotation_inspector_init_type);
    rut_text_t *text;
    int i;

    inspector->shell = shell;

    inspector->user_axis_magnitude = 1;

    /* These user values are saved and used when a quaternion value is
     * given non interactively. We want out default axis be (0, 0, 1)
     * since we guess it's most common to want to rotate UI components
     * around the z axis...*/
    inspector->user_values[0] = 0;
    inspector->user_values[1] = 0;
    inspector->user_values[2] = 1;
    inspector->user_values[3] = 0;

    rut_graphable_init(inspector);

    rut_introspectable_init(
        inspector, _rut_rotation_inspector_prop_specs, inspector->properties);

    inspector->hbox =
        rut_box_layout_new(shell, RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);
    rut_graphable_add_child(inspector, inspector->hbox);
    rut_object_unref(inspector->hbox);

    /*
     * Axis
     */

    text = rut_text_new_with_text(shell, NULL, "(");
    rut_box_layout_add(inspector->hbox, false, text);
    rut_object_unref(text);

    for (i = 0; i < 3; i++) {
        inspector->components[i].slider = rut_number_slider_new(shell);
        rut_box_layout_add(
            inspector->hbox, false, inspector->components[i].slider);
        rut_object_unref(inspector->components[i].slider);

        rut_number_slider_set_min_value(inspector->components[i].slider,
                                        -G_MAXFLOAT);
        rut_number_slider_set_max_value(inspector->components[i].slider,
                                        G_MAXFLOAT);

        if (i != 2) {
            text = rut_text_new_with_text(shell, NULL, ", ");
            rut_box_layout_add(inspector->hbox, false, text);
            rut_object_unref(text);
        }

        inspector->components[i].property = rut_introspectable_lookup_property(
            inspector->components[i].slider, "value");
    }

    text = rut_text_new_with_text(shell, NULL, ") ");
    rut_box_layout_add(inspector->hbox, false, text);
    rut_object_unref(text);

    rut_number_slider_set_markup_label(inspector->components[0].slider,
                                       "<span foreground=\"red\">x:</span>");
    rut_number_slider_set_markup_label(inspector->components[1].slider,
                                       "<span foreground=\"green\">y:</span>");
    rut_number_slider_set_markup_label(inspector->components[2].slider,
                                       "<span foreground=\"blue\">z:</span>");

    /*
     * Angle
     */

    inspector->components[3].slider = rut_number_slider_new(shell);

    rut_number_slider_set_min_value(inspector->components[i].slider, 0);
    rut_number_slider_set_max_value(inspector->components[i].slider, 360);

    rut_box_layout_add(inspector->hbox, false, inspector->components[3].slider);
    rut_object_unref(inspector->components[3].slider);

    rut_number_slider_set_markup_label(inspector->components[3].slider,
                                       "<span foreground=\"yellow\">a:</span>");

    inspector->components[3].property = rut_introspectable_lookup_property(
        inspector->components[3].slider, "value");

    text = rut_text_new_with_text(shell, NULL, "°");
    rut_box_layout_add(inspector->hbox, false, text);
    rut_object_unref(text);

    enable_value_binding(inspector);

    rut_sizable_set_size(inspector, 60, 40);

    return inspector;
}

void
rut_rotation_inspector_set_value(rut_object_t *obj,
                                 const c_quaternion_t *value)
{
    rut_rotation_inspector_t *inspector = obj;

    set_value(inspector, value, false);
}

void
rut_rotation_inspector_set_step(rut_rotation_inspector_t *inspector,
                                float step)
{
    int i;

    for (i = 0; i < 4; i++) {
        rut_rotation_inspector_component_t *component =
            inspector->components + i;
        rut_number_slider_set_step(component->slider, step);
    }
}

int
rut_rotation_inspector_get_decimal_places(rut_rotation_inspector_t *inspector)
{
    return rut_number_slider_get_decimal_places(
        inspector->components[0].slider);
}

void
rut_rotation_inspector_set_decimal_places(rut_rotation_inspector_t *inspector,
                                          int decimal_places)
{
    int i;

    for (i = 0; i < 4; i++) {
        rut_rotation_inspector_component_t *component =
            inspector->components + i;
        rut_number_slider_set_decimal_places(component->slider, decimal_places);
    }
}
