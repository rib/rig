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

#ifndef _RUT_DATA_H_
#define _RUT_DATA_H_

#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-client/lookup.h>

#include "rig-protobuf-c-rpc.h"

#include "rig-transition.h"
#include "rig-transition-view.h"
#include "rig-types.h"
#include "rig-undo-journal.h"
#include "rut-box-layout.h"
#include "rig-osx.h"
#include "rig-split-view.h"
#include "rig-camera-view.h"

enum {
  RIG_DATA_PROP_WIDTH,
  RIG_DATA_PROP_HEIGHT,
  RIG_DATA_PROP_DEVICE_WIDTH,
  RIG_DATA_PROP_DEVICE_HEIGHT,

  RIG_DATA_N_PROPS
};

struct _RigData
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
#endif

  GArray *journal;

  RigUndoJournal *undo_journal;

  /* shadow mapping */
  CoglOffscreen *shadow_fb;
  CoglTexture2D *shadow_color;
  CoglTexture *shadow_map;
  RutCamera *shadow_map_camera;

  float device_width;
  float device_height;
  CoglColor background_color;

  //float width;
  //RutProperty width_property;
  //float height;
  //RutProperty height_property;

  RutBin *top_bin;
  RutBoxLayout *top_vbox;
  RutBoxLayout *top_hbox;
  RutBoxLayout *top_bar_hbox;
  RutBoxLayout *top_bar_hbox_ltr;
  RutBoxLayout *top_bar_hbox_rtl;
  RutBoxLayout *toolbar_vbox;
  RutBoxLayout *properties_hbox;
  RigSplitView *splits[2];

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

  RutUIViewport *assets_vp;
  RutFold *assets_results_fold;
  RutFlowLayout *assets_flow;
  RutAsset *text_builtin_asset;
  RutAsset *circle_builtin_asset;
  RutAsset *diamond_builtin_asset;
  GList *asset_input_closures;
  GList *asset_enumerators;

  RutUIViewport *tool_vp;
  RutBoxLayout *inspector_box_layout;
  RutObject *inspector;
  GList *all_inspectors;

  RutUIViewport *timeline_vp;
  RigTransitionView *transition_view;

  CoglMatrix main_view;
  float z_2d;

  RutEntity *light;
  RutEntity *light_handle;

  /* postprocessing */
  CoglFramebuffer *postprocess;
  RutDepthOfField *dof;
  CoglBool enable_dof;

  RutArcball arcball;
  CoglQuaternion saved_rotation;

  //RutTransform *screen_area_transform;
  RutTransform *device_transform;

  RutTimeline *timeline;
  RutProperty *timeline_elapsed;
  RutProperty *timeline_progress;

  float grab_x;
  float grab_y;
  float entity_grab_pos[3];
  RutInputCallback key_focus_callback;
  float grab_progress;

  GList *assets;

  GList *transitions;

  RutEntity *selected_entity;
  RigTransition *selected_transition;

  RutTool *tool;

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
  CoglSnippet *lighting_vertex_snippet;
  CoglSnippet *normal_map_vertex_snippet;
  CoglSnippet *shadow_mapping_vertex_snippet;
  CoglSnippet *blended_discard_snippet;
  CoglSnippet *unblended_discard_snippet;
  CoglSnippet *premultiply_snippet;
  CoglSnippet *unpremultiply_snippet;
  CoglSnippet *normal_map_fragment_snippet;
  CoglSnippet *material_lighting_snippet;
  CoglSnippet *simple_lighting_snippet;
  CoglSnippet *shadow_mapping_fragment_snippet;

  GHashTable *assets_registry;

  int rpc_server_port;
  PB_RPC_Server *rpc_server;
  int rpc_server_source_id;

  const AvahiPoll *avahi_poll_api;
  char *avahi_service_name;
  AvahiClient *avahi_client;
  AvahiEntryGroup *avahi_group;
  AvahiServiceBrowser *avahi_browser;

  GList *slave_addresses;

  GList *slave_masters;

  RutProperty properties[RIG_DATA_N_PROPS];
};

/* FIXME: find a better place to put these prototypes */

extern CoglBool _rig_in_device_mode;

RigTransition *
rig_create_transition (RigData *data,
                       uint32_t id);

void
rig_free_ux (RigData *data);

void
rig_register_asset (RigData *data,
                    RutAsset *asset);

RutAsset *
rig_lookup_asset (RigData *data,
                  const char *path);

RutAsset *
rig_load_asset (RigData *data, GFileInfo *info, GFile *asset_file);

void
rig_set_selected_entity (RigData *data,
                         RutEntity *entity);

void
rig_reload_inspector_property (RigData *data,
                               RutProperty *property);

void
rig_reload_position_inspector (RigData *data,
                               RutEntity *entity);

void
rig_set_play_mode_enabled (RigData *data, CoglBool enabled);

#endif /* _RUT_DATA_H_ */
