/*
 * Rut
 *
 * Copyright (C) 2013  Intel Corporation
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef _RIG_EDITOR_H_
#define _RIG_EDITOR_H_

#include <rut.h>

#include "rig-types.h"

#include "rig.pb-c.h"

typedef struct _RigEditor RigEditor;

extern RutType rig_editor_type;

RigEditor *
rig_editor_new (const char *filename);

void
rig_editor_run (RigEditor *editor);

void
rig_editor_load_file (RigEditor *editor,
                      const char *filename);

/* FIXME: move necessary state to RigEditor and update this
 * api to take a RigEditor pointer */
void
rig_editor_create_ui (RigEngine *engine);

void
rig_editor_apply_last_op (RigEngine *engine);

/* XXX: This rather esoteric prototype is used as a 'read_callback' to
 * rig_asset_thumbnail and is called whenever an asset's thumnail has
 * been updated.
 *
 * It would probably be better to just have a
 * rig_editor_reload_thumbnails(RigEditor *editor) considering that
 * all this function does is trigger an asset search to refresh the
 * assets view.
 */
void
rig_editor_refresh_thumbnails (RigAsset *video, void *user_data);

/* FIXME: move necessary state to RigEditor and update this
 * api to take a RigEditor pointer */
void
rig_editor_clear_search_results (RigEngine *engine);

void
rig_editor_free_result_input_closures (RigEngine *engine);

#endif /* _RIG_EDITOR_H_ */
