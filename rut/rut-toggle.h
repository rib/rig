/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef _RUT_TOGGLE_H_
#define _RUT_TOGGLE_H_

#include "rut-context.h"
#include "rut-types.h"

extern RutType rut_toggle_type;

typedef struct _RutToggle RutToggle;

RutToggle *
rut_toggle_new (RutContext *ctx,
                const char *label);

RutToggle *
rut_toggle_new_with_icons (RutContext *ctx,
                           const char *unselected_icon,
                           const char *selected_icon,
                           const char *label);

typedef void (*RutToggleCallback) (RutToggle *toggle,
                                   bool value,
                                   void *user_data);

RutClosure *
rut_toggle_add_on_toggle_callback (RutToggle *toggle,
                                   RutToggleCallback callback,
                                   void *user_data,
                                   RutClosureDestroyCallback destroy_cb);

void
rut_toggle_set_enabled (RutObject *toggle,
                        bool enabled);

void
rut_toggle_set_state (RutObject *toggle,
                      bool state);

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
rut_toggle_set_tick (RutObject *toggle,
                     const char *tick);

const char *
rut_toggle_get_tick (RutObject *toggle);

/**
 * rut_toggle_set_tick_color:
 * @toggle: A #RutToggle
 * @color: The new color
 *
 * Sets the color that will be used to display the tick character.
 * This defaults to black
 */
void
rut_toggle_set_tick_color (RutObject *toggle,
                           const CoglColor *color);

const CoglColor *
rut_toggle_get_tick_color (RutObject *toggle);

#endif /* _RUT_TOGGLE_H_ */
