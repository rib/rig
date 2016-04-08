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

#include "rig-inspector.h"
#include "rig-prop-inspector.h"

#define RIG_INSPECTOR_EDGE_GAP 5
#define RIG_INSPECTOR_PROPERTY_GAP 5

typedef struct {
    rut_stack_t *stack;
    rut_object_t *control;
    rut_drag_bin_t *drag_bin;
    rig_property_t *source_prop;
    rig_property_t *target_prop;

    /* A pointer is stored back to the inspector so that we can use a
     * pointer to this data directly as the callback data for the
     * property binding */
    rig_inspector_t *inspector;
} rig_inspector_property_data_t;

struct _rig_inspector_t {
    rut_object_base_t _base;

    rut_shell_t *shell;
    c_llist_t *objects;

    rut_graphable_props_t graphable;

    rut_box_layout_t *vbox;

    int n_props;
    rig_inspector_property_data_t *prop_data;

    rig_inspector_callback_t property_changed_cb;
    rig_inspector_controlled_callback_t controlled_changed_cb;
    void *user_data;
};

rut_type_t rig_inspector_type;

static void
_rig_inspector_free(void *object)
{
    rig_inspector_t *inspector = object;

    c_llist_foreach(inspector->objects, (GFunc)rut_object_unref, NULL);
    c_llist_free(inspector->objects);
    inspector->objects = NULL;

    c_free(inspector->prop_data);

    rut_graphable_destroy(inspector);

    rut_object_free(rig_inspector_t, inspector);
}

static void
_rig_inspector_init_type(void)
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

    rut_type_t *type = &rig_inspector_type;
#define TYPE rig_inspector_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_inspector_free);
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
                       offsetof(TYPE, vbox),
                       NULL); /* no vtable */

#undef TYPE
}

static void
property_changed_cb(rig_property_t *primary_target_prop,
                    rig_property_t *source_prop,
                    void *user_data)
{
    rig_inspector_property_data_t *prop_data = user_data;
    rig_inspector_t *inspector = prop_data->inspector;
    c_llist_t *l;
    bool mergable;

    c_return_if_fail(primary_target_prop == prop_data->target_prop);

    switch (source_prop->spec->type) {
    case RUT_PROPERTY_TYPE_FLOAT:
    case RUT_PROPERTY_TYPE_DOUBLE:
    case RUT_PROPERTY_TYPE_INTEGER:
    case RUT_PROPERTY_TYPE_UINT32:
    case RUT_PROPERTY_TYPE_VEC3:
    case RUT_PROPERTY_TYPE_VEC4:
    case RUT_PROPERTY_TYPE_QUATERNION:
        mergable = true;
        break;
    default:
        mergable = false;
    }

    /* Forward the property change to the corresponding property
     * of all objects being inspected... */
    for (l = inspector->objects; l; l = l->next) {
        rig_property_t *target_prop = rut_introspectable_lookup_property(
            l->data, primary_target_prop->spec->name);
        inspector->property_changed_cb(target_prop, /* target */
                                       source_prop,
                                       mergable,
                                       inspector->user_data);
    }
}

static void
controlled_changed_cb(rig_property_t *primary_property,
                      bool value,
                      void *user_data)
{
    rig_inspector_property_data_t *prop_data = user_data;
    rig_inspector_t *inspector = prop_data->inspector;
    c_llist_t *l;

    c_return_if_fail(primary_property == prop_data->target_prop);

    /* Forward the controlled state change to the corresponding property
     * of all objects being inspected... */
    for (l = inspector->objects; l; l = l->next) {
        rig_property_t *property = rut_introspectable_lookup_property(
            l->data, primary_property->spec->name);

        inspector->controlled_changed_cb(property, value, inspector->user_data);
    }
}

static void
get_all_properties_cb(rig_property_t *prop, void *user_data)
{
    c_array_t *array = user_data;
    rig_inspector_property_data_t *prop_data;

    c_array_set_size(array, array->len + 1);
    prop_data =
        &c_array_index(array, rig_inspector_property_data_t, array->len - 1);
    prop_data->target_prop = prop;
}

static void
create_property_controls(rig_inspector_t *inspector)
{
    rut_object_t *reference_object = inspector->objects->data;
    c_array_t *props;
    int i;

    props = c_array_new(false, /* not zero terminated */
                        false, /* don't clear */
                        sizeof(rig_inspector_property_data_t));

    if (rut_object_is(reference_object, RUT_TRAIT_ID_INTROSPECTABLE))
        rut_introspectable_foreach_property(
            reference_object, get_all_properties_cb, props);

    inspector->n_props = props->len;

    inspector->prop_data =
        ((rig_inspector_property_data_t *)c_array_free(props, false));

    for (i = 0; i < inspector->n_props; i++) {
        rig_inspector_property_data_t *prop_data = inspector->prop_data + i;
        rut_object_t *control;
        rut_bin_t *bin;

        prop_data->inspector = inspector;

        prop_data->stack = rut_stack_new(inspector->shell, 1, 1);
        rut_box_layout_add(inspector->vbox, false, prop_data->stack);
        rut_object_unref(prop_data->stack);

        prop_data->drag_bin = rut_drag_bin_new(inspector->shell);
        rut_graphable_add_child(prop_data->stack, prop_data->drag_bin);
        rut_object_unref(prop_data->drag_bin);

        bin = rut_bin_new(inspector->shell);
        rut_bin_set_bottom_padding(bin, 5);
        rut_drag_bin_set_child(prop_data->drag_bin, bin);
        rut_object_unref(bin);

        control = rig_prop_inspector_new(inspector->shell,
                                         prop_data->target_prop,
                                         property_changed_cb,
                                         controlled_changed_cb,
                                         true,
                                         prop_data);
        rut_bin_set_child(bin, control);
        rut_object_unref(control);

        /* XXX: It could be better if the payload could represent the selection
         * of multiple properties when an inspector is inspecting multiple
         * selected objects... */
        rut_drag_bin_set_payload(prop_data->drag_bin, control);

        prop_data->control = control;
    }
}

rig_inspector_t *
rig_inspector_new(rut_shell_t *shell,
                  c_llist_t *objects,
                  rig_inspector_callback_t user_property_changed_cb,
                  rig_inspector_controlled_callback_t user_controlled_changed_cb,
                  void *user_data)
{
    rig_inspector_t *inspector = rut_object_alloc0(
        rig_inspector_t, &rig_inspector_type, _rig_inspector_init_type);

    inspector->shell = shell;
    inspector->objects = c_llist_copy(objects);

    c_llist_foreach(objects, (GFunc)rut_object_ref, NULL);

    inspector->property_changed_cb = user_property_changed_cb;
    inspector->controlled_changed_cb = user_controlled_changed_cb;
    inspector->user_data = user_data;

    rut_graphable_init(inspector);

    inspector->vbox =
        rut_box_layout_new(shell, RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM);
    rut_graphable_add_child(inspector, inspector->vbox);
    rut_object_unref(inspector->vbox);

    create_property_controls(inspector);

    rut_sizable_set_size(inspector, 10, 10);

    return inspector;
}

void
rig_inspector_reload_property(rig_inspector_t *inspector,
                              rig_property_t *property)
{
    int i;

    for (i = 0; i < inspector->n_props; i++) {
        rig_inspector_property_data_t *prop_data = inspector->prop_data + i;

        if (prop_data->target_prop == property)
            rig_prop_inspector_reload_property(prop_data->control);
    }
}

void
rig_inspector_set_property_controlled(rig_inspector_t *inspector,
                                      rig_property_t *property,
                                      bool controlled)
{
    int i;

    for (i = 0; i < inspector->n_props; i++) {
        rig_inspector_property_data_t *prop_data = inspector->prop_data + i;

        if (prop_data->target_prop == property)
            rig_prop_inspector_set_controlled(prop_data->control, controlled);
    }
}
