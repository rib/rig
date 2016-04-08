/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013 Intel Corporation.
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

#include <rut.h>

#include "rig-code.h"
#include "rig-binding.h"
#include "rig-prop-inspector.h"

typedef struct _rig_binding_view_t rig_binding_view_t;

typedef struct _dependency_t {
    rig_binding_view_t *binding_view;

    rut_object_t *object;
    rig_property_t *property;

    bool preview;

    rut_box_layout_t *hbox;
    rut_text_t *label;
    rut_text_t *variable_name_label;

} dependency_t;

struct _rig_binding_view_t {
    rut_object_base_t _base;

    rig_engine_t *engine;

    rut_graphable_props_t graphable;

    rut_stack_t *top_stack;
    rut_drag_bin_t *drag_bin;

    rut_box_layout_t *vbox;

    rut_box_layout_t *dependencies_vbox;

    rut_stack_t *drop_stack;
    rut_input_region_t *drop_region;
    rut_text_t *drop_label;

    rig_binding_t *binding;

    rut_text_t *code_view;

    rig_property_t *preview_dependency_prop;
    c_llist_t *dependencies;
};

static void
_rig_binding_view_free(void *object)
{
    rig_binding_view_t *binding_view = object;
    // rig_controller_view_t *view = binding_view->view;

    rut_object_unref(binding_view->binding);

    rut_graphable_destroy(binding_view);

    rut_object_free(rig_binding_view_t, binding_view);
}

rut_type_t rig_binding_view_type;

static void
_rig_binding_view_init_type(void)
{
    static rut_graphable_vtable_t graphable_vtable = { 0 };

    static rut_sizable_vtable_t sizable_vtable = {
        rut_composite_sizable_set_size,
        rut_composite_sizable_get_size,
        rut_composite_sizable_get_preferred_width,
        rut_composite_sizable_get_preferred_height,
        rut_composite_sizable_add_preferred_size_callback
    };

    rut_type_t *type = &rig_binding_view_type;
#define TYPE rig_binding_view_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_binding_view_free);
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
remove_dependency(rig_binding_view_t *binding_view,
                  rig_property_t *property)
{
    c_llist_t *l;

    for (l = binding_view->dependencies; l; l = l->next) {
        dependency_t *dependency = l->data;
        if (dependency->property == property) {
            if (!dependency->preview)
                rig_binding_remove_dependency(binding_view->binding, property);

            rut_box_layout_remove(binding_view->dependencies_vbox,
                                  dependency->hbox);
            rut_object_unref(dependency->object);
            c_slice_free(dependency_t, dependency);

            return;
        }
    }

    c_warn_if_reached();
}

static void
on_dependency_delete_button_click_cb(rut_icon_button_t *button,
                                     void *user_data)
{
    dependency_t *dependency = user_data;

    remove_dependency(dependency->binding_view, dependency->property);
}

static void
dependency_name_changed_cb(rut_text_t *text, void *user_data)
{
    dependency_t *dependency = user_data;

    rig_binding_set_dependency_name(dependency->binding_view->binding,
                                    dependency->property,
                                    rut_text_get_text(text));
}

static dependency_t *
add_dependency(rig_binding_view_t *binding_view,
               rig_property_t *property,
               bool drag_preview)
{
    dependency_t *dependency = c_slice_new0(dependency_t);
    rig_property_t *label_prop;
    char *dependency_label;
    rut_object_t *object = property->object;
    rut_bin_t *bin;
    const char *component_str = NULL;
    const char *label_str;
    rut_shell_t *shell = binding_view->engine->shell;

    dependency->object = rut_object_ref(object);
    dependency->binding_view = binding_view;

    dependency->property = property;

    dependency->preview = drag_preview;

    dependency->hbox =
        rut_box_layout_new(shell, RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);

    if (!drag_preview) {
        rut_icon_button_t *delete_button =
            rut_icon_button_new(shell,
                                NULL, /* label */
                                0, /* ignore label position */
                                "delete-white.png", /* normal */
                                "delete-white.png", /* hover */
                                "delete.png", /* active */
                                "delete-white.png"); /* disabled */
        rut_box_layout_add(dependency->hbox, false, delete_button);
        rut_object_unref(delete_button);
        rut_icon_button_add_on_click_callback(
            delete_button,
            on_dependency_delete_button_click_cb,
            dependency,
            NULL); /* destroy notify */
    }

    /* XXX:
     * We want a clear way of showing a relationship to an object +
     * property here.
     *
     * Just showing a property name isn't really enough
     * */

    if (rut_object_is(object, RUT_TRAIT_ID_COMPONENTABLE)) {
        rut_componentable_props_t *component =
            rut_object_get_properties(object, RUT_TRAIT_ID_COMPONENTABLE);
        rig_entity_t *entity = component->entity;
        label_prop = rig_introspectable_lookup_property(entity, "label");
        /* XXX: Hack to drop the "Rut" prefix from the name... */
        component_str = rut_object_get_type_name(object) + 3;
    } else
        label_prop = rig_introspectable_lookup_property(object, "label");

    if (label_prop)
        label_str = rig_property_get_text(label_prop);
    else
        label_str = "<Object>";

    if (component_str) {
        dependency_label = c_strdup_printf(
            "%s::%s::%s", label_str, component_str, property->spec->name);
    } else {
        dependency_label =
            c_strdup_printf("%s::%s", label_str, property->spec->name);
    }

    dependency->label = rut_text_new_with_text(shell, NULL, dependency_label);
    c_free(dependency_label);
    rut_box_layout_add(dependency->hbox, false, dependency->label);
    rut_object_unref(dependency->label);

    bin = rut_bin_new(shell);
    rut_bin_set_left_padding(bin, 20);
    rut_box_layout_add(dependency->hbox, false, bin);
    rut_object_unref(bin);

    /* TODO: Check if the name is unique for the current binding... */
    dependency->variable_name_label =
        rut_text_new_with_text(shell, NULL, property->spec->name);
    rut_text_set_editable(dependency->variable_name_label, true);
    rut_bin_set_child(bin, dependency->variable_name_label);
    rut_object_unref(dependency->variable_name_label);

    rut_text_add_text_changed_callback(dependency->variable_name_label,
                                       dependency_name_changed_cb,
                                       dependency,
                                       NULL); /* destroy notify */

    binding_view->dependencies =
        c_llist_prepend(binding_view->dependencies, dependency);

    rut_box_layout_add(
        binding_view->dependencies_vbox, false, dependency->hbox);
    rut_object_unref(dependency->hbox);

    if (!drag_preview) {
        rig_binding_add_dependency(
            binding_view->binding, property, property->spec->name);
    }

    return dependency;
}

static rut_input_event_status_t
drop_region_input_cb(rut_input_region_t *region,
                     rut_input_event_t *event,
                     void *user_data)
{
    rig_binding_view_t *binding_view = user_data;

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_DROP_OFFER) {
        rut_object_t *payload = rut_drop_offer_event_get_payload(event);

        if (rut_object_get_type(payload) == &rig_prop_inspector_type) {
            rig_property_t *property = rig_prop_inspector_get_property(payload);

            c_debug("Drop Offer\n");

            binding_view->preview_dependency_prop = property;
            add_dependency(binding_view, property, true);

            rut_shell_onscreen_take_drop_offer(rut_input_event_get_onscreen(event),
                                               binding_view->drop_region);
            return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    } else if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_DROP) {
        rut_object_t *payload = rut_drop_offer_event_get_payload(event);

        /* We should be able to assume a _DROP_OFFER was accepted before
         * we'll be sent a _DROP */
        c_warn_if_fail(binding_view->preview_dependency_prop);

        remove_dependency(binding_view, binding_view->preview_dependency_prop);
        binding_view->preview_dependency_prop = NULL;

        if (rut_object_get_type(payload) == &rig_prop_inspector_type) {
            rig_property_t *property = rig_prop_inspector_get_property(payload);

            add_dependency(binding_view, property, false);

            return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    } else if (rut_input_event_get_type(event) ==
               RUT_INPUT_EVENT_TYPE_DROP_CANCEL) {
        /* NB: This may be cleared by a _DROP */
        if (binding_view->preview_dependency_prop) {
            remove_dependency(binding_view,
                              binding_view->preview_dependency_prop);
            binding_view->preview_dependency_prop = NULL;
        }

        return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
text_changed_cb(rut_text_t *text, void *user_data)
{
    rig_binding_view_t *binding_view = user_data;

    rig_binding_set_expression(binding_view->binding, rut_text_get_text(text));
}

rig_binding_view_t *
rig_binding_view_new(rig_engine_t *engine,
                     rig_property_t *property,
                     rig_binding_t *binding)
{
    rut_shell_t *shell = engine->shell;
    rig_binding_view_t *binding_view =
        rut_object_alloc0(rig_binding_view_t,
                          &rig_binding_view_type,
                          _rig_binding_view_init_type);
    rut_bin_t *dependencies_indent;
    rut_box_layout_t *hbox;
    rut_text_t *equals;

    binding_view->engine = engine;

    rut_graphable_init(binding_view);

    binding_view->binding = rut_object_ref(binding);

    binding_view->top_stack = rut_stack_new(shell, 1, 1);
    rut_graphable_add_child(binding_view, binding_view->top_stack);
    rut_object_unref(binding_view->top_stack);

    binding_view->vbox =
        rut_box_layout_new(shell, RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM);
    rut_stack_add(binding_view->top_stack, binding_view->vbox);
    rut_object_unref(binding_view->vbox);

    binding_view->drop_stack = rut_stack_new(shell, 1, 1);
    rut_box_layout_add(binding_view->vbox, false, binding_view->drop_stack);
    rut_object_unref(binding_view->drop_stack);

    binding_view->drop_label =
        rut_text_new_with_text(shell, NULL, "Dependencies...");
    rut_stack_add(binding_view->drop_stack, binding_view->drop_label);
    rut_object_unref(binding_view->drop_label);

    binding_view->drop_region = rut_input_region_new_rectangle(
        0, 0, 1, 1, drop_region_input_cb, binding_view);
    rut_stack_add(binding_view->drop_stack, binding_view->drop_region);
    rut_object_unref(binding_view->drop_region);

    dependencies_indent = rut_bin_new(shell);
    rut_box_layout_add(binding_view->vbox, false, dependencies_indent);
    rut_object_unref(dependencies_indent);
    rut_bin_set_left_padding(dependencies_indent, 10);

    binding_view->dependencies_vbox =
        rut_box_layout_new(shell, RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM);
    rut_bin_set_child(dependencies_indent, binding_view->dependencies_vbox);
    rut_object_unref(binding_view->dependencies_vbox);

    hbox = rut_box_layout_new(shell, RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);
    rut_box_layout_add(binding_view->vbox, false, hbox);
    rut_object_unref(hbox);

    equals = rut_text_new_with_text(shell, "bold", "=");
    rut_box_layout_add(hbox, false, equals);
    rut_object_unref(equals);

    binding_view->code_view = rut_text_new_with_text(shell, "monospace", "");
    rut_text_set_hint_text(binding_view->code_view, "Expression...");
    rut_text_set_editable(binding_view->code_view, true);
    rut_box_layout_add(hbox, false, binding_view->code_view);
    rut_object_unref(binding_view->code_view);

    rut_text_add_text_changed_callback(binding_view->code_view,
                                       text_changed_cb,
                                       binding_view,
                                       NULL); /* destroy notify */
    return binding_view;
}
