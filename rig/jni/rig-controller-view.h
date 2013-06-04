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

#ifndef _RIG_TRANSITION_VIEW_H_
#define _RIG_TRANSITION_VIEW_H_

#include <rut.h>
#include "rig-controller.h"
#include "rig-undo-journal.h"

extern RutType rig_controller_view_type;

typedef struct _RigControllerView RigControllerView;

#define RIG_TRANSITION_VIEW(x) ((RigControllerView *) x)

RigControllerView *
rig_controller_view_new (RigEngine *engine,
                         RutObject *graph,
                         RigController *controller,
                         RutTimeline *timeline,
                         RigUndoJournal *undo_journal);

#endif /* _RIG_TRANSITION_VIEW_H_ */
