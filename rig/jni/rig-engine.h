/*
 * Rig
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

#ifndef _RIG_ENGINE_H_
#define _RIG_ENGINE_H_

extern CoglBool _rig_in_device_mode;

void
rig_engine_init (RutShell *shell, void *user_data);

RutInputEventStatus
rig_engine_input_handler (RutInputEvent *event, void *user_data);

CoglBool
rig_engine_paint (RutShell *shell, void *user_data);

void
rig_engine_fini (RutShell *shell, void *user_data);

#endif /* _RIG_ENGINE_H_ */
