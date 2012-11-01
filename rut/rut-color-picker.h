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

#ifndef _RUT_COLOR_PICKER_H_
#define _RUT_COLOR_PICKER_H_

#include "rut.h"

extern RutType rut_color_picker_type;

typedef struct _RutColorPicker RutColorPicker;

#define RUT_COLOR_PICKER(x) ((RutColorPicker *) x)

RutColorPicker *
rut_color_picker_new (RutContext *ctx);

void
rut_color_picker_set_color (RutObject *picker,
                            const CoglColor *color);

const CoglColor *
rut_color_picker_get_color (RutColorPicker *picker);

#endif /* _RUT_COLOR_PICKER_H_ */
