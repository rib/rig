/*
 * Rig
 *
 * Copyright (C) 2012  Intel Corporation
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

#ifndef _RUT_ICON_H_
#define _RUT_ICON_H_

#include "rut-property.h"

extern RutType rut_icon_type;

typedef struct _RutIcon RutIcon;

#define RUT_ICON(x) ((RutIcon *) x)

RutIcon *
rut_icon_new (RutContext *ctx,
              const char *filename);

#endif /* _RUT_ICON_H_ */
