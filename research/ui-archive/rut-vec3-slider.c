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
#include "rut-vec3-slider.h"
#include "rut-text.h"
#include "rut-box-layout.h"
#include "rut-composite-sizable.h"

enum {
    RUT_VEC3_SLIDER_PROP_VALUE,
    RUT_VEC3_SLIDER_N_PROPS
};

typedef struct {
    rut_number_slider_t *slider;
    rig_property_t *property;
} rut_vec3_slider_component_t;

struct _rut_vec3_slider_t {
    rut_object_base_t _base;

    rut_shell_t *shell;

    rut_graphable_props_t graphable;

    rut_box_layout_t *hbox;

    rut_vec3_slider_component_t components[3];

    bool in_set_value;
    float value[3];

    rig_introspectable_props_t introspectable;
    rig_property_t properties[RUT_VEC3_SLIDER_N_PROPS];
};

rut_type_t rut_vec3_slider_type;

static rig_property_spec_t _rut_vec3_slider_prop_specs[] = {
    { .name = "value",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_VEC3,
      .data_offset = offsetof(rut_vec3_slider_t, value),
      .setter.vec3_type = rut_vec3_slider_set_value, },
    { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rut_vec3_slider_free(void *object)
{
    rut_vec3_slider_t *slider = object;

    rig_introspectable_destroy(slider);
    rut_graphable_destroy(slider);

    rut_object_free(rut_vec3_slider_t, slider);
}

static void
_rut_vec3_slider_init_type(void)
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

    rut_type_t *type = &rut_vec3_slider_type;
#define TYPE rut_vec3_slider_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rut_vec3_slider_free);
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
rut_vec3_slider_property_changed_cb(rig_property_t *target_property,
                                    void *user_data)
{
    rut_vec3_slider_t *slider = user_data;
    float value[3];
    int i;

    if (slider->in_set_value)
        return;

    for (i = 0; i < 3; i++)
        value[i] = rut_number_slider_get_value(slider->components[i].slider);

    rut_vec3_slider_set_value(slider, value);
}

rut_vec3_slider_t *
rut_vec3_slider_new(rut_shell_t *shell)
{
    rut_vec3_slider_t *slider = rut_object_alloc0(
        rut_vec3_slider_t, &rut_vec3_slider_type, _rut_vec3_slider_init_type);
    int i;

    slider->shell = shell;

    rut_graphable_init(slider);

    rig_introspectable_init(
        slider, _rut_vec3_slider_prop_specs, slider->properties);

    slider->hbox =
        rut_box_layout_new(shell, RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);
    rut_graphable_add_child(slider, slider->hbox);
    rut_object_unref(slider->hbox);

    for (i = 0; i < 3; i++) {
        rut_text_t *text;

        slider->components[i].slider = rut_number_slider_new(shell);
        rut_box_layout_add(slider->hbox, false, slider->components[i].slider);
        rut_object_unref(slider->components[i].slider);

        if (i != 2) {
            text = rut_text_new_with_text(shell, NULL, ", ");
            rut_box_layout_add(slider->hbox, false, text);
            rut_object_unref(text);
        }

        slider->components[i].property = rig_introspectable_lookup_property(
            slider->components[i].slider, "value");
    }

    rut_number_slider_set_markup_label(slider->components[0].slider,
                                       "<span foreground=\"red\">x:</span>");
    rut_number_slider_set_markup_label(slider->components[1].slider,
                                       "<span foreground=\"green\">y:</span>");
    rut_number_slider_set_markup_label(slider->components[2].slider,
                                       "<span foreground=\"blue\">z:</span>");

    rig_property_set_binding(&slider->properties[RUT_VEC3_SLIDER_PROP_VALUE],
                             rut_vec3_slider_property_changed_cb,
                             slider,
                             slider->components[0].property,
                             slider->components[1].property,
                             slider->components[2].property,
                             NULL);

    rut_sizable_set_size(slider, 60, 30);

    return slider;
}

void
rut_vec3_slider_set_min_value(rut_vec3_slider_t *slider, float min_value)
{
    int i;

    for (i = 0; i < 3; i++) {
        rut_vec3_slider_component_t *component = slider->components + i;
        rut_number_slider_set_min_value(component->slider, min_value);
    }
}

void
rut_vec3_slider_set_max_value(rut_vec3_slider_t *slider, float max_value)
{
    int i;

    for (i = 0; i < 3; i++) {
        rut_vec3_slider_component_t *component = slider->components + i;
        rut_number_slider_set_max_value(component->slider, max_value);
    }
}

void
rut_vec3_slider_set_value(rut_object_t *obj, const float *value)
{
    rut_vec3_slider_t *slider = obj;
    int i;

    memcpy(slider->value, value, sizeof(float) * 3);

    /* Normally we update slider->value[] based on notifications from
     * the per-component slider controls, but since we are manually
     * updating the controls here we need to temporarily ignore
     * the notifications so we avoid any recursion
     *
     * Note: If we change property notifications be deferred to the
     * mainloop then this mechanism will become redundant
     */
    slider->in_set_value = true;
    for (i = 0; i < 3; i++) {
        rut_vec3_slider_component_t *component = slider->components + i;
        rut_number_slider_set_value(component->slider, value[i]);
    }

    slider->in_set_value = false;

    rig_property_dirty(&slider->shell->property_ctx,
                       &slider->properties[RUT_VEC3_SLIDER_PROP_VALUE]);
}

void
rut_vec3_slider_set_step(rut_vec3_slider_t *slider, float step)
{
    int i;

    for (i = 0; i < 3; i++) {
        rut_vec3_slider_component_t *component = slider->components + i;
        rut_number_slider_set_step(component->slider, step);
    }
}

int
rut_vec3_slider_get_decimal_places(rut_vec3_slider_t *slider)
{
    return rut_number_slider_get_decimal_places(slider->components[0].slider);
}

void
rut_vec3_slider_set_decimal_places(rut_vec3_slider_t *slider,
                                   int decimal_places)
{
    int i;

    for (i = 0; i < 3; i++) {
        rut_vec3_slider_component_t *component = slider->components + i;
        rut_number_slider_set_decimal_places(component->slider, decimal_places);
    }
}
