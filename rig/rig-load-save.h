/*
 * Rut
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

#ifndef _RUT_LOAD_SAVE_H_
#define _RUT_LOAD_SAVE_H_

#include "rig-engine.h"
#include "rig-ui.h"

void
rig_save (RigEngine *engine, const char *path);

RigUI *
rig_load (RigEngine *engine, const char *file);

#endif /* _RUT_LOAD_SAVE_H_ */
