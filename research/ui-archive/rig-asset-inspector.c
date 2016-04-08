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

#include <clib.h>

#include <rut.h>

#include "rig-asset.h"
#include "rig-asset-inspector.h"

enum {
    RIG_ASSET_INSPECTOR_PROP_ASSET,
    RIG_ASSET_INSPECTOR_N_PROPS
};

struct _rig_asset_inspector_t {
    rut_object_base_t _base;

    rut_shell_t *shell;

    rig_asset_type_t asset_type;
    rig_asset_t *asset;
    rut_image_t *image;
    rut_drag_bin_t *drag_bin;
    rut_shim_t *shim;
    rut_input_region_t *input_region;
    rut_nine_slice_t *highlight;
    rut_stack_t *stack;

    rut_nine_slice_t *drop_preview;
    rut_rectangle_t *drop_preview_overlay;

    rut_graphable_props_t graphable;

    rig_introspectable_props_t introspectable;
    rig_property_t properties[RIG_ASSET_INSPECTOR_N_PROPS];

    unsigned int selected : 1;
};

static rig_property_spec_t _rig_asset_inspector_prop_specs[] = {
    { .name = "asset",
      .nick = "Asset",
      .type = RUT_PROPERTY_TYPE_ASSET,
      .getter.object_type = rig_asset_inspector_get_asset,
      .setter.object_type = rig_asset_inspector_set_asset,
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .animatable = false },
    { NULL }
};

static void
_rig_asset_inspector_set_selected(rig_asset_inspector_t *asset_inspector,
                                  bool selected)
{
    if (asset_inspector->selected == selected)
        return;

    if (selected)
        rut_stack_add(asset_inspector->stack, asset_inspector->highlight);
    else
        rut_graphable_remove_child(asset_inspector->highlight);

    asset_inspector->selected = selected;

    rut_shell_queue_redraw(asset_inspector->shell);
}

static void
_rig_asset_inspector_free(void *object)
{
    rig_asset_inspector_t *asset_inspector = object;

    _rig_asset_inspector_set_selected(asset_inspector, false);
    rut_object_unref(asset_inspector->highlight);
    asset_inspector->highlight = NULL;

    rig_asset_inspector_set_asset(asset_inspector, NULL);

    rut_graphable_destroy(asset_inspector);

    rig_introspectable_destroy(asset_inspector);

    rut_object_free(rig_asset_inspector_t, asset_inspector);
}

static void
_rig_asset_inspector_cancel_selection(rut_object_t *object)
{
    rig_asset_inspector_t *asset_inspector = object;

    rut_graphable_remove_child(asset_inspector->highlight);
    rut_shell_queue_redraw(asset_inspector->shell);
}

static rut_object_t *
_rig_asset_inspector_copy_selection(rut_object_t *object)
{
    rig_asset_inspector_t *asset_inspector = object;

    return rut_object_ref(asset_inspector->asset);
}

static void
_rig_asset_inspector_delete_selection(rut_object_t *object)
{
    rig_asset_inspector_t *asset_inspector = object;

    rig_asset_inspector_set_asset(asset_inspector, NULL);
}

rut_type_t rig_asset_inspector_type;

static void
_rig_asset_inspector_init_type(void)
{
    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child removed */
                                                       NULL, /* child added */
                                                       NULL /* parent changed */
    };
    static rut_sizable_vtable_t sizable_vtable = {
        rut_composite_sizable_set_size,
        rut_composite_sizable_get_size,
        rut_composite_sizable_get_preferred_width,
        rut_composite_sizable_get_preferred_height,
        rut_composite_sizable_add_preferred_size_callback
    };
    static rut_selectable_vtable_t selectable_vtable = {
        .cancel = _rig_asset_inspector_cancel_selection,
        .copy = _rig_asset_inspector_copy_selection,
        .del = _rig_asset_inspector_delete_selection,
    };

    rut_type_t *type = &rig_asset_inspector_type;
#define TYPE rig_asset_inspector_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_asset_inspector_free);
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
                       offsetof(TYPE, shim),
                       NULL); /* no vtable */
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_SELECTABLE,
                       0, /* no implied properties */
                       &selectable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INTROSPECTABLE,
                       offsetof(TYPE, introspectable),
                       NULL); /* no implied vtable */

#undef TYPE
}

static rut_input_event_status_t
input_cb(rut_input_region_t *region, rut_input_event_t *event, void *user_data)
{
    rig_asset_inspector_t *asset_inspector = user_data;

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION &&
        rut_motion_event_get_action(event) == RUT_MOTION_EVENT_ACTION_UP) {
        _rig_asset_inspector_set_selected(asset_inspector, true);
        rut_shell_set_selection(asset_inspector->shell, asset_inspector);
        return RUT_INPUT_EVENT_STATUS_HANDLED;
    } else if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_KEY &&
               (rut_key_event_get_keysym(event) == RUT_KEY_Delete ||
                rut_key_event_get_keysym(event) == RUT_KEY_BackSpace)) {
        rig_asset_inspector_set_asset(asset_inspector, NULL);
    } else if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_DROP) {
        rut_object_t *data = rut_drop_event_get_data(event);

        if (rut_object_get_type(data) == &rig_asset_type &&
            asset_inspector->asset_type == rig_asset_get_type(data)) {
            rig_asset_inspector_set_asset(asset_inspector, data);
            return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    } else if (rut_input_event_get_type(event) ==
               RUT_INPUT_EVENT_TYPE_DROP_OFFER) {
        rut_object_t *payload = rut_drop_offer_event_get_payload(event);

        if (rut_object_get_type(payload) == &rig_asset_type &&
            asset_inspector->asset_type == rig_asset_get_type(payload)) {
            rig_asset_t *asset = payload;
            bool save_selected = asset_inspector->selected;

            _rig_asset_inspector_set_selected(asset_inspector, false);

            asset_inspector->drop_preview =
                rut_nine_slice_new(asset_inspector->shell,
                                   rig_asset_get_thumbnail(asset),
                                   0,
                                   0,
                                   0,
                                   0,
                                   100,
                                   100);
            rut_stack_add(asset_inspector->stack,
                          asset_inspector->drop_preview);
            rut_object_unref(asset_inspector->drop_preview);

            asset_inspector->drop_preview_overlay = rut_rectangle_new4f(
                asset_inspector->shell, 1, 1, 0.5, 0.5, 0.5, 0.5);
            rut_stack_add(asset_inspector->stack,
                          asset_inspector->drop_preview_overlay);
            rut_object_unref(asset_inspector->drop_preview_overlay);

            _rig_asset_inspector_set_selected(asset_inspector, save_selected);

            rut_shell_onscreen_take_drop_offer(rut_input_event_get_onscreen(event),
                                      asset_inspector->input_region);
        }
    } else if (rut_input_event_get_type(event) ==
               RUT_INPUT_EVENT_TYPE_DROP_CANCEL) {
        c_warn_if_fail(asset_inspector->drop_preview);
        rut_graphable_remove_child(asset_inspector->drop_preview);
        rut_graphable_remove_child(asset_inspector->drop_preview_overlay);
        return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static rut_nine_slice_t *
create_highlight_nine_slice(rut_shell_t *shell)
{
    cg_texture_t *texture =
        rut_load_texture_from_data_file(shell, "highlight.png", NULL);
    int width = cg_texture_get_width(texture);
    int height = cg_texture_get_height(texture);
    rut_nine_slice_t *highlight;
    cg_pipeline_t *pipeline;

    highlight = rut_nine_slice_new(shell, texture, 15, 15, 15, 15, width,
                                   height);
    cg_object_unref(texture);

    pipeline = rut_nine_slice_get_pipeline(highlight);

    cg_pipeline_set_color4f(pipeline, 1, 1, 0, 1);

    return highlight;
}

rig_asset_inspector_t *
rig_asset_inspector_new(rut_shell_t *shell,
                        rig_asset_type_t asset_type)
{
    rig_asset_inspector_t *asset_inspector =
        rut_object_alloc0(rig_asset_inspector_t,
                          &rig_asset_inspector_type,
                          _rig_asset_inspector_init_type);
    rut_shim_t *shim;
    rut_stack_t *stack;

    asset_inspector->shell = shell;

    rig_introspectable_init(asset_inspector,
                            _rig_asset_inspector_prop_specs,
                            asset_inspector->properties);

    rut_graphable_init(asset_inspector);

    asset_inspector->asset_type = asset_type;

    shim = rut_shim_new(asset_inspector->shell, 100, 100);
    rut_graphable_add_child(asset_inspector, shim);
    asset_inspector->shim = shim;
    rut_object_unref(shim);

    stack = rut_stack_new(asset_inspector->shell, 0, 0);
    rut_shim_set_child(shim, stack);
    asset_inspector->stack = stack;
    rut_object_unref(stack);

    asset_inspector->highlight =
        create_highlight_nine_slice(asset_inspector->shell);

    asset_inspector->input_region =
        rut_input_region_new_rectangle(0, 0, 0, 0, input_cb, asset_inspector);
    rut_stack_add(stack, asset_inspector->input_region);
    rut_object_unref(asset_inspector->input_region);

    return asset_inspector;
}

rut_object_t *
rig_asset_inspector_get_asset(rut_object_t *object)
{
    rig_asset_inspector_t *asset_inspector = object;
    return asset_inspector->asset;
}

void
rig_asset_inspector_set_asset(rut_object_t *object,
                              rut_object_t *asset_object)
{
    rig_asset_inspector_t *asset_inspector = object;
    bool save_selected = asset_inspector->selected;
    rig_asset_t *asset = asset_object;
    cg_texture_t *texture;

    if (asset_inspector->asset == asset)
        return;

    _rig_asset_inspector_set_selected(asset_inspector, false);

    if (asset_inspector->asset) {
        rut_object_unref(asset_inspector->asset);
        asset_inspector->asset = NULL;

        if (asset_inspector->image) {
            rut_graphable_remove_child(asset_inspector->image);
            rut_object_unref(asset_inspector->image);
            asset_inspector->image = NULL;
        }
    }

    if (asset_object) {
        asset_inspector->asset = rut_object_ref(asset);

        texture = rig_asset_get_thumbnail(asset);
        if (texture) {
            asset_inspector->image =
                rut_image_new(asset_inspector->shell, texture);
            rut_stack_add(asset_inspector->stack, asset_inspector->image);
        }
    }

    _rig_asset_inspector_set_selected(asset_inspector, save_selected);

    rig_property_dirty(
        &asset_inspector->shell->property_ctx,
        &asset_inspector->properties[RIG_ASSET_INSPECTOR_PROP_ASSET]);
}
