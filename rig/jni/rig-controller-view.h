/*
 * Rig
 *
 * Copyright (C) 2012  Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _RIG_CONTROLLER_VIEW_H_
#define _RIG_CONTROLLER_VIEW_H_

#include <rut.h>
#include "rig-controller.h"
#include "rig-undo-journal.h"

extern RutType rig_controller_view_type;

typedef struct _RigControllerView RigControllerView;

#define RIG_CONTROLLER_VIEW(x) ((RigControllerView *) x)

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
