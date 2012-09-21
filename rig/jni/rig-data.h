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

#include "rig-transition.h"

/* TODO:
 * This structure should be split up into runtime data and editor data
 */

typedef struct _RigUndoJournal RigUndoJournal;

enum {
  RUT_DATA_PROP_WIDTH,
  RUT_DATA_PROP_HEIGHT,

  RUT_DATA_N_PROPS
};

typedef struct _RigData
{
  CoglBool play_mode;

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

  RutShell *shell;
  RutContext *ctx;
  CoglOnscreen *onscreen;

  RigUndoJournal *undo_journal;

  /* shadow mapping */
  CoglOffscreen *shadow_fb;
  CoglTexture2D *shadow_color;
  CoglTexture *shadow_map;
  RutCamera *shadow_map_camera;

  CoglTexture *circle_texture;

  CoglTexture *light_icon;
  CoglTexture *clip_plane_icon;

  //float width;
  //RutProperty width_property;
  //float height;
  //RutProperty height_property;

  RutSplitView *splits[5];


  RutBevel *main_area_bevel;
  RutStack *top_bar_stack;
  RutStack *icon_bar_stack;
  //RutTransform *top_bar_transform;
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
  float top_bar_height;
  float left_bar_width;
  float right_bar_width;
  float bottom_bar_height;
  float grab_margin;
  float main_x;
  float main_y;
  float main_width;
  float main_height;
  float screen_area_width;
  float screen_area_height;

  RutRectangle *top_bar_rect;
  RutRectangle *icon_bar_rect;
  RutRectangle *left_bar_rect;
  RutRectangle *right_bar_rect;
  RutRectangle *bottom_bar_rect;

  RutUIViewport *assets_vp;
  RutGraph *assets_list;
  GList *asset_input_closures;

  RutUIViewport *tool_vp;
  RutObject *inspector;
  GList *component_inspectors;

  RutCamera *timeline_camera;
  RutInputRegion *timeline_input_region;
  float timeline_width;
  float timeline_height;
  float timeline_len;
  float timeline_scale;

  RutUIViewport *timeline_vp;

  float grab_timeline_vp_t;
  float grab_timeline_vp_y;

  CoglMatrix main_view;
  float z_2d;

  RutEntity *editor_camera_to_origin; /* move to origin */
  RutEntity *editor_camera_rotate; /* armature rotate rotate */
  RutEntity *editor_camera_origin_offset; /* negative offset */
  RutEntity *editor_camera_armature; /* armature length */
  RutEntity *editor_camera_dev_scale; /* scale to fit device coords */
  RutEntity *editor_camera_screen_pos; /* position screen in edit view */
  RutEntity *editor_camera_2d_view; /* setup 2d view, origin top-left */

  RutEntity *current_camera;

  RutEntity *editor_camera;
  RutCamera *editor_camera_component;
  float editor_camera_z;
  RutInputRegion *editor_input_region;

  RutEntity *light;
  RutEntity *light_handle;

  /* postprocessing */
  CoglFramebuffer *postprocess;
  RutDepthOfField *dof;
  CoglBool enable_dof;

  RutArcball arcball;
  CoglQuaternion saved_rotation;
  float origin[3];
  float saved_origin[3];

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

  uint32_t entity_next_id;
  GList *entities;
  GList *lights;
  GList *transitions;

  RutEntity *selected_entity;
  RigTransition *selected_transition;

  RutTool *tool;

  /* picking ray */
  CoglPipeline *picking_ray_color;
  CoglPrimitive *picking_ray;
  CoglBool debug_pick_ray;

  //Path *path;
  //float path_t;
  //RutProperty path_property;

  RutProperty properties[RUT_DATA_N_PROPS];

} RigData;

/* FIXME: find a better place to put these prototypes */

extern CoglBool _rut_in_device_mode;

void
rig_update_asset_list (RigData *data);

RigTransition *
rig_create_transition (RigData *data,
                       uint32_t id);

void
rig_free_ux (RigData *data);

#endif /* _RUT_DATA_H_ */
