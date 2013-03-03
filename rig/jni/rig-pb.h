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

typedef void (*RigAssetReferenceCallback) (RutAsset *asset,
                                           void *user_data);

Rig__UI *
rig_pb_serialize_ui (RigEngine *engine,
                     RigAssetReferenceCallback asset_callback,
                     void *user_data);

typedef struct _RigSerializedAsset
{
  RutObjectProps _parent;
  int ref_count;

  RutAsset *asset;

  Rig__SerializedAsset pb_data;

} RigSerializedAsset;

RigSerializedAsset *
rig_pb_serialize_asset (RutAsset *asset);

void
rig_pb_unserialize_ui (RigEngine *engine, const Rig__UI *pb_ui);

#endif /* __RIG_PB_H__ */
