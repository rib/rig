/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012  Intel Corporation
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

#ifndef _RIG_CONTROLLER_VIEW_H_
#define _RIG_CONTROLLER_VIEW_H_

#include <rut.h>

typedef struct _rig_controller_view_t rig_controller_view_t;

#include "rig-controller.h"
#include "rig-undo-journal.h"

extern rut_type_t rig_controller_view_type;

rig_controller_view_t *rig_controller_view_new(rig_editor_t *editor,
                                               rig_undo_journal_t *undo_journal);

void rig_controller_view_update_controller_list(rig_controller_view_t *view);

typedef void (*rig_controller_view_controller_changed_callback_t)(
    rig_controller_view_t *view, rig_controller_t *controller, void *user_data);

rut_closure_t *rig_controller_view_add_controller_changed_callback(
    rig_controller_view_t *view,
    rig_controller_view_controller_changed_callback_t callback,
    void *user_data,
    rut_closure_destroy_callback_t destroy_cb);

rig_controller_t *
rig_controller_view_get_controller(rig_controller_view_t *view);

void rig_controller_view_set_controller(rig_controller_view_t *view,
                                        rig_controller_t *controller);

double rig_controller_view_get_focus(rig_controller_view_t *view);

void rig_controller_view_edit_property(rig_controller_view_t *view,
                                       bool mergable,
                                       rut_property_t *property,
                                       rut_boxed_t *boxed_value);

#endif /* _RIG_CONTROLLER_VIEW_H_ */
