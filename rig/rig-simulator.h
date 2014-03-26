/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013  Intel Corporation
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
  RigEngineOpMapContext map_to_sim_objects_op_ctx;
  RigEngineOpMapContext map_to_frontend_ids_op_ctx;

  GHashTable *object_to_id_map;
  GHashTable *id_to_object_map;
  //GHashTable *object_to_tmp_id_map;
  uint64_t next_tmp_id;

  RutList actions;
  int n_actions;

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
rig_simulator_action_select_object (RigSimulator *simulator,
                                    RutObject *object,
                                    RutSelectAction action);

#endif /* _RIG_SIMULATOR_H_ */
