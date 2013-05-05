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

#ifndef _RUT_MIMABLE_H_
#define _RUT_MIMABLE_H_

#include <stdbool.h>
#include "rut-object.h"

typedef enum _RutMimableType
{
  RUT_MIMABLE_TYPE_TEXT,
  RUT_MIMABLE_TYPE_OBJECT
} RutMimableType;

typedef struct _RutMimableVTable
{
  RutObject *(*copy) (RutObject *mimable);
  bool (*has) (RutObject *mimable, RutMimableType type);
  void *(*get) (RutObject *mimable, RutMimableType type);
} RutMimableVTable;

char *
rut_mimable_get_text (RutObject *object);

bool
rut_mimable_has_text (RutObject *object);

#endif /* _RUT_MIMABLE_H_ */
