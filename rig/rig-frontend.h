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

#ifndef _RIG_FRONTEND_H_
#define _RIG_FRONTEND_H_

#include <rut.h>

typedef struct _rig_frontend_t rig_frontend_t;

typedef enum _rig_frontend_id_t {
    RIG_FRONTEND_ID_EDITOR = 1,
    RIG_FRONTEND_ID_SLAVE,
    RIG_FRONTEND_ID_DEVICE
} rig_frontend_id_t;

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
struct _rig_frontend_t {
    rut_object_base_t _base;

    rig_frontend_id_t id;

    rig_engine_t *engine;

    pid_t simulator_pid;

#ifdef linux
    int listen_fd;
#endif
    int fd;
    rig_rpc_peer_t *frontend_peer;
    bool connected;

    bool has_resized;
    int pending_width;
    int pending_height;

    bool pending_play_mode_enabled;

    rut_closure_t *simulator_queue_frame_closure;

    bool ui_update_pending;

    rut_list_t ui_update_cb_list;

    void (*simulator_connected_callback)(void *user_data);
    void *simulator_connected_data;

    rig_engine_op_map_context_t map_op_ctx;
    rig_engine_op_apply_context_t apply_op_ctx;
    rig_pb_un_serializer_t *prop_change_unserializer;

    uint8_t *pending_dso_data;
    size_t pending_dso_len;

    c_hash_table_t *tmp_id_to_object_map;
};

rig_frontend_t *
rig_frontend_new(rut_shell_t *shell, rig_frontend_id_t id, bool play_mode);

void rig_frontend_post_init_engine(rig_frontend_t *frontend,
                                   const char *ui_filename);

void rig_frontend_forward_simulator_ui(rig_frontend_t *frontend,
                                       const Rig__UI *pb_ui,
                                       bool play_mode);

void rig_frontend_reload_simulator_ui(rig_frontend_t *frontend,
                                      rig_ui_t *ui,
                                      bool play_mode);

/* TODO: should support a destroy_notify callback */
void rig_frontend_sync(rig_frontend_t *frontend,
                       void (*synchronized)(const Rig__SyncAck *result,
                                            void *user_data),
                       void *user_data);

void rig_frontend_run_simulator_frame(rig_frontend_t *frontend,
                                      rig_pb_serializer_t *serializer,
                                      Rig__FrameSetup *setup);

void
rig_frontend_set_simulator_connected_callback(rig_frontend_t *frontend,
                                              void (*callback)(void *user_data),
                                              void *user_data);

typedef void (*rig_frontend_ui_update_callback_t)(rig_frontend_t *frontend,
                                                  void *user_data);

rut_closure_t *
rig_frontend_add_ui_update_callback(rig_frontend_t *frontend,
                                    rig_frontend_ui_update_callback_t callback,
                                    void *user_data,
                                    rut_closure_destroy_callback_t destroy);

void rig_frontend_queue_set_play_mode_enabled(rig_frontend_t *frontend,
                                              bool play_mode_enabled);

/* Similar to rut_shell_queue_redraw() but for queuing a new simulator
 * frame. If the simulator is currently busy this waits until we
 * recieve an update from the simulator and then queues a redraw. */
void rig_frontend_queue_simulator_frame(rig_frontend_t *frontend);

void rig_frontend_update_simulator_dso(rig_frontend_t *frontend,
                                       uint8_t *dso,
                                       int len);

#endif /* _RIG_FRONTEND_H_ */
