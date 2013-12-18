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

#ifndef _RUT_ENGINE_H_
#define _RUT_ENGINE_H_

#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-client/lookup.h>

/* Forward declare since there is a circular dependency between this
 * header and rig-camera-view.h which depends on this typedef... */
typedef enum _RigToolID
{
  RIG_TOOL_ID_SELECTION = 1,
  RIG_TOOL_ID_ROTATION,
} RigToolID;


#include "rig-protobuf-c-rpc.h"
#include "rig-rpc-network.h"

#include "rig-controller.h"
#include "rig-controller-view.h"
#include "rig-types.h"
#include "rig-undo-journal.h"
#include "rut-box-layout.h"
#include "rig-osx.h"
#include "rig-split-view.h"
#include "rig-camera-view.h"

enum {
  RIG_ENGINE_PROP_WIDTH,
  RIG_ENGINE_PROP_HEIGHT,
  RIG_ENGINE_PROP_DEVICE_WIDTH,
  RIG_ENGINE_PROP_DEVICE_HEIGHT,

  RIG_ENGINE_N_PROPS
};

typedef struct _RigEntitesSelection
{
  RutObjectProps _parent;
  int ref_count;
  RigEngine *engine;
  GList *objects;
  RutList selection_events_cb_list;
} RigObjectsSelection;

struct _RigEngine
{
  CoglBool play_mode;

  char *ui_filename;
  char *next_ui_filename;

  RutCamera *camera;
  RutObject *root;
  RutObject *scene;

  CoglMatrix identity;

  CoglTexture *gradient;

  CoglPipeline *shadow_color_tex;
  CoglPipeline *shadow_map_tex;

  CoglPipeline *default_pipeline;

  CoglPipeline *dof_pipeline_template;
  CoglPipeline *dof_pipeline;
  CoglPipeline *dof_diamond_pipeline;
  CoglPipeline *dof_unshaped_pipeline;

  RutShell *shell;
  RutContext *ctx;
  CoglOnscreen *onscreen;

#ifdef RIG_EDITOR_ENABLED
  RutMemoryStack *serialization_stack;

  RutText *search_text;
  GList *required_search_tags;

  RutList tool_changed_cb_list;
#endif

  RutObject *renderer;

  GList *undo_journal_stack;
  RigUndoJournal *undo_journal;

  /* shadow mapping */
  CoglOffscreen *shadow_fb;
  CoglTexture2D *shadow_color;
  CoglTexture *shadow_map;

  float device_width;
  float device_height;
  CoglColor background_color;

  //float width;
  //RutProperty width_property;
  //float height;
  //RutProperty height_property;

  RutStack *top_stack;
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
  RigCameraView *main_camera_view;
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

  //RutTransform *slider_transform;
  //RutSlider *slider;
  //RutProperty *slider_progress;
  RutRectangle *rect;
  float width;
  float height;
  //float main_x;
  //float main_y;
  //float main_width;
  //float main_height;
  float screen_area_width;
  float screen_area_height;

  RutUIViewport *search_vp;
  RutFold *search_results_fold;
  RutBoxLayout *search_results_vbox;
  RutFlowLayout *entity_results;
  RutFlowLayout *controller_results;
  RutFlowLayout *assets_geometry_results;
  RutFlowLayout *assets_image_results;
  RutFlowLayout *assets_video_results;
  RutFlowLayout *assets_other_results;

  RutAsset *text_builtin_asset;
  RutAsset *circle_builtin_asset;
  RutAsset *nine_slice_builtin_asset;
  RutAsset *diamond_builtin_asset;
  RutAsset *pointalism_grid_builtin_asset;
  RutAsset *hair_builtin_asset;
  RutAsset *button_input_builtin_asset;
  GList *result_input_closures;
  GList *asset_enumerators;

  RutUIViewport *tool_vp;
  RutUIViewport *properties_vp;
  RutBin *inspector_bin;
  RutBoxLayout *inspector_box_layout;
  RutObject *inspector;
  GList *all_inspectors;

  RigControllerView *controller_view;

  CoglMatrix main_view;
  float z_2d;

  RutEntity *light;
  RutEntity *light_handle;

  RutEntity *play_camera;
  RutCamera *play_camera_component;
#ifdef RIG_EDITOR_ENABLED
  RutEntity *play_camera_handle;
#endif

  /* postprocessing */
  CoglFramebuffer *postprocess;
  RutDepthOfField *dof;
  CoglBool enable_dof;

  RutArcball arcball;
  CoglQuaternion saved_rotation;

  float grab_x;
  float grab_y;
  float entity_grab_pos[3];
  RutInputCallback key_focus_callback;
  float grab_progress;

  GList *assets;

  GList *controllers;
  RigController *selected_controller;
  RutPropertyClosure *controller_progress_closure;

  RigObjectsSelection *objects_selection;

  /* picking ray */
  CoglPipeline *picking_ray_color;
  CoglPrimitive *picking_ray;
  CoglBool debug_pick_ray;

  /* The transparency grid widget that is displayed behind the assets list */
  RutImage *transparency_grid;

  RutTransform *resize_handle_transform;

  //Path *path;
  //float path_t;
  //RutProperty path_property;

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

  GHashTable *assets_registry;

  pid_t simulator_pid;
  RigRPCPeer *simulator_peer;

  RigRPCServer *slave_service;

  const AvahiPoll *avahi_poll_api;
  char *avahi_service_name;
  AvahiClient *avahi_client;
  AvahiEntryGroup *avahi_group;
  AvahiServiceBrowser *avahi_browser;

  GList *slave_addresses;

  GList *slave_masters;

  RutProperty properties[RIG_ENGINE_N_PROPS];
};

/* FIXME: find a better place to put these prototypes */

#ifdef RIG_EDITOR_ENABLED
extern bool _rig_in_editor_mode;
#endif

extern bool _rig_in_simulator_mode;

extern RutType rig_objects_selection_type;


void
rig_engine_init (RigEngine *engine, RutShell *shell);

RutInputEventStatus
rig_engine_input_handler (RutInputEvent *event, void *user_data);

CoglBool
rig_engine_paint (RutShell *shell, void *user_data);

void
rig_engine_fini (RutShell *shell, void *user_data);

void
rig_engine_set_onscreen_size (RigEngine *engine,
                              int width,
                              int height);

void
rig_engine_free_ui (RigEngine *engine);

void
rig_engine_handle_ui_update (RigEngine *engine);

void
rig_register_asset (RigEngine *engine,
                    RutAsset *asset);

RutAsset *
rig_lookup_asset (RigEngine *engine,
                  const char *path);

RutAsset *
rig_load_asset (RigEngine *engine, GFileInfo *info, GFile *asset_file);

typedef enum _RutSelectAction
{
  /* replaces the current selection */
  RUT_SELECT_ACTION_REPLACE,
  /* toggles whether the given item is selected or not */
  RUT_SELECT_ACTION_TOGGLE,
} RutSelectAction;

void
rig_select_object (RigEngine *engine,
                   RutObject *object,
                   RutSelectAction action);

void
rig_reload_inspector_property (RigEngine *engine,
                               RutProperty *property);

void
rig_reload_position_inspector (RigEngine *engine,
                               RutEntity *entity);

void
rig_set_play_mode_enabled (RigEngine *engine, CoglBool enabled);

void
rig_engine_sync_slaves (RigEngine *engine);

void
rig_engine_dirty_properties_menu (RutImageSource *source,
                                  void *user_data);

void
_rig_engine_update_inspector (RigEngine *engine);

void
rig_engine_push_undo_subjournal (RigEngine *engine);

RigUndoJournal *
rig_engine_pop_undo_subjournal (RigEngine *engine);

typedef enum _RigObjectsSelectionEvent
{
  RIG_OBJECTS_SELECTION_ADD_EVENT,
  RIG_OBJECTS_SELECTION_REMOVE_EVENT
} RigObjectsSelectionEvent;

typedef void (*RigObjectsSelectionEventCallback) (RigObjectsSelection *selection,
                                                  RigObjectsSelectionEvent event,
                                                  RutObject *object,
                                                  void *user_data);

RutClosure *
rig_objects_selection_add_event_callback (RigObjectsSelection *selection,
                                          RigObjectsSelectionEventCallback callback,
                                          void *user_data,
                                          RutClosureDestroyCallback destroy_cb);

typedef void (*RigToolChangedCallback) (RigEngine *engine,
                                        RigToolID tool_id,
                                        void *user_data);

void
rig_add_tool_changed_callback (RigEngine *engine,
                               RigToolChangedCallback callback,
                               void *user_data,
                               RutClosureDestroyCallback destroy_notify);


#endif /* _RUT_ENGINE_H_ */
