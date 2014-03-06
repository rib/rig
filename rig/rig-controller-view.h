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

typedef struct _RigControllerView RigControllerView;

#include "rig-controller.h"
#include "rig-undo-journal.h"

extern RutType rig_controller_view_type;

RigControllerView *
rig_controller_view_new (RigEngine *engine,
                         RigUndoJournal *undo_journal);

void
rig_controller_view_update_controller_list (RigControllerView *view);

typedef void (*RigControllerViewControllerChangedCallback)
    (RigControllerView *view,
     RigController *controller,
     void *user_data);

RutClosure *
rig_controller_view_add_controller_changed_callback (
                              RigControllerView *view,
                              RigControllerViewControllerChangedCallback callback,
                              void *user_data,
                              RutClosureDestroyCallback destroy_cb);

RigController *
rig_controller_view_get_controller (RigControllerView *view);

void
rig_controller_view_set_controller (RigControllerView *view,
                                    RigController *controller);

double
rig_controller_view_get_focus (RigControllerView *view);

void
rig_controller_view_edit_property (RigControllerView *view,
                                   bool mergable,
                                   RutProperty *property,
                                   RutBoxed *boxed_value);

#endif /* _RIG_CONTROLLER_VIEW_H_ */
