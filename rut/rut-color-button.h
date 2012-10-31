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

#ifndef _RUT_COLOR_BUTTON_H_
#define _RUT_COLOR_BUTTON_H_

#include "rut.h"

extern RutType rut_color_button_type;

typedef struct _RutColorButton RutColorButton;

#define RUT_COLOR_BUTTON(x) ((RutColorButton *) x)

RutColorButton *
rut_color_button_new (RutContext *ctx);

void
rut_color_button_set_color (RutColorButton *picker,
                            const CoglColor *color);

const CoglColor *
rut_color_button_get_color (RutColorButton *picker);

#endif /* _RUT_COLOR_BUTTON_H_ */
