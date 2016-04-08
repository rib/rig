/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2014  Intel Corporation.
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

#include <clib.h>

#include <rut.h>

typedef struct _rig_ui_t rig_ui_t;
typedef struct _rig_view rig_view_t;

#include "rut-object.h"
#include "rig-engine.h"
#include "rig-entity.h"
#include "rig-controller.h"
#include "rig-camera-view.h"

typedef struct _rig_ui_grab rig_ui_grab_t;

enum {
    RIG_VIEW_PROP_WIDTH,
    RIG_VIEW_PROP_HEIGHT,
    RIG_VIEW_PROP_CAMERA_ENTITY,
    RIG_VIEW_N_PROPS,
};

struct _rig_view
{
    rut_object_base_t _base;

    rig_engine_t *engine;

    int width;
    int height;

    rut_object_t *camera_entity;

    /* Frontend only...
     */
    rig_camera_view_t *camera_view;
    rut_shell_onscreen_t *onscreen;


    rig_introspectable_props_t introspectable;
    rig_property_t properties[RIG_VIEW_N_PROPS];

};

struct _rig_ui_t {
    rut_object_base_t _base;

    rig_engine_t *engine;

    /* XXX: entity changes within a UI need to notify the renderer so
     * if this UI is loaded within a frontend then to avoid a bit of
     * indirection we point to the renderer here.
     */
    rut_object_t *renderer;

    c_llist_t *assets;

    rut_object_t *scene;

    c_llist_t *cameras;
    c_llist_t *lights;
    c_llist_t *controllers;
    c_llist_t *views;
    c_llist_t *buffers;

    bool dirty_view_geometry;

    /* TODO: remove the limitation of rendering with only one light */
    rig_entity_t *light;

    rut_matrix_stack_t *pick_matrix_stack;

    /* List of grabs that are currently in place. This are in order from
     * highest to lowest priority. */
    c_list_t grabs;
    /* A pointer to the next grab to process. This is only used while
     * invoking the grab callbacks so that we can cope with multiple
     * grabs being removed from the list while one is being processed */
    rig_ui_grab_t *next_grab;

    uint8_t *dso_data;
    int dso_len;

    c_list_t code_modules;

    c_llist_t *suspended_controllers;
    bool suspended;
};

rig_ui_t *rig_ui_new(rig_engine_t *engine);

void rig_ui_set_dso_data(rig_ui_t *ui, uint8_t *data, int len);

void rig_ui_suspend(rig_ui_t *ui);

void rig_ui_resume(rig_ui_t *ui);

rig_entity_t *rig_ui_find_entity(rig_ui_t *ui, const char *label);

void rig_ui_reap(rig_ui_t *ui);

void rig_ui_add_controller(rig_ui_t *ui, rig_controller_t *controller);

void rig_ui_remove_controller(rig_ui_t *ui, rig_controller_t *controller);

void rig_ui_print(rig_ui_t *ui);

void rig_ui_entity_component_added_notify(rig_ui_t *ui,
                                          rig_entity_t *entity,
                                          rut_object_t *component);
void rig_ui_entity_component_pre_remove_notify(rig_ui_t *ui,
                                               rig_entity_t *entity,
                                               rut_object_t *component);
void rig_ui_register_all_entity_components(rig_ui_t *ui, rig_entity_t *entity);

void rig_ui_code_modules_load(rig_ui_t *ui);
void rig_ui_code_modules_update(rig_ui_t *ui);
void rig_ui_code_modules_handle_input(rig_ui_t *ui, rut_input_event_t *event);

rut_input_event_status_t rig_ui_handle_input_event(rig_ui_t *ui,
                                                   rut_input_event_t *event);


typedef rut_input_event_status_t (*rig_input_grab_callback_t)(rut_input_event_t *event,
                                                              rut_object_t *pick_entity,
                                                              void *user_data);
void rig_ui_grab_input(rig_ui_t *ui,
                       rut_object_t *camera_entity,
                       rig_input_grab_callback_t callback,
                       void *user_data);

void rig_ui_ungrab_input(rig_ui_t *ui,
                         rig_input_grab_callback_t callback,
                         void *user_data);

rig_view_t *rig_view_new(rig_engine_t *engine);

void rig_view_set_width(rut_object_t *view, int width);
void rig_view_set_height(rut_object_t *view, int height);
void rig_view_set_camera(rut_object_t *obj, rut_object_t *camera_entity);

void rig_view_reap(rig_view_t *view);

void rig_ui_add_view(rig_ui_t *ui, rig_view_t *view);
void rig_ui_remove_view(rig_ui_t *ui, rig_view_t *view);

rig_view_t *rig_ui_find_view_for_onscreen(rig_ui_t *ui, rut_shell_onscreen_t *onscreen);

void rig_ui_add_buffer(rig_ui_t *ui, rut_buffer_t *buffer);
void rig_ui_remove_buffer(rig_ui_t *ui, rut_buffer_t *buffer);
void rig_ui_reap_buffer(rig_ui_t *ui, rut_buffer_t *buffer);

