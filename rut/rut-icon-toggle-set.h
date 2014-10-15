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

#ifndef __RUT_ICON_TOGGLE_SET_H__
#define __RUT_ICON_TOGGLE_SET_H__

typedef struct _rut_icon_toggle_set_t rut_icon_toggle_set_t;
extern rut_type_t rut_icon_toggle_set_type;

typedef enum _rut_icon_toggle_set_packing_t {
    RUT_ICON_TOGGLE_SET_PACKING_LEFT_TO_RIGHT,
    RUT_ICON_TOGGLE_SET_PACKING_RIGHT_TO_LEFT,
    RUT_ICON_TOGGLE_SET_PACKING_TOP_TO_BOTTOM,
    RUT_ICON_TOGGLE_SET_PACKING_BOTTOM_TO_TOP
} rut_icon_toggle_set_packing_t;

rut_icon_toggle_set_t *
rut_icon_toggle_set_new(rut_shell_t *shell,
                        rut_icon_toggle_set_packing_t packing);

typedef void (*rut_icon_toggle_set_changed_callback_t)(
    rut_icon_toggle_set_t *toggle_set, int selection_value, void *user_data);

rut_closure_t *rut_icon_toggle_set_add_on_change_callback(
    rut_icon_toggle_set_t *toggle_set,
    rut_icon_toggle_set_changed_callback_t callback,
    void *user_data,
    rut_closure_destroy_callback_t destroy_cb);

void rut_icon_toggle_set_add(rut_icon_toggle_set_t *toggle_set,
                             rut_icon_toggle_t *toggle,
                             int selection_value);

int rut_icon_toggle_set_get_selection(rut_object_t *toggle_set);

void rut_icon_toggle_set_set_selection(rut_object_t *toggle_set,
                                       int selection_value);

#endif /* __RUT_ICON_TOGGLE_SET_H__ */
