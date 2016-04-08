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
typedef struct _rig_simulator_t rig_simulator_t;

#if !defined(__ANDROID__) && (defined(__linux__) || defined(__APPLE__))
#define RIG_SUPPORT_SIMULATOR_PROCESS
#endif

enum rig_simulator_run_mode {
    RIG_SIMULATOR_RUN_MODE_MAINLOOP,
#ifdef C_SUPPORTS_THREADS
    RIG_SIMULATOR_RUN_MODE_THREADED,
#endif
#ifdef RIG_SUPPORT_SIMULATOR_PROCESS
    RIG_SIMULATOR_RUN_MODE_PROCESS,
#endif
#ifdef __linux__
    RIG_SIMULATOR_RUN_MODE_LISTEN_ABSTRACT_SOCKET,
    RIG_SIMULATOR_RUN_MODE_CONNECT_ABSTRACT_SOCKET,
#endif
#ifdef USE_UV
    RIG_SIMULATOR_RUN_MODE_LISTEN_TCP,
    RIG_SIMULATOR_RUN_MODE_CONNECT_TCP,
#endif
#ifdef __EMSCRIPTEN__
    RIG_SIMULATOR_RUN_MODE_WEB_WORKER,
    RIG_SIMULATOR_RUN_MODE_WEB_SOCKET,
#endif
};

#include "rig-engine.h"
#include "rig-engine-op.h"
#include "protobuf-c-rpc/rig-protobuf-c-stream.h"
#include "rig-rpc-network.h"
#include "rig-pb.h"
#include "rig-frontend.h"
#include "rig-js.h"

/*
 * Simulator actions are sent back as requests to the frontend at the
 * end of a frame.
 */
typedef enum _rig_simulator_action_type_t {
    RIG_SIMULATOR_ACTION_TYPE_REPORT_EDIT_FAILURE = 1,
} rig_simulator_action_type_t;

/* The "simulator" is the process responsible for updating object
 * properties either in response to user input, the progression of
 * animations or running other forms of simulation such as physics.
 */
struct _rig_simulator_t {
    rut_object_base_t _base;

    bool in_frame;
    bool redraw_queued;

    struct {
        double progress;
    } frame_info;

    rut_shell_t *shell;
    rig_engine_t *engine;

#ifdef __linux__
    int listen_fd;
#endif
#ifdef USE_UV
    uv_tcp_t listening_socket;
    char *listening_address;
    int listening_port;
#endif

    struct {
        /* The frontend only needs asset paths and will be responsible
         * for loading those assets (E.g. browser based frontend) */
        unsigned image_loader : 1;
    } frontend_features;

    rig_pb_stream_t *stream;
    rig_rpc_peer_t *simulator_peer;

    rig_pb_serializer_t *log_serializer;
    rut_memory_stack_t *log_serializer_stack;

    float view_x;
    float view_y;

    float last_pointer_x;
    float last_pointer_y;

    rut_button_state_t button_state;

    rig_pb_un_serializer_t *ops_unserializer;
    rig_engine_op_apply_context_t apply_op_ctx;
    rig_engine_op_map_context_t map_to_sim_objects_op_ctx;
    rig_engine_op_map_context_t map_to_frontend_ids_op_ctx;

    c_hash_table_t *object_registry;

    void *(*lookup_object_cb)(uint64_t id, void *user_data);
    uint64_t (*lookup_object_id)(rig_simulator_t *simulator, void *object);
    void (*register_object_cb)(void *object, uint64_t id, void *user_data);
    void (*unregister_object_cb)(void *object, void *user_data);
    void *(*unregister_id)(rig_simulator_t *simulator, uint64_t id);

    c_list_t actions;
    int n_actions;

    rut_queue_t *ops;

    bool connected;
    c_list_t connected_closures;

    rig_js_runtime_t *js;
};

extern rut_type_t rig_simulator_type;

rig_simulator_t *rig_simulator_new(rut_shell_t *main_shell);

void rig_simulator_set_frontend_fd(rig_simulator_t *simulator, int fd);

void rig_simulator_run(rig_simulator_t *simulator);

void rig_simulator_run_frame(rut_shell_t *shell, void *user_data);

void rig_simulator_queue_redraw_hook(rut_shell_t *shell, void *user_data);

void rig_simulator_print_mappings(rig_simulator_t *simulator);

void rig_simulator_forward_log(rig_simulator_t *simulator);

enum rig_simulator_run_flags {
    RIG_SIMULATOR_LISTEN       = 1<<0, /* implies standalone,
                                          disallows connect modes,
                                          ommision disallows listen modes */
    RIG_SIMULATOR_STANDALONE   = 1<<1  /* disallows thread/mainloop/prcess modes */
};

bool rig_simulator_parse_run_mode(const char *option,
                                  void (*usage)(void),
                                  enum rig_simulator_run_flags flags,
                                  enum rig_simulator_run_mode *mode,
                                  char **address,
                                  int *port);

typedef void (*rig_simulator_connected_func_t)(rig_simulator_t *simulator,
                                               void *user_data);
void rig_simulator_add_connected_callback(rig_simulator_t *simulator,
                                          rut_closure_t *closure);

void rig_simulator_queue_ui_load_on_connect(rig_simulator_t *simulator,
                                            const char *ui_filename);

void rig_simulator_forward_frontend_ui(rig_simulator_t *simulator,
                                       const Rig__UI *pb_ui);

void rig_simulator_reload_frontend_ui(rig_simulator_t *simulator,
                                      rig_ui_t *ui);

void rig_simulator_load_empty_ui(rig_simulator_t *simulator);
void rig_simulator_load_file(rig_simulator_t *simulator, const char *filename);
