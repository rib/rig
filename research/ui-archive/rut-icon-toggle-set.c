/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013  Intel Corporation.
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

#include "rut-shell.h"
#include "rut-interfaces.h"
#include "rut-transform.h"
#include "rut-box-layout.h"
#include "rut-icon-toggle.h"
#include "rut-icon-toggle-set.h"
#include "rut-composite-sizable.h"
#include "rig-introspectable.h"

typedef struct _rut_icon_toggle_set_state_t {
    c_list_t list_node;

    rut_icon_toggle_t *toggle;
    rut_closure_t *on_toggle_closure;

    int value;
} rut_icon_toggle_set_state_t;

enum {
    RUT_ICON_TOGGLE_SET_PROP_SELECTION,
    RUT_ICON_TOGGLE_SET_N_PROPS
};

struct _rut_icon_toggle_set_t {
    rut_object_base_t _base;

    rut_shell_t *shell;

    rut_box_layout_t *layout;

    c_list_t toggles_list;
    rut_icon_toggle_set_state_t *current_toggle_state;

    c_list_t on_change_cb_list;

    rut_graphable_props_t graphable;

    rig_introspectable_props_t introspectable;
    rig_property_t properties[RUT_ICON_TOGGLE_SET_N_PROPS];
};

static void
remove_toggle_state(rut_icon_toggle_set_state_t *toggle_state)
{
    c_list_remove(&toggle_state->list_node);
    rut_object_unref(toggle_state->toggle);
    c_slice_free(rut_icon_toggle_set_state_t, toggle_state);
}

static void
_rut_icon_toggle_set_free(void *object)
{
    rut_icon_toggle_set_t *toggle_set = object;
    rut_icon_toggle_set_state_t *toggle_state, *tmp;

    rut_closure_list_disconnect_all_FIXME(&toggle_set->on_change_cb_list);

    rut_graphable_destroy(toggle_set);

    c_list_for_each_safe(
        toggle_state, tmp, &toggle_set->toggles_list, list_node)
    {
        remove_toggle_state(toggle_state);
    }

    rig_introspectable_destroy(toggle_set);

    rut_object_free(rut_icon_toggle_set_t, object);
}

static rig_property_spec_t _rut_icon_toggle_set_prop_specs[] = {
    { .name = "selection",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .getter.integer_type = rut_icon_toggle_set_get_selection,
      .setter.integer_type = rut_icon_toggle_set_set_selection },
    { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

rut_type_t rut_icon_toggle_set_type;

static void
_rut_icon_toggle_set_init_type(void)
{
    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child remove */
                                                       NULL, /* child add */
                                                       NULL /* parent changed */
    };

    static rut_sizable_vtable_t sizable_vtable = {
        rut_composite_sizable_set_size,
        rut_composite_sizable_get_size,
        rut_composite_sizable_get_preferred_width,
        rut_composite_sizable_get_preferred_height,
        rut_composite_sizable_add_preferred_size_callback
    };

    rut_type_t *type = &rut_icon_toggle_set_type;
#define TYPE rut_icon_toggle_set_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rut_icon_toggle_set_free);
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
                       offsetof(TYPE, layout),
                       NULL); /* no vtable */
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INTROSPECTABLE,
                       offsetof(TYPE, introspectable),
                       NULL); /* no implied vtable */

#undef TYPE
}

rut_icon_toggle_set_t *
rut_icon_toggle_set_new(rut_shell_t *shell,
                        rut_icon_toggle_set_packing_t packing)
{
    rut_icon_toggle_set_t *toggle_set =
        rut_object_alloc0(rut_icon_toggle_set_t,
                          &rut_icon_toggle_set_type,
                          _rut_icon_toggle_set_init_type);
    rut_box_layout_packing_t box_packing;

    c_list_init(&toggle_set->on_change_cb_list);
    c_list_init(&toggle_set->toggles_list);

    rut_graphable_init(toggle_set);

    rig_introspectable_init(
        toggle_set, _rut_icon_toggle_set_prop_specs, toggle_set->properties);

    toggle_set->shell = shell;

    switch (packing) {
    case RUT_ICON_TOGGLE_SET_PACKING_LEFT_TO_RIGHT:
        box_packing = RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT;
        break;
    case RUT_ICON_TOGGLE_SET_PACKING_RIGHT_TO_LEFT:
        box_packing = RUT_BOX_LAYOUT_PACKING_RIGHT_TO_LEFT;
        break;
    case RUT_ICON_TOGGLE_SET_PACKING_TOP_TO_BOTTOM:
        box_packing = RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM;
        break;
    case RUT_ICON_TOGGLE_SET_PACKING_BOTTOM_TO_TOP:
        box_packing = RUT_BOX_LAYOUT_PACKING_BOTTOM_TO_TOP;
        break;
    }

    toggle_set->layout = rut_box_layout_new(shell, box_packing);
    rut_graphable_add_child(toggle_set, toggle_set->layout);
    rut_object_unref(toggle_set->layout);

    toggle_set->current_toggle_state = NULL;

    return toggle_set;
}

rut_closure_t *
rut_icon_toggle_set_add_on_change_callback(
    rut_icon_toggle_set_t *toggle_set,
    rut_icon_toggle_set_changed_callback_t callback,
    void *user_data,
    rut_closure_destroy_callback_t destroy_cb)
{
    c_return_val_if_fail(callback != NULL, NULL);

    return rut_closure_list_add_FIXME(
        &toggle_set->on_change_cb_list, callback, user_data, destroy_cb);
}

static rut_icon_toggle_set_state_t *
find_state_for_value(rut_icon_toggle_set_t *toggle_set, int value)
{
    rut_icon_toggle_set_state_t *toggle_state;

    c_list_for_each(toggle_state, &toggle_set->toggles_list, list_node)
    {
        if (toggle_state->value == value)
            return toggle_state;
    }
    return NULL;
}

static rut_icon_toggle_set_state_t *
find_state_for_toggle(rut_icon_toggle_set_t *toggle_set,
                      rut_icon_toggle_t *toggle)
{
    rut_icon_toggle_set_state_t *toggle_state;

    c_list_for_each(toggle_state, &toggle_set->toggles_list, list_node)
    {
        if (toggle_state->toggle == toggle)
            return toggle_state;
    }

    return NULL;
}

static void
on_toggle_cb(rut_icon_toggle_t *toggle, bool value, void *user_data)
{
    rut_icon_toggle_set_t *toggle_set = user_data;
    rut_icon_toggle_set_state_t *toggle_state;

    if (value == false)
        return;

    toggle_state = find_state_for_toggle(toggle_set, toggle);
    rut_icon_toggle_set_set_selection(toggle_set, toggle_state->value);
}

void
rut_icon_toggle_set_add(rut_icon_toggle_set_t *toggle_set,
                        rut_icon_toggle_t *toggle,
                        int value)
{
    rut_icon_toggle_set_state_t *toggle_state;

    c_return_if_fail(rut_object_get_type(toggle_set) ==
                     &rut_icon_toggle_set_type);

    c_return_if_fail(find_state_for_toggle(toggle_set, toggle) == NULL);
    c_return_if_fail(find_state_for_value(toggle_set, value) == NULL);

    toggle_state = c_slice_new0(rut_icon_toggle_set_state_t);
    toggle_state->toggle = rut_object_ref(toggle);
    toggle_state->on_toggle_closure = rut_icon_toggle_add_on_toggle_callback(
        toggle, on_toggle_cb, toggle_set, NULL); /* destroy notify */
    toggle_state->value = value;
    c_list_insert(&toggle_set->toggles_list, &toggle_state->list_node);

    rut_box_layout_add(toggle_set->layout, false, toggle);
}

void
rut_icon_toggle_set_remove(rut_icon_toggle_set_t *toggle_set,
                           rut_icon_toggle_t *toggle)
{
    rut_icon_toggle_set_state_t *toggle_state;

    c_return_if_fail(rut_object_get_type(toggle_set) ==
                     &rut_icon_toggle_set_type);

    toggle_state = find_state_for_toggle(toggle_set, toggle);

    c_return_if_fail(toggle_state);

    if (toggle_set->current_toggle_state == toggle_state)
        toggle_set->current_toggle_state = NULL;

    remove_toggle_state(toggle_state);

    rut_box_layout_remove(toggle_set->layout, toggle);
}

int
rut_icon_toggle_set_get_selection(rut_object_t *object)
{
    rut_icon_toggle_set_t *toggle_set = object;

    return toggle_set->current_toggle_state
           ? toggle_set->current_toggle_state->value
           : -1;
}

void
rut_icon_toggle_set_set_selection(rut_object_t *object,
                                  int selection_value)
{
    rut_icon_toggle_set_t *toggle_set = object;
    rut_icon_toggle_set_state_t *toggle_state;

    if (toggle_set->current_toggle_state &&
        toggle_set->current_toggle_state->value == selection_value)
        return;

    if (selection_value > 0) {
        toggle_state = find_state_for_value(toggle_set, selection_value);
        c_return_if_fail(toggle_state);
    } else {
        toggle_state = NULL;
        selection_value = -1;
    }

    if (toggle_set->current_toggle_state) {
        rut_icon_toggle_set_state(toggle_set->current_toggle_state->toggle,
                                  false);
    }

    toggle_set->current_toggle_state = toggle_state;
    rut_icon_toggle_set_state(toggle_set->current_toggle_state->toggle, true);

    rig_property_dirty(
        &toggle_set->shell->property_ctx,
        &toggle_set->properties[RUT_ICON_TOGGLE_SET_PROP_SELECTION]);

    rut_closure_list_invoke(&toggle_set->on_change_cb_list,
                            rut_icon_toggle_set_changed_callback_t,
                            toggle_set,
                            selection_value);
}
