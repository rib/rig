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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "rig-paintable.h"

void
rig_paintable_init (RigObject *object)
{
#if 0
  RigPaintableProps *props =
    rig_object_get_properties (object, RIG_INTERFACE_ID_PAINTABLE);
#endif
}

void
rig_paintable_paint (RigObject *object, RigPaintContext *paint_ctx)
{
  RigPaintableVTable *paintable =
    rig_object_get_vtable (object, RIG_INTERFACE_ID_PAINTABLE);

  paintable->paint (object, paint_ctx);
}

