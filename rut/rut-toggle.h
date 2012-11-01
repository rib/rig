/*
 * Rut
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

#ifndef _RUT_TOGGLE_H_
#define _RUT_TOGGLE_H_

#include "rut-context.h"
#include "rut-types.h"

extern RutType rut_toggle_type;

typedef struct _RutToggle RutToggle;

#define RUT_TOGGLE(x) ((RutToggle *) x)

RutToggle *
rut_toggle_new (RutContext *ctx,
                const char *label);

RutToggle *
rut_toggle_new_with_icons (RutContext *ctx,
                           const char *unselected_icon,
                           const char *selected_icon,
                           const char *label);

typedef void (*RutToggleCallback) (RutToggle *toggle,
                                   CoglBool value,
                                   void *user_data);

RutClosure *
rut_toggle_add_on_toggle_callback (RutToggle *toggle,
                                   RutToggleCallback callback,
                                   void *user_data,
                                   RutClosureDestroyCallback destroy_cb);

void
rut_toggle_set_enabled (RutToggle *toggle,
                        CoglBool enabled);

void
rut_toggle_set_state (RutToggle *toggle,
                      CoglBool state);

RutProperty *
rut_toggle_get_enabled_property (RutToggle *toggle);

/**
 * rut_toggle_set_tick:
 * @toggle: A #RutToggle
 * @tick: The new string to display for the tick
 *
 * Sets the string used to display the tick character. This defaults to ‘✔’.
 */
void
rut_toggle_set_tick (RutToggle *toggle,
                     const char *tick);

const char *
rut_toggle_get_tick (RutToggle *toggle);

/**
 * rut_toggle_set_tick_color:
 * @toggle: A #RutToggle
 * @color: The new color
 *
 * Sets the color that will be used to display the tick character.
 * This defaults to black
 */
void
rut_toggle_set_tick_color (RutToggle *toggle,
                           const CoglColor *color);

void
rut_toggle_get_tick_color (RutToggle *toggle,
                           CoglColor *color);

#endif /* _RUT_TOGGLE_H_ */
