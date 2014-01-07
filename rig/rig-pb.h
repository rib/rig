/*
 * Rig
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

#ifndef __RIG_PB_H__
#define __RIG_PB_H__

#include <rut.h>

#include "rig-engine.h"
#include "rig.pb-c.h"

typedef struct _RigPBSerializer RigPBSerializer;
typedef struct _RigPBUnSerializer RigPBUnSerializer;

typedef void (*RigAssetReferenceCallback) (RutAsset *asset,
                                           void *user_data);

RigPBSerializer *
rig_pb_serializer_new (RigEngine *engine);

typedef bool (*RigPBAssetFilter) (RutAsset *asset, void *user_data);

void
rig_pb_serializer_set_asset_filter (RigPBSerializer *serializer,
                                    RigPBAssetFilter filter,
                                    void *user_data);

void
rig_pb_serializer_destroy (RigPBSerializer *serializer);

Rig__UI *
rig_pb_serialize_ui (RigPBSerializer *serializer);

void
rig_pb_serialized_ui_destroy (Rig__UI *ui);

Rig__Event **
rig_pb_serialize_input_events (RigEngine *engine,
                               RutList *input_queue,
                               int n_events);

RigPBUnSerializer *
rig_pb_unserializer_new (RigEngine *engine);

void
rig_pb_unserializer_destroy (RigPBUnSerializer *unserializer);

void
rig_pb_unserialize_ui (RigPBUnSerializer *unserializer,
                       const Rig__UI *pb_ui,
                       bool skip_assets);

RutMesh *
rig_pb_unserialize_mesh (RigPBUnSerializer *unserializer,
                         Rig__Mesh *pb_mesh);

#endif /* __RIG_PB_H__ */
