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

#ifndef _RIG_TOGGLE_H_
#define _RIG_TOGGLE_H_

#include "rig-context.h"
#include "rig-types.h"

extern RigType rig_toggle_type;

typedef struct _RigToggle RigToggle;

#define RIG_TOGGLE(x) ((RigToggle *) x)

RigToggle *
rig_toggle_new (RigContext *ctx,
                const char *label);

typedef void (*RigToggleCallback) (RigToggle *toggle,
                                   CoglBool value,
                                   void *user_data);

RigClosure *
rig_toggle_add_on_toggle_callback (RigToggle *toggle,
                                   RigToggleCallback callback,
                                   void *user_data,
                                   RigClosureDestroyCallback destroy_cb);

void
rig_toggle_set_enabled (RigToggle *toggle,
                        CoglBool enabled);

void
rig_toggle_set_state (RigToggle *toggle,
                      CoglBool state);

RigProperty *
rig_toggle_get_enabled_property (RigToggle *toggle);

/**
 * rig_toggle_set_tick:
 * @toggle: A #RigToggle
 * @tick: The new string to display for the tick
 *
 * Sets the string used to display the tick character. This defaults to ‘✔’.
 */
void
rig_toggle_set_tick (RigToggle *toggle,
                     const char *tick);

const char *
rig_toggle_get_tick (RigToggle *toggle);

/**
 * rig_toggle_set_tick_color:
 * @toggle: A #RigToggle
 * @color: The new color
 *
 * Sets the color that will be used to display the tick character.
 * This defaults to black
 */
void
rig_toggle_set_tick_color (RigToggle *toggle,
                           const RigColor *color);

void
rig_toggle_get_tick_color (RigToggle *toggle,
                           RigColor *color);

#endif /* _RIG_TOGGLE_H_ */
