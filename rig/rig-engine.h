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

#include <gmodule.h>
#include <gio/gio.h>

#include <rut.h>

/* Forward declare since there is a circular dependency between this
 * header and rig-camera-view.h which depends on this typedef... */
typedef enum _RigToolID
{
  RIG_TOOL_ID_SELECTION = 1,
  RIG_TOOL_ID_ROTATION,
} RigToolID;

typedef enum _RutSelectAction
{
  /* replaces the current selection */
  RUT_SELECT_ACTION_REPLACE,
  /* toggles whether the given item is selected or not */
  RUT_SELECT_ACTION_TOGGLE,
} RutSelectAction;

#include "rig-types.h"

typedef struct _RigEntitesSelection
{
  RutObjectBase _base;
  RigEngine *engine;
  CList *objects;
  RutList selection_events_cb_list;
} RigObjectsSelection;


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

extern RutType rig_engine_type;

struct _RigEngine
{
  RutObjectBase _base;

  RigFrontendID frontend_id;

  bool headless;
  bool play_mode;

  char *ui_filename;
  RutClosure *finish_ui_load_closure;

  RutObject *camera_2d;
  RutObject *root;

  CoglMatrix identity;

  /* TODO: Move to RigFrontend */
  CoglTexture *gradient;

  /* TODO: Move to RigFrontend */
  GHashTable *image_source_wrappers;

  /* TODO: Move to RigFrontend */
  CoglPipeline *shadow_color_tex;
  CoglPipeline *shadow_map_tex;

  CoglPipeline *default_pipeline;

  /* TODO: Move to RigFrontend */
  CoglPipeline *dof_pipeline_template;
  CoglPipeline *dof_pipeline;
  CoglPipeline *dof_diamond_pipeline;
  CoglPipeline *dof_unshaped_pipeline;

  RutShell *shell;
  RutContext *ctx;

  /* TODO: Move to RigFrontend */
  CoglOnscreen *onscreen;

  RigPBSerializer *ops_serializer;
  RutMemoryStack *frame_stack;
  RutMagazine *object_id_magazine;

  /* TODO: Move to RigFrontend */
  RutMemoryStack *sim_frame_stack;

  RutInputQueue *simulator_input_queue;

  /* XXX: Move to RigEditor */
#ifdef RIG_EDITOR_ENABLED
  RutText *search_text;
  CList *required_search_tags;

  RutList tool_changed_cb_list;

  CString *codegen_string0;
  CString *codegen_string1;
  int next_code_id;

  CString *code_string;
  char *code_dso_filename;
  bool need_recompile;
#endif

  RigCodeNode *code_graph;
  GModule *code_dso_module;

  RutObject *renderer;

  /* XXX: Move to RigEditor */
  CList *undo_journal_stack;
  RigUndoJournal *undo_journal;

  /* shadow mapping */
  CoglOffscreen *shadow_fb;
  CoglTexture2D *shadow_color;
  CoglTexture *shadow_map;

  RutStack *top_stack;
  RigCameraView *main_camera_view;

  /* XXX: Move to RigEditor */
  RutBin *top_bin;
  RutBoxLayout *top_vbox;
  RutBoxLayout *top_hbox;
  RutBoxLayout *top_bar_hbox;
  RutBoxLayout *top_bar_hbox_ltr;
  RutBoxLayout *top_bar_hbox_rtl;
  RutBoxLayout *asset_panel_hbox;
  RutBoxLayout *toolbar_vbox;
  RutBoxLayout *properties_hbox;
  RigSplitView *splits[1];
  //RutBevel *main_area_bevel;
  RutStack *icon_bar_stack;
  RutStack *left_bar_stack;
  //RutTransform *left_bar_transform;
  //RutTransform *right_bar_transform;
  RutStack *right_bar_stack;
  //RutTransform *main_transform;
  RutStack *bottom_bar_stack;
  //RutTransform *bottom_bar_transform;
  //RutTransform *screen_area_transform;

  CoglPrimitive *grid_prim;
  CoglAttribute *circle_node_attribute;
  int circle_node_n_verts;

  float window_width;
  float window_height;

  float screen_area_width;
  float screen_area_height;

  float device_width;
  float device_height;

  /* XXX: Move to RigEditor */
  RutUIViewport *tool_vp;
  RutUIViewport *properties_vp;
  RutBin *inspector_bin;
  RutBoxLayout *inspector_box_layout;
  RutObject *inspector;
  CList *all_inspectors;

  /* XXX: Move to RigEditor */
  RigControllerView *controller_view;

  CoglMatrix main_view;
  float z_2d;

  /* XXX: Move to RigEditor */
#ifdef RIG_EDITOR_ENABLED
  RigEntity *light_handle;
  RigEntity *play_camera_handle;
  RigObjectsSelection *objects_selection;
  /* The transparency grid widget that is displayed behind the assets list */
  RutImage *transparency_grid;
#endif

  float grab_x;
  float grab_y;
  float entity_grab_pos[3];
  RutInputCallback key_focus_callback;
  float grab_progress;

  RigController *selected_controller;
  RutPropertyClosure *controller_progress_closure;

  RutTransform *resize_handle_transform;

#ifdef __APPLE__
  RigOSXData *osx_data;
#endif

  CoglSnippet *alpha_mask_snippet;
  CoglSnippet *alpha_mask_video_snippet;
  CoglSnippet *lighting_vertex_snippet;
  CoglSnippet *normal_map_vertex_snippet;
  CoglSnippet *shadow_mapping_vertex_snippet;
  CoglSnippet *blended_discard_snippet;
  CoglSnippet *unblended_discard_snippet;
  CoglSnippet *premultiply_snippet;
  CoglSnippet *unpremultiply_snippet;
  CoglSnippet *normal_map_fragment_snippet;
  CoglSnippet *normal_map_video_snippet;
  CoglSnippet *material_lighting_snippet;
  CoglSnippet *simple_lighting_snippet;
  CoglSnippet *shadow_mapping_fragment_snippet;
  CoglSnippet *pointalism_vertex_snippet;
  CoglSnippet *pointalism_video_snippet;
  CoglSnippet *pointalism_halo_snippet;
  CoglSnippet *pointalism_opaque_snippet;
  CoglSnippet *cache_position_snippet;
  CoglSnippet *hair_simple_snippet;
  CoglSnippet *hair_material_snippet;
  CoglSnippet *hair_vertex_snippet;
  CoglSnippet *hair_fin_snippet;

  /* TODO: The frontend, editor and simulator should be accessed as
   * traits of the engine.
   */
  RigFrontend *frontend; /* NULL if engine not acting as a frontend process */
  RigEditor *editor; /* NULL if frontend isn't an editor */
  RigSimulator *simulator; /* NULL if engine not acting as a simulator */

  RigRPCServer *slave_service;

#ifdef USE_AVAHI
  const AvahiPoll *avahi_poll_api;
  char *avahi_service_name;
  AvahiClient *avahi_client;
  AvahiEntryGroup *avahi_group;
  AvahiServiceBrowser *avahi_browser;
#endif

  CList *slave_addresses;

  CList *slave_masters;

  RigUI *edit_mode_ui;
  RigUI *play_mode_ui;
  RigUI *current_ui;

  RutQueue *queued_deletes;

  /* Note: this is only referenced by the rig_engine_op_XYZ operation
   * functions, and when calling apis like rig_engine_pb_op_apply()
   * that take an explicit apply_op_ctx argument then this isn't
   * referenced. */
  RigEngineOpApplyContext *apply_op_ctx;
  void (*log_op_callback) (Rig__Operation *pb_op,
                           void *user_data);
  void *log_op_data;

  void (*play_mode_callback) (bool play_mode,
                              void *user_data);
  void *play_mode_data;

  void (*ui_load_callback) (void *user_data);
  void *ui_load_data;

  RutIntrospectableProps introspectable;
  RutProperty properties[RIG_ENGINE_N_PROPS];
};

/* FIXME: find a better place to put these prototypes */

extern RutType rig_objects_selection_type;

RigEngine *
rig_engine_new_for_frontend (RutShell *shell,
                             RigFrontend *frontend);

RigEngine *
rig_engine_new_for_simulator (RutShell *shell,
                              RigSimulator *simulator);

void
rig_engine_load_file (RigEngine *engine,
                      const char *filename);

void
rig_engine_load_empty_ui (RigEngine *engine);

RutInputEventStatus
rig_engine_input_handler (RutInputEvent *event, void *user_data);

void
rig_engine_paint (RigEngine *engine);

void
rig_engine_resize (RigEngine *engine,
                   int width,
                   int height);

void
rig_engine_set_onscreen_size (RigEngine *engine,
                              int width,
                              int height);

void
rig_engine_set_edit_mode_ui (RigEngine *engine,
                             RigUI *ui);
void
rig_engine_set_play_mode_ui (RigEngine *engine,
                             RigUI *ui);

void
rig_register_asset (RigEngine *engine,
                    RigAsset *asset);

RigAsset *
rig_lookup_asset (RigEngine *engine,
                  const char *path);

RigAsset *
rig_load_asset (RigEngine *engine, GFileInfo *info, GFile *asset_file);

typedef void (*RigToolChangedCallback) (RigEngine *engine,
                                        RigToolID tool_id,
                                        void *user_data);

void
rig_add_tool_changed_callback (RigEngine *engine,
                               RigToolChangedCallback callback,
                               void *user_data,
                               RutClosureDestroyCallback destroy_notify);

void
rig_engine_set_log_op_callback (RigEngine *engine,
                                void (*callback) (Rig__Operation *pb_op,
                                                  void *user_data),
                                void *user_data);

void
rig_engine_set_ui_load_callback (RigEngine *engine,
                                 void (*callback) (void *user_data),
                                 void *user_data);

void
rig_engine_set_play_mode_callback (RigEngine *engine,
                                   void (*callback) (bool play_mode,
                                                     void *user_data),
                                   void *user_data);

void
rig_engine_queue_delete (RigEngine *engine,
                         RutObject *object);

void
rig_engine_garbage_collect (RigEngine *engine,
                            void (*object_callback) (RutObject *object,
                                                     void *user_data),
                            void *user_data);

void
rig_engine_set_play_mode_enabled (RigEngine *engine, bool enabled);

char *
rig_engine_get_object_debug_name (RutObject *object);

void
rig_engine_set_apply_op_context (RigEngine *engine,
                                 RigEngineOpApplyContext *ctx);

void
rig_engine_allocate (RigEngine *engine);

#endif /* _RIG_ENGINE_H_ */
