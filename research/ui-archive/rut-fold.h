/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013  Intel Corporation
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
 */

#ifndef _RUT_FOLD_H_
#define _RUT_FOLD_H_

#include "rut-type.h"
#include "rut-box-layout.h"
#include "rut-text.h"

#include "rut-nine-slice.h"

extern rut_type_t rut_fold_type;

enum {
    RUT_FOLD_PROP_LABEL,
    RUT_FOLD_N_PROPS
};

typedef struct _rut_fold_t {
    rut_object_base_t _base;

    rut_shell_t *shell;

    rut_box_layout_t *vbox;
    rut_box_layout_t *header_hbox_right;

    rut_text_t *label;
    rut_fixed_t *fold_icon_shim;
    rut_nine_slice_t *fold_up_icon;
    rut_nine_slice_t *fold_down_icon;

    rut_input_region_t *input_region;

    bool folded;

    rut_object_t *child;
    rut_object_t *header_child;

    rut_graphable_props_t graphable;

    rut_introspectable_props_t introspectable;
    rut_property_t properties[RUT_FOLD_N_PROPS];

} rut_fold_t;

rut_fold_t *rut_fold_new(rut_shell_t *shell, const char *label);

void rut_fold_set_child(rut_fold_t *fold, rut_object_t *child);

void rut_fold_set_header_child(rut_fold_t *fold, rut_object_t *child);

void rut_fold_set_folded(rut_fold_t *fold, bool folded);

void rut_fold_set_folder_color(rut_fold_t *fold, const cg_color_t *color);

void rut_fold_set_label_color(rut_fold_t *fold, const cg_color_t *color);

void rut_fold_set_label(rut_object_t *object, const char *label);

const char *rut_fold_get_label(rut_object_t *object);

void rut_fold_set_font_name(rut_fold_t *fold, const char *font);

#endif /* _RUT_FOLD_H_ */
