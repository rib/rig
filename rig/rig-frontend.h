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

#ifndef _RIG_FRONTEND_H_
#define _RIG_FRONTEND_H_

#include <rut.h>

typedef struct _RigFrontend RigFrontend;

typedef enum _RigFrontendID
{
  RIG_FRONTEND_ID_EDITOR=1,
  RIG_FRONTEND_ID_SLAVE,
  RIG_FRONTEND_ID_DEVICE
} RigFrontendID;


#include "rig-frontend.h"
#include "rig-engine.h"
#include "rig-simulator.h"
#include "rig-pb.h"
#include "rig-engine-op.h"

#include "rig.pb-c.h"

/* The "frontend" is the main process that controls the running
 * of a Rig UI, either in device mode, as a slave or as an editor.
 *
 * Currently this is also the process where all rendering is handled,
 * though in the future more things may be split out into different
 * threads and processes.
 */
struct _RigFrontend
{
  RutObjectBase _base;

  RigFrontendID id;

  RigEngine *engine;

  pid_t simulator_pid;

  int fd;
  RigRPCPeer *frontend_peer;
  bool connected;

  bool has_resized;
  int pending_width;
  int pending_height;

  bool pending_play_mode_enabled;

  RutClosure *simulator_queue_frame_closure;

  bool ui_update_pending;

  RutList ui_update_cb_list;

  void (*simulator_connected_callback) (void *user_data);
  void *simulator_connected_data;

  RigEngineOpApplyContext apply_op_ctx;
  RigPBUnSerializer *prop_change_unserializer;

  uint8_t *pending_dso_data;
  size_t pending_dso_len;

  GHashTable *tmp_id_to_object_map;
};


RigFrontend *
rig_frontend_new (RutShell *shell,
                  RigFrontendID id,
                  const char *ui_filename,
                  bool play_mode);

void
rig_frontend_forward_simulator_ui (RigFrontend *frontend,
                                   const Rig__UI *pb_ui,
                                   bool play_mode);

void
rig_frontend_reload_simulator_ui (RigFrontend *frontend,
                                  RigUI *ui,
                                  bool play_mode);

/* TODO: should support a destroy_notify callback */
void
rig_frontend_sync (RigFrontend *frontend,
                   void (*synchronized) (const Rig__SyncAck *result,
                                         void *user_data),
                   void *user_data);

void
rig_frontend_run_simulator_frame (RigFrontend *frontend,
                                  RigPBSerializer *serializer,
                                  Rig__FrameSetup *setup);

void
rig_frontend_start_service (RigFrontend *frontend);

void
rig_frontend_stop_service (RigFrontend *frontend);

void
rig_frontend_set_simulator_connected_callback (RigFrontend *frontend,
                                               void (*callback) (void *user_data),
                                               void *user_data);

typedef void (*RigFrontendUIUpdateCallback) (RigFrontend *frontend,
                                             void *user_data);

RutClosure *
rig_frontend_add_ui_update_callback (RigFrontend *frontend,
                                     RigFrontendUIUpdateCallback callback,
                                     void *user_data,
                                     RutClosureDestroyCallback destroy);

void
rig_frontend_queue_set_play_mode_enabled (RigFrontend *frontend,
                                          bool play_mode_enabled);

/* Similar to rut_shell_queue_redraw() but for queuing a new simulator
 * frame. If the simulator is currently busy this waits until we
 * recieve an update from the simulator and then queues a redraw. */
void
rig_frontend_queue_simulator_frame (RigFrontend *frontend);

void
rig_frontend_update_simulator_dso (RigFrontend *frontend,
                                   uint8_t *dso,
                                   int len);

#endif /* _RIG_FRONTEND_H_ */
