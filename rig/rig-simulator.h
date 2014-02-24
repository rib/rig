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
#include "rig-engine-op.h"
#include "rig-pb.h"

/*
 * Simulator actions are sent back as requests to the frontend at the
 * end of a frame.
 */
typedef enum _RigSimulatorActionType
{
  RIG_SIMULATOR_ACTION_TYPE_SELECT_OBJECT = 1,
  RIG_SIMULATOR_ACTION_TYPE_REPORT_EDIT_FAILURE,
} RigSimulatorActionType;

/* The "simulator" is the process responsible for updating object
 * properties either in response to user input, the progression of
 * animations or running other forms of simulation such as physics.
 */
struct _RigSimulator
{
  RutObjectBase _base;

  RigFrontendID frontend_id;

  bool redraw_queued;

  /* when running as an editor or slave device then the UI
   * can be edited at runtime and we handle some things a
   * bit differently. For example we only need to be able
   * to map ids to objects to support editing operations.
   */
  bool editable;

  RutShell *shell;
  RutContext *ctx;
  RigEngine *engine;

  int fd;
  RigRPCPeer *simulator_peer;

  float view_x;
  float view_y;

  float last_pointer_x;
  float last_pointer_y;

  RutButtonState button_state;

  RigPBUnSerializer *ui_unserializer;
  RigPBUnSerializer *ops_unserializer;
  RigEngineOpApplyContext apply_op_ctx;

  GHashTable *object_to_id_map;
  GHashTable *id_to_object_map;
  //GHashTable *object_to_tmp_id_map;
  uint64_t next_tmp_id;

  RutList actions;
  int n_actions;

  RutQueue *queued_deletes;

  RutQueue *ops;
};

extern RutType rig_simulator_type;

RigSimulator *
rig_simulator_new (RigFrontendID frontend_id,
                   int fd);

void
rig_simulator_run (RigSimulator *simulator);

void
rig_simulator_run_frame (RutShell *shell, void *user_data);

void
rig_simulator_queue_redraw_hook (RutShell *shell, void *user_data);

void
rig_simulator_stop_service (RigSimulator *simulator);

void
rig_simulator_stop_service (RigSimulator *simulator);

void
rig_simulator_action_select_object (RigSimulator *simulator,
                                    RutObject *object,
                                    RutSelectAction action);

#endif /* _RIG_SIMULATOR_H_ */
