/*
 * Rut
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

#ifndef _RUT_BOX_LAYOUT_H_
#define _RUT_BOX_LAYOUT_H_

#include "rut.h"

extern RutType rut_box_layout_type;

typedef struct _RutBoxLayout RutBoxLayout;

#define RUT_BOX_LAYOUT(x) ((RutBoxLayout *) x)

typedef enum
{
  RUT_BOX_LAYOUT_ORIENTATION_HORIZONTAL,
  RUT_BOX_LAYOUT_ORIENTATION_VERTICAL
} RutBoxLayoutOrientation;

RutBoxLayout *
rut_box_layout_new (RutContext *ctx,
                    RutBoxLayoutOrientation orientation);

void
rut_box_layout_add (RutBoxLayout *box,
                    RutObject *child);

void
rut_box_layout_remove (RutBoxLayout *box,
                       RutObject *child);

#endif /* _RUT_BOX_LAYOUT_H_ */
