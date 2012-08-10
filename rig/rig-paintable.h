/*
 * Rig
 *
 * A tiny toolkit
 *
 * Copyright (C) 2012 Intel Corporation.
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
 *
 */

#ifndef _RIG_PAINTABLE_H_
#define _RIG_PAINTABLE_H_

#include "rig-object.h"
#include "rig-types.h"

typedef struct _RigPaintableProps
{
  int padding;
} RigPaintableProps;

typedef struct _RigPaintContext
{
  RigCamera *camera;
} RigPaintContext;

#define RIG_PAINT_CONTEXT(X) ((RigPaintContext *)X)

typedef struct _RigPaintableVtable
{
  void (*paint) (RigObject *object, RigPaintContext *paint_ctx);
} RigPaintableVTable;

void
rig_paintable_init (RigObject *object);

void
rig_paintable_paint (RigObject *object, RigPaintContext *paint_ctx);

#endif /* _RIG_PAINTABLE_H_ */
