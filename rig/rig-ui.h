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

typedef struct _RigUI RigUI;

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
struct _RigUI
{
  RutObjectBase _base;

  RigEngine *engine;

  c_list_t *assets;

  RutObject *scene;
  c_list_t *controllers;

  RigEntity *light;
  RigEntity *play_camera;
  RutObject *play_camera_component;

  uint8_t *dso_data;
  int dso_len;

  c_list_t *suspended_controllers;
  bool suspended;
};

RigUI *
rig_ui_new (RigEngine *engine);

void
rig_ui_set_dso_data (RigUI *ui, uint8_t *data, int len);

void
rig_ui_prepare (RigUI *ui);

void
rig_ui_suspend (RigUI *ui);

void
rig_ui_resume (RigUI *ui);

RigEntity *
rig_ui_find_entity (RigUI *ui, const char *label);

void
rig_ui_reap (RigUI *ui);

void
rig_ui_add_controller (RigUI *ui,
                       RigController *controller);

void
rig_ui_remove_controller (RigUI *ui,
                          RigController *controller);

#endif /* _RIG_UI_H_ */
