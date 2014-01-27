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

  RutEntity *light;
  RutEntity *play_camera;
  RutCamera *play_camera_component;
};

RigUI *
rig_ui_new (RigEngine *engine);

void
rig_ui_prepare (RigUI *ui);

#endif /* _RIG_UI_H_ */
