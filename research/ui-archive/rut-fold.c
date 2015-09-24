/*
 * Rut
 *
 * Rig Utilities
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

#include "rut-fixed.h"
#include "rut-stack.h"
#include "rut-bin.h"
#include "rut-fold.h"
#include "rut-composite-sizable.h"
#include "rut-input-region.h"
#include "rut-texture-cache.h"

static rut_property_spec_t _rut_fold_prop_specs[] = {
    { .name = "label",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_TEXT,
      .setter.text_type = rut_fold_set_label,
      .getter.text_type = rut_fold_get_label },
    { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rut_fold_free(void *object)
{
    rut_fold_t *fold = object;

    rut_fold_set_child(fold, NULL);

    rut_object_unref(fold->fold_up_icon);
    rut_object_unref(fold->fold_down_icon);

    rut_graphable_destroy(fold);
    rut_introspectable_destroy(fold);

    rut_object_free(rut_fold_t, fold);
}

rut_type_t rut_fold_type;

static void
_rut_fold_init_type(void)
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

    rut_type_t *type = &rut_fold_type;
#define TYPE rut_fold_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rut_fold_free);
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
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INTROSPECTABLE,
                       offsetof(TYPE, introspectable),
                       NULL); /* no implied vtable */

#undef TYPE
}

static rut_input_event_status_t
input_cb(rut_input_region_t *region, rut_input_event_t *event, void *user_data)
{
    rut_fold_t *fold = user_data;

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION &&
        rut_motion_event_get_action(event) == RUT_MOTION_EVENT_ACTION_UP) {
        rut_fold_set_folded(fold, !fold->folded);
        return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

rut_fold_t *
rut_fold_new(rut_shell_t *shell, const char *label)
{
    rut_fold_t *fold =
        rut_object_alloc0(rut_fold_t, &rut_fold_type, _rut_fold_init_type);
    rut_box_layout_t *header_hbox;
    rut_stack_t *left_header_stack;
    rut_box_layout_t *left_header_hbox;
    rut_bin_t *label_bin;
    rut_bin_t *fold_icon_align;
    cg_texture_t *texture;
    cg_color_t black;

    fold->shell = shell;

    rut_graphable_init(fold);

    rut_introspectable_init(fold, _rut_fold_prop_specs, fold->properties);

    fold->vbox = rut_box_layout_new(shell,
                                    RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM);

    header_hbox = rut_box_layout_new(shell,
                                     RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);
    rut_box_layout_add(fold->vbox, false, header_hbox);
    rut_object_unref(header_hbox);

    left_header_stack = rut_stack_new(shell, 0, 0);
    rut_box_layout_add(header_hbox, true, left_header_stack);
    rut_object_unref(left_header_stack);

    left_header_hbox =
        rut_box_layout_new(shell, RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);
    rut_stack_add(left_header_stack, left_header_hbox);
    rut_object_unref(left_header_hbox);

    fold_icon_align = rut_bin_new(shell);
    rut_bin_set_x_position(fold_icon_align, RUT_BIN_POSITION_BEGIN);
    rut_bin_set_y_position(fold_icon_align, RUT_BIN_POSITION_CENTER);
    rut_bin_set_right_padding(fold_icon_align, 10);
    rut_box_layout_add(left_header_hbox, false, fold_icon_align);
    rut_object_unref(fold_icon_align);

    texture = rut_load_texture_from_data_file(shell, "tri-fold-up.png", NULL);
    fold->fold_up_icon = rut_nine_slice_new(shell,
                                            texture,
                                            0,
                                            0,
                                            0,
                                            0,
                                            cg_texture_get_width(texture),
                                            cg_texture_get_height(texture));
    cg_object_unref(texture);

    texture = rut_load_texture_from_data_file(shell, "tri-fold-down.png",
                                              NULL);
    fold->fold_down_icon = rut_nine_slice_new(shell,
                                              texture,
                                              0,
                                              0,
                                              0,
                                              0,
                                              cg_texture_get_width(texture),
                                              cg_texture_get_height(texture));
    cg_object_unref(texture);

    fold->fold_icon_shim = rut_fixed_new(shell, cg_texture_get_width(texture),
                                         cg_texture_get_height(texture));
    rut_bin_set_child(fold_icon_align, fold->fold_icon_shim);
    rut_object_unref(fold->fold_icon_shim);

    rut_graphable_add_child(fold->fold_icon_shim, fold->fold_down_icon);

    /* NB: we keep references to the icons so they can be swapped
     * without getting disposed. */

    label_bin = rut_bin_new(shell);
    rut_bin_set_y_position(label_bin, RUT_BIN_POSITION_CENTER);
    rut_box_layout_add(left_header_hbox, false, label_bin);
    rut_object_unref(label_bin);

    fold->label = rut_text_new_with_text(shell, NULL, label);
    rut_bin_set_child(label_bin, fold->label);
    rut_object_unref(fold->label);

    fold->header_hbox_right =
        rut_box_layout_new(shell, RUT_BOX_LAYOUT_PACKING_RIGHT_TO_LEFT);
    rut_box_layout_add(header_hbox, true, fold->header_hbox_right);
    rut_object_unref(fold->header_hbox_right);

    cg_color_init_from_4f(&black, 0, 0, 0, 1);
    rut_fold_set_folder_color(fold, &black);
    rut_fold_set_label_color(fold, &black);

    rut_graphable_add_child(fold, fold->vbox);
    rut_object_unref(fold->vbox);

    fold->input_region =
        rut_input_region_new_rectangle(0, 0, 0, 0, input_cb, fold);
    rut_stack_add(left_header_stack, fold->input_region);
    rut_object_unref(fold->input_region);

    fold->folded = false;

    return fold;
}

void
rut_fold_set_child(rut_fold_t *fold, rut_object_t *child)
{
    c_return_if_fail(rut_object_get_type(fold) == &rut_fold_type);

    if (child)
        rut_object_claim(child, fold);

    if (fold->child) {
        if (!fold->folded)
            rut_box_layout_remove(fold->vbox, fold->child);
        rut_object_release(fold->child, fold);
    }

    fold->child = child;
    if (child && !fold->folded)
        rut_box_layout_add(fold->vbox, true, child);
}

void
rut_fold_set_header_child(rut_fold_t *fold, rut_object_t *child)
{
    c_return_if_fail(rut_object_get_type(fold) == &rut_fold_type);

    if (child)
        rut_object_claim(child, fold);

    if (fold->header_child) {
        rut_box_layout_remove(fold->header_hbox_right, fold->header_child);
        rut_object_release(fold->header_child, fold);
    }

    fold->header_child = child;
    rut_box_layout_add(fold->header_hbox_right, true, child);
}

void
rut_fold_set_folded(rut_fold_t *fold, bool folded)
{
    if (fold->folded == folded || fold->child == NULL)
        return;

    if (folded) {
        rut_fixed_remove_child(fold->fold_icon_shim, fold->fold_down_icon);
        rut_fixed_add_child(fold->fold_icon_shim, fold->fold_up_icon);
        rut_box_layout_remove(fold->vbox, fold->child);
    } else {
        rut_fixed_remove_child(fold->fold_icon_shim, fold->fold_up_icon);
        rut_fixed_add_child(fold->fold_icon_shim, fold->fold_down_icon);
        rut_box_layout_add(fold->vbox, true, fold->child);
    }

    fold->folded = folded;

    rut_shell_queue_redraw(fold->shell);
}

void
rut_fold_set_folder_color(rut_fold_t *fold, const cg_color_t *color)
{
    cg_pipeline_t *pipeline;

    pipeline = rut_nine_slice_get_pipeline(fold->fold_up_icon);
    cg_pipeline_set_color(pipeline, color);
    pipeline = rut_nine_slice_get_pipeline(fold->fold_down_icon);
    cg_pipeline_set_color(pipeline, color);
}

void
rut_fold_set_label_color(rut_fold_t *fold, const cg_color_t *color)
{
    rut_text_set_color(fold->label, color);
}

void
rut_fold_set_font_name(rut_fold_t *fold, const char *font)
{
    rut_text_set_font_name(fold->label, font);
}

void
rut_fold_set_label(rut_object_t *object, const char *label)
{
    rut_fold_t *fold = object;

    rut_text_set_text(fold->label, label);

    rut_property_dirty(&fold->shell->property_ctx,
                       &fold->properties[RUT_FOLD_PROP_LABEL]);
    rut_shell_queue_redraw(fold->shell);
}

const char *
rut_fold_get_label(rut_object_t *object)
{
    rut_fold_t *fold = object;
    return rut_text_get_text(fold->label);
}
