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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _RUT_ASSET_INSPECTOR_H_
#define _RUT_ASSET_INSPECTOR_H_

#include "rut-object.h"

extern RutType rut_asset_inspector_type;

typedef struct _RutAssetInspector RutAssetInspector;

RutAssetInspector *
rut_asset_inspector_new (RutContext *ctx, RutAssetType asset_type);

RutObject *
rut_asset_inspector_get_asset (RutObject *object);

void
rut_asset_inspector_set_asset (RutObject *object, RutObject *asset_object);

#endif /* _RUT_ASSET_INSPECTOR_H_ */
