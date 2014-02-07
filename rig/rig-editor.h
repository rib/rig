/*
 * Rut
 *
 * Copyright (C) 2013  Intel Corporation
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef _RIG_EDITOR_H_
#define _RIG_EDITOR_H_

#include <rut.h>

#include "rig-types.h"

#include "rig.pb-c.h"

typedef struct _RigEditor RigEditor;

void
rig_editor_init (RutShell *shell, void *user_data);

void
rig_editor_fini (RutShell *shell, void *user_data);

void
rig_editor_paint (RutShell *shell, void *user_data);

void
rig_editor_apply_last_op (RigEngine *engine);

void
rig_editor_set_play_mode_enabled (RigEditor *editor, bool enabled);

#endif /* _RIG_EDITOR_H_ */
