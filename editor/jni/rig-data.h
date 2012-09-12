/*
 * Rig
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

#ifndef _RIG_DATA_H_
#define _RIG_DATA_H_

#include "rig-transition.h"

/* TODO:
 * This structure should be split up into runtime data and editor data
 */

typedef struct _RigUndoJournal RigUndoJournal;

enum {
  RIG_DATA_PROP_WIDTH,
  RIG_DATA_PROP_HEIGHT,

  RIG_DATA_N_PROPS
};

typedef struct _RigData
{
  RigCamera *camera;
  RigObject *root;
  RigObject *scene;

  CoglMatrix identity;

  CoglTexture *gradient;

  CoglPipeline *shadow_color_tex;
  CoglPipeline *shadow_map_tex;

  CoglPipeline *default_pipeline;

  CoglPipeline *dof_pipeline_template;
  CoglPipeline *dof_pipeline;
  CoglPipeline *dof_diamond_pipeline;

  RigShell *shell;
  RigContext *ctx;
  CoglOnscreen *onscreen;

  RigUndoJournal *undo_journal;

  /* shadow mapping */
  CoglOffscreen *shadow_fb;
  CoglTexture2D *shadow_color;
  CoglTexture *shadow_map;
  RigCamera *shadow_map_camera;

  CoglTexture *circle_texture;

  CoglTexture *light_icon;
  CoglTexture *clip_plane_icon;

  //float width;
  //RigProperty width_property;
  //float height;
  //RigProperty height_property;

  RigSplitView *splits[5];


  RigBevel *main_area_bevel;
  RigStack *top_bar_stack;
  RigStack *icon_bar_stack;
  //RigTransform *top_bar_transform;
  RigStack *left_bar_stack;
  //RigTransform *left_bar_transform;
  //RigTransform *right_bar_transform;
  RigStack *right_bar_stack;
  //RigTransform *main_transform;

  RigStack *bottom_bar_stack;
  //RigTransform *bottom_bar_transform;

  //RigTransform *screen_area_transform;

  CoglPrimitive *grid_prim;
  CoglAttribute *circle_node_attribute;
  int circle_node_n_verts;

  //RigTransform *slider_transform;
  //RigSlider *slider;
  //RigProperty *slider_progress;
  RigRectangle *rect;
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

  RigRectangle *top_bar_rect;
  RigRectangle *icon_bar_rect;
  RigRectangle *left_bar_rect;
  RigRectangle *right_bar_rect;
  RigRectangle *bottom_bar_rect;

  RigUIViewport *assets_vp;
  RigGraph *assets_list;
  GList *asset_input_closures;

  RigUIViewport *tool_vp;
  RigObject *inspector;

  RigCamera *timeline_camera;
  RigInputRegion *timeline_input_region;
  float timeline_width;
  float timeline_height;
  float timeline_len;
  float timeline_scale;

  RigUIViewport *timeline_vp;

  float grab_timeline_vp_t;
  float grab_timeline_vp_y;

  CoglMatrix main_view;
  float z_2d;

  RigEntity *editor_camera_to_origin; /* move to origin */
  RigEntity *editor_camera_rotate; /* armature rotate rotate */
  RigEntity *editor_camera_origin_offset; /* negative offset */
  RigEntity *editor_camera_armature; /* armature length */
  RigEntity *editor_camera_dev_scale; /* scale to fit device coords */
  RigEntity *editor_camera_screen_pos; /* position screen in edit view */
  RigEntity *editor_camera_2d_view; /* setup 2d view, origin top-left */

  RigEntity *current_camera;

  RigEntity *editor_camera;
  RigCamera *editor_camera_component;
  float editor_camera_z;
  RigInputRegion *editor_input_region;

  RigEntity *plane;
  RigEntity *light;
  RigEntity *light_handle;

  /* postprocessing */
  CoglFramebuffer *postprocess;
  RigDepthOfField *dof;
  CoglBool enable_dof;

  RigArcball arcball;
  CoglQuaternion saved_rotation;
  float origin[3];
  float saved_origin[3];

  //RigTransform *screen_area_transform;
  RigTransform *device_transform;

  RigTimeline *timeline;
  RigProperty *timeline_elapsed;
  RigProperty *timeline_progress;

  float grab_x;
  float grab_y;
  float entity_grab_pos[3];
  RigInputCallback key_focus_callback;

  GList *assets;

  uint32_t entity_next_id;
  GList *entities;
  GList *lights;
  GList *transitions;

  RigEntity *selected_entity;
  RigTransition *selected_transition;

  RigTool *tool;

  /* picking ray */
  CoglPipeline *picking_ray_color;
  CoglPrimitive *picking_ray;
  CoglBool debug_pick_ray;

  //Path *path;
  //float path_t;
  //RigProperty path_property;

  RigProperty properties[RIG_DATA_N_PROPS];

} RigData;

/* FIXME: find a better place to put these prototypes */

void
rig_update_asset_list (RigData *data);

RigTransition *
rig_create_transition (RigData *data,
                       uint32_t id);

void
rig_free_ux (RigData *data);

#endif /* _RIG_DATA_H_ */
