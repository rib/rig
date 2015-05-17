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

#ifndef _RIG_UI_H_
#define _RIG_UI_H_

#include <clib.h>

#include <rut.h>

typedef struct _rig_ui_t rig_ui_t;

#include "rut-object.h"
#include "rig-engine.h"
#include "rig-entity.h"
#include "rig-controller.h"

/* This structure encapsulates all the dynamic state for a UI
 *
 * When running the editor we distinguish the edit mode UI from
 * the play mode UI so that all UI logic involved in play mode
 * does not affect the state of the UI that gets saved.
 */
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
    c_llist_t *controllers;

    rig_entity_t *light;
    rig_entity_t *play_camera;
    rut_object_t *play_camera_component;

    uint8_t *dso_data;
    int dso_len;

    c_list_t code_modules;

    c_llist_t *suspended_controllers;
    bool suspended;
};

rig_ui_t *rig_ui_new(rig_engine_t *engine);

void rig_ui_set_dso_data(rig_ui_t *ui, uint8_t *data, int len);

void rig_ui_prepare(rig_ui_t *ui);

void rig_ui_suspend(rig_ui_t *ui);

void rig_ui_resume(rig_ui_t *ui);

rig_entity_t *rig_ui_find_entity(rig_ui_t *ui, const char *label);

void rig_ui_reap(rig_ui_t *ui);

void rig_ui_add_controller(rig_ui_t *ui, rig_controller_t *controller);

void rig_ui_remove_controller(rig_ui_t *ui, rig_controller_t *controller);

void rig_ui_print(rig_ui_t *ui);

void rig_ui_entity_component_added_notify(rig_ui_t *ui,
                                          rig_entity_t *entity,
                                          rut_component_t *component);
void rig_ui_entity_component_pre_remove_notify(rig_ui_t *ui,
                                               rig_entity_t *entity,
                                               rut_component_t *component);
void rig_ui_register_entity(rig_ui_t *ui, rig_entity_t *entity);

void rig_ui_code_modules_load(rig_ui_t *ui);
void rig_ui_code_modules_update(rig_ui_t *ui);
void rig_ui_code_modules_handle_input(rig_ui_t *ui, rut_input_event_t *event);

#endif /* _RIG_UI_H_ */
