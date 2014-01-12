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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _RIG_SIMULATOR_H_
#define _RIG_SIMULATOR_H_

#include <rut.h>
typedef struct _RigSimulator RigSimulator;

#include "rig-engine.h"

typedef enum _RigSimulatorMode
{
  RIG_SIMULATOR_MODE_EDITOR=1,
  RIG_SIMULATOR_MODE_DEVICE
} RigSimulatorMode;

/* The "simulator" is the process responsible for updating object
 * properties either in response to user input, the progression of
 * animations or running other forms of simulation such as physics.
 */
struct _RigSimulator
{
  RigSimulatorMode mode;

  RutShell *shell;
  RutContext *ctx;
  RigEngine *engine;

  int fd;
  RigRPCPeer *simulator_peer;

  float last_pointer_x;
  float last_pointer_y;

  RutButtonState button_state;

  GHashTable *id_map;
};


void
rig_simulator_stop_service (RigSimulator *simulator);

void
rig_simulator_stop_service (RigSimulator *simulator);

#endif /* _RIG_SIMULATOR_H_ */
