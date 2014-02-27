/*
 * Rut.
 *
 * Copyright (C) 2013  Intel Corporation.
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

#ifndef __RIG_BUTTON_INPUT_H__
#define __RIG_BUTTON_INPUT_H__

#include <cogl/cogl.h>

#include <rut-context.h>

typedef struct _RigButtonInput RigButtonInput;
extern RutType rig_button_input_type;

RigButtonInput *
rig_button_input_new (RutContext *ctx);

#endif /* __RIG_BUTTON_INPUT_H__ */
