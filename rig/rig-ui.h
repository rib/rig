/*
 * Rig
 *
 * Copyright (C) 2014  Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef _RIG_UI_H_
#define _RIG_UI_H_

#include <glib.h>

#include <rut.h>

typedef struct _RigUI RigUI;

#include "rut-object.h"
#include "rig-engine.h"
#include "rig-entity.h"

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

  GList *assets;

  RutObject *scene;
  GList *controllers;

  RigEntity *light;
  RigEntity *play_camera;
  RutObject *play_camera_component;

  uint8_t *dso_data;
  int dso_len;

  GList *suspended_controllers;
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

#endif /* _RIG_UI_H_ */
