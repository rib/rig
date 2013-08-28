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

#ifndef _RUT_DRAG_BIN_H_
#define _RUT_DRAG_BIN_H_

#include "rut.h"

extern RutType rut_drag_bin_type;

typedef struct _RutDragBin RutDragBin;

RutDragBin *
rut_drag_bin_new (RutContext *ctx);

void
rut_drag_bin_set_child (RutDragBin *bin,
                        RutObject *child);

void
rut_drag_bin_set_payload (RutDragBin *bin,
                          RutObject *payload);

#endif /* _RUT_DRAG_BIN_H_ */
