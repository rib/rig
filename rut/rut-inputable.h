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

#ifndef __RUT_INPUTABLE_H__
#define __RUT_INPUTABLE_H__

#include <cogl/cogl.h>

#include "rut-object.h"

typedef RutInputEventStatus (*RutInputableCallback) (RutObject *inputable,
                                                     RutInputEvent *event);

typedef struct _RutInputableVTable
{
  RutInputableCallback handle_event;
} RutInputableVTable;

static inline RutInputEventStatus
rut_inputable_handle_event (RutObject *inputable,
                            RutInputEvent *event)
{
  RutInputableVTable *vtable =
    rut_object_get_vtable (inputable, RUT_INTERFACE_ID_INPUTABLE);

  return vtable->handle_event (inputable, event);
}

#endif /* __RUT_INPUTABLE_H__ */
