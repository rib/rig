/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012  Intel Corporation
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

#ifndef _RIG_ENGINE_H_
#define _RIG_ENGINE_H_

#ifdef USE_AVAHI
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-client/lookup.h>
#endif

#include <cmodule.h>
#include <gio/gio.h>

#include <rut.h>

/* Forward declare since there is a circular dependency between this
 * header and rig-camera-view.h which depends on this typedef... */
typedef enum _rig_tool_id_t {
    RIG_TOOL_ID_SELECTION = 1,
    RIG_TOOL_ID_ROTATION,
} rig_tool_id_t;

typedef enum _rut_select_action_t {
    /* replaces the current selection */
    RUT_SELECT_ACTION_REPLACE,
    /* toggles whether the given item is selected or not */
    RUT_SELECT_ACTION_TOGGLE,
} rut_select_action_t;

#include "rig-types.h"

typedef struct _rig_entites_selection_t {
    rut_object_base_t _base;
    rig_engine_t *engine;
    c_list_t *objects;
    rut_list_t selection_events_cb_list;
} rig_objects_selection_t;

#include "rig-editor.h"
#include "rig-image-source.h"

#include "rig-protobuf-c-rpc.h"
#include "rig-rpc-network.h"

#include "rig-controller.h"
#include "rig-controller-view.h"
#include "rig-undo-journal.h"
#include "rut-box-layout.h"
#include "rig-osx.h"
#include "rig-split-view.h"
#include "rig-camera-view.h"
#include "rig-frontend.h"
#include "rig-simulator.h"
#include "rig-code.h"
#include "rig-image-source.h"

enum {
    RIG_ENGINE_PROP_WIDTH,
    RIG_ENGINE_PROP_HEIGHT,
    RIG_ENGINE_PROP_DEVICE_WIDTH,
    RIG_ENGINE_PROP_DEVICE_HEIGHT,
    RIG_ENGINE_N_PROPS
};

extern rut_type_t rig_engine_type;

struct _rig_engine_t {
    rut_object_base_t _base;

    rig_frontend_id_t frontend_id;

    bool headless;
    bool play_mode;

    char *ui_filename;
    rut_closure_t *finish_ui_load_closure;

    rut_object_t *camera_2d;
    rut_object_t *root;

    cg_matrix_t identity;

    /* TODO: Move to rig_frontend_t */
    cg_texture_t *gradient;

    /* TODO: Move to rig_frontend_t */
    c_hash_table_t *image_source_wrappers;

    /* TODO: Move to rig_frontend_t */
    cg_pipeline_t *shadow_color_tex;
    cg_pipeline_t *shadow_map_tex;

    cg_pipeline_t *default_pipeline;

    /* TODO: Move to rig_frontend_t */
    cg_pipeline_t *dof_pipeline_template;
    cg_pipeline_t *dof_pipeline;
    cg_pipeline_t *dof_diamond_pipeline;
    cg_pipeline_t *dof_unshaped_pipeline;

    rut_shell_t *shell;
    rut_context_t *ctx;

    /* TODO: Move to rig_frontend_t */
    cg_onscreen_t *onscreen;

    rig_pb_serializer_t *ops_serializer;
    rut_memory_stack_t *frame_stack;
    rut_magazine_t *object_id_magazine;

    /* TODO: Move to rig_frontend_t */
    rut_memory_stack_t *sim_frame_stack;

    rut_input_queue_t *simulator_input_queue;

/* XXX: Move to rig_editor_t */
#ifdef RIG_EDITOR_ENABLED
    rut_text_t *search_text;
    c_list_t *required_search_tags;

    rut_list_t tool_changed_cb_list;

    c_string_t *codegen_string0;
    c_string_t *codegen_string1;
    int next_code_id;

    c_string_t *code_string;
    char *code_dso_filename;
    bool need_recompile;
#endif

    rig_code_node_t *code_graph;
    c_module_t *code_dso_module;

    rut_object_t *renderer;

    /* XXX: Move to rig_editor_t */
    c_list_t *undo_journal_stack;
    rig_undo_journal_t *undo_journal;

    /* shadow mapping */
    cg_offscreen_t *shadow_fb;
    cg_texture_2d_t *shadow_color;
    cg_texture_t *shadow_map;

    rut_stack_t *top_stack;
    rig_camera_view_t *main_camera_view;

    /* XXX: Move to rig_editor_t */
    rut_bin_t *top_bin;
    rut_box_layout_t *top_vbox;
    rut_box_layout_t *top_hbox;
    rut_box_layout_t *top_bar_hbox;
    rut_box_layout_t *top_bar_hbox_ltr;
    rut_box_layout_t *top_bar_hbox_rtl;
    rut_box_layout_t *asset_panel_hbox;
    rut_box_layout_t *toolbar_vbox;
    rut_box_layout_t *properties_hbox;
    rig_split_view_t *splits[1];
    // RutBevel *main_area_bevel;
    rut_stack_t *icon_bar_stack;
    rut_stack_t *left_bar_stack;
    // rut_transform_t *left_bar_transform;
    // rut_transform_t *right_bar_transform;
    rut_stack_t *right_bar_stack;
    // rut_transform_t *main_transform;
    rut_stack_t *bottom_bar_stack;
    // rut_transform_t *bottom_bar_transform;
    // rut_transform_t *screen_area_transform;

    cg_primitive_t *grid_prim;
    cg_attribute_t *circle_node_attribute;
    int circle_node_n_verts;

    float window_width;
    float window_height;

    float screen_area_width;
    float screen_area_height;

    float device_width;
    float device_height;

    /* XXX: Move to rig_editor_t */
    rut_ui_viewport_t *tool_vp;
    rut_ui_viewport_t *properties_vp;
    rut_bin_t *inspector_bin;
    rut_box_layout_t *inspector_box_layout;
    rut_object_t *inspector;
    c_list_t *all_inspectors;

    /* XXX: Move to rig_editor_t */
    rig_controller_view_t *controller_view;

    cg_matrix_t main_view;
    float z_2d;

/* XXX: Move to rig_editor_t */
#ifdef RIG_EDITOR_ENABLED
    rig_entity_t *light_handle;
    rig_entity_t *play_camera_handle;
    rig_objects_selection_t *objects_selection;
    /* The transparency grid widget that is displayed behind the assets list */
    rut_image_t *transparency_grid;
#endif

    float grab_x;
    float grab_y;
    float entity_grab_pos[3];
    rut_input_callback_t key_focus_callback;
    float grab_progress;

    rig_controller_t *selected_controller;
    rut_property_closure_t *controller_progress_closure;

    rut_transform_t *resize_handle_transform;

#ifdef __APPLE__
    rig_osx_data_t *osx_data;
#endif

    cg_snippet_t *alpha_mask_snippet;
    cg_snippet_t *alpha_mask_video_snippet;
    cg_snippet_t *lighting_vertex_snippet;
    cg_snippet_t *normal_map_vertex_snippet;
    cg_snippet_t *shadow_mapping_vertex_snippet;
    cg_snippet_t *blended_discard_snippet;
    cg_snippet_t *unblended_discard_snippet;
    cg_snippet_t *premultiply_snippet;
    cg_snippet_t *unpremultiply_snippet;
    cg_snippet_t *normal_map_fragment_snippet;
    cg_snippet_t *normal_map_video_snippet;
    cg_snippet_t *material_lighting_snippet;
    cg_snippet_t *simple_lighting_snippet;
    cg_snippet_t *shadow_mapping_fragment_snippet;
    cg_snippet_t *pointalism_vertex_snippet;
    cg_snippet_t *pointalism_video_snippet;
    cg_snippet_t *pointalism_halo_snippet;
    cg_snippet_t *pointalism_opaque_snippet;
    cg_snippet_t *cache_position_snippet;
    cg_snippet_t *hair_simple_snippet;
    cg_snippet_t *hair_material_snippet;
    cg_snippet_t *hair_vertex_snippet;
    cg_snippet_t *hair_fin_snippet;

    /* TODO: The frontend, editor and simulator should be accessed as
     * traits of the engine.
     */
    rig_frontend_t *
    frontend; /* NULL if engine not acting as a frontend process */
    rig_editor_t *editor; /* NULL if frontend isn't an editor */
    rig_simulator_t *simulator; /* NULL if engine not acting as a simulator */

    rig_rpc_server_t *slave_service;

#ifdef USE_AVAHI
    const AvahiPoll *avahi_poll_api;
    char *avahi_service_name;
    AvahiClient *avahi_client;
    AvahiEntryGroup *avahi_group;
    AvahiServiceBrowser *avahi_browser;
#endif

    c_list_t *slave_addresses;

    c_list_t *slave_masters;

    rig_ui_t *edit_mode_ui;
    rig_ui_t *play_mode_ui;
    rig_ui_t *current_ui;

    rut_queue_t *queued_deletes;

    /* Note: this is only referenced by the rig_engine_op_XYZ operation
     * functions, and when calling apis like rig_engine_pb_op_apply()
     * that take an explicit apply_op_ctx argument then this isn't
     * referenced. */
    rig_engine_op_apply_context_t *apply_op_ctx;
    void (*log_op_callback)(Rig__Operation *pb_op, void *user_data);
    void *log_op_data;

    void (*play_mode_callback)(bool play_mode, void *user_data);
    void *play_mode_data;

    void (*ui_load_callback)(void *user_data);
    void *ui_load_data;

    rut_introspectable_props_t introspectable;
    rut_property_t properties[RIG_ENGINE_N_PROPS];
};

/* FIXME: find a better place to put these prototypes */

extern rut_type_t rig_objects_selection_type;

rig_engine_t *rig_engine_new_for_frontend(rut_shell_t *shell,
                                          rig_frontend_t *frontend);

rig_engine_t *rig_engine_new_for_simulator(rut_shell_t *shell,
                                           rig_simulator_t *simulator);

void rig_engine_load_file(rig_engine_t *engine, const char *filename);

void rig_engine_load_empty_ui(rig_engine_t *engine);

rut_input_event_status_t rig_engine_input_handler(rut_input_event_t *event,
                                                  void *user_data);

void rig_engine_paint(rig_engine_t *engine);

void rig_engine_resize(rig_engine_t *engine, int width, int height);

void rig_engine_set_onscreen_size(rig_engine_t *engine, int width, int height);

void rig_engine_set_edit_mode_ui(rig_engine_t *engine, rig_ui_t *ui);
void rig_engine_set_play_mode_ui(rig_engine_t *engine, rig_ui_t *ui);

void rig_register_asset(rig_engine_t *engine, rig_asset_t *asset);

rig_asset_t *rig_lookup_asset(rig_engine_t *engine, const char *path);

rig_asset_t *
rig_load_asset(rig_engine_t *engine, GFileInfo *info, GFile *asset_file);

typedef void (*rig_tool_changed_callback_t)(rig_engine_t *engine,
                                            rig_tool_id_t tool_id,
                                            void *user_data);

void
rig_add_tool_changed_callback(rig_engine_t *engine,
                              rig_tool_changed_callback_t callback,
                              void *user_data,
                              rut_closure_destroy_callback_t destroy_notify);

void rig_engine_set_log_op_callback(rig_engine_t *engine,
                                    void (*callback)(Rig__Operation *pb_op,
                                                     void *user_data),
                                    void *user_data);

void rig_engine_set_ui_load_callback(rig_engine_t *engine,
                                     void (*callback)(void *user_data),
                                     void *user_data);

void rig_engine_set_play_mode_callback(rig_engine_t *engine,
                                       void (*callback)(bool play_mode,
                                                        void *user_data),
                                       void *user_data);

void rig_engine_queue_delete(rig_engine_t *engine, rut_object_t *object);

void rig_engine_garbage_collect(rig_engine_t *engine,
                                void (*object_callback)(rut_object_t *object,
                                                        void *user_data),
                                void *user_data);

void rig_engine_set_play_mode_enabled(rig_engine_t *engine, bool enabled);

char *rig_engine_get_object_debug_name(rut_object_t *object);

void rig_engine_set_apply_op_context(rig_engine_t *engine,
                                     rig_engine_op_apply_context_t *ctx);

void rig_engine_allocate(rig_engine_t *engine);

#endif /* _RIG_ENGINE_H_ */
