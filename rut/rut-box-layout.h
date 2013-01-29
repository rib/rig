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
  RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT,
  RUT_BOX_LAYOUT_PACKING_RIGHT_TO_LEFT,
  RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM,
  RUT_BOX_LAYOUT_PACKING_BOTTOM_TO_TOP
} RutBoxLayoutPacking;

RutBoxLayout *
rut_box_layout_new (RutContext *ctx,
                    RutBoxLayoutPacking packing,
                    ...);

void
rut_box_layout_add (RutBoxLayout *box,
                    CoglBool expand,
                    RutObject *child);

void
rut_box_layout_remove (RutBoxLayout *box,
                       RutObject *child);

CoglBool
rut_box_layout_get_homogeneous (RutObject *obj);

void
rut_box_layout_set_homogeneous (RutObject *obj,
                                CoglBool homogeneous);

int
rut_box_layout_get_spacing (RutObject *obj);

void
rut_box_layout_set_spacing (RutObject *obj,
                            int spacing);

int
rut_box_layout_get_packing (RutObject *obj);

void
rut_box_layout_set_packing (RutObject *obj,
                            RutBoxLayoutPacking packing);

#endif /* _RUT_BOX_LAYOUT_H_ */
