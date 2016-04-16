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

#pragma once

#include <rut.h>

typedef struct _rig_frontend_t rig_frontend_t;

#include "rig-frontend.h"
#include "rig-engine.h"
#include "rig-simulator.h"
#include "rig-pb.h"
#include "rig-engine-op.h"
#include "rig-camera-view.h"
#include "rig-rpc-network.h"
#include "protobuf-c-rpc/rig-protobuf-c-stream.h"

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

    rig_engine_t *engine;

    pid_t simulator_pid;

    /* When listening for an out of process simulator
     * to connect (for debugging)...
     */
#ifdef __linux__
    int listen_fd;
#endif
#ifdef USE_UV
    uv_tcp_t listening_socket;
    char *listening_address;
    int listening_port;
#endif
#ifdef __EMSCRIPTEN__
    worker_handle sim_worker;
    int sim_fd;
#endif

    rig_pb_stream_t *stream;
    rig_rpc_peer_t *frontend_peer;
    bool connected;

    cg_pipeline_t *default_pipeline;
    cg_texture_2d_t *default_tex2d;
    cg_pipeline_t *default_tex2d_pipeline;

    rut_object_t *renderer;

    cg_attribute_t *circle_node_attribute;
    int circle_node_n_verts;

    c_hash_table_t *source_wrappers;

    /* FIXME: HACK */
    void (*swap_buffers_hook)(cg_framebuffer_t *fb, void *data);
    void *swap_buffers_hook_data;

    rut_closure_t *simulator_queue_frame_closure;

    rut_closure_t *finish_ui_load_closure;

    /* Painting is split into two parts, where we first calculate a
     * target presentation time and kick the simulator to prepare
     * an update for the frame, and then later we 'finish' by
     * painting the update from the simulator (if it arrive in
     * time). */
    rut_poll_timer_t *paint_finish_timer;

    /* true if the simulator is busy preparing a scene update */
    bool sim_update_pending;

    bool mid_frame;

    int64_t est_frame_delta_ns;

    int64_t prev_target_time; /* the predicted presentation time used for the
                                 previous frame; updated after painting */
    int64_t target_time; /* predicted time for current frame based on
                            presentation times from GL */

    int64_t prev_sim_target_time; /* the predicted presentation time for the
                                     previous simulated frame. */
    int64_t sim_target_time; /* predicted presentation time for frame
                                currently being simulated */

    c_list_t ui_update_cb_list;

    bool dirty_view_geometry;

    void (*simulator_connected_callback)(void *user_data);
    void *simulator_connected_data;

    rig_engine_op_map_context_t map_to_frontend_objects_op_ctx;
    rig_engine_op_apply_context_t apply_op_ctx;
    rig_pb_unserializer_t *prop_change_unserializer;

    rig_pb_unserializer_t *ui_unserializer;

    c_hash_table_t *id_to_object_map;
    c_hash_table_t *object_to_id_map;

    void (*delete_object)(rig_frontend_t *frontend, void *object);
};

rig_frontend_t *rig_frontend_new(rut_shell_t *shell);

typedef void (*rig_local_sim_init_func_t)(rig_simulator_t *simulator,
                                          void *user_data);

/* Note: the local_sim_init_cb and local_sim_init_data arguments
 * only make sense for a simulator that will run in the same process
 * as the frontend (either via mainloop or another thread).
 *
 * local_sim_init_cb is convenient for minimal test rigs where we want
 * to easily write native code that runs in the simulator.
 */
void rig_frontend_spawn_simulator(rig_frontend_t *frontend,
                                  enum rig_simulator_run_mode mode,
                                  const char *address,
                                  int port,
                                  rig_local_sim_init_func_t local_sim_init_cb,
                                  void *local_sim_init_data,
                                  const char *ui_filename);

void rig_frontend_delete_object(rig_frontend_t *frontend, void *object);

uint64_t rig_frontend_lookup_id(rig_frontend_t *frontend, void *object);

void rig_frontend_post_init_engine(rig_frontend_t *frontend,
                                   const char *ui_filename);

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

void rig_frontend_paint(rig_frontend_t *frontend);

void rig_frontend_start_frame(rig_frontend_t *frontend);
