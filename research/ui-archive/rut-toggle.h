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

#include "rut-shell.h"
#include "rut-types.h"

extern rut_type_t rut_toggle_type;

typedef struct _rut_toggle_t rut_toggle_t;

rut_toggle_t *rut_toggle_new(rut_shell_t *shell, const char *label);

rut_toggle_t *rut_toggle_new_with_icons(rut_shell_t *shell,
                                        const char *unselected_icon,
                                        const char *selected_icon,
                                        const char *label);

typedef void (*rut_toggle_callback_t)(rut_toggle_t *toggle,
                                      bool value,
                                      void *user_data);

rut_closure_t *
rut_toggle_add_on_toggle_callback(rut_toggle_t *toggle,
                                  rut_toggle_callback_t callback,
                                  void *user_data,
                                  rut_closure_destroy_callback_t destroy_cb);

void rut_toggle_set_enabled(rut_object_t *toggle, bool enabled);

void rut_toggle_set_state(rut_object_t *toggle, bool state);

rut_property_t *rut_toggle_get_enabled_property(rut_toggle_t *toggle);

/**
 * rut_toggle_set_tick:
 * @toggle: A #rut_toggle_t
 * @tick: The new string to display for the tick
 *
 * Sets the string used to display the tick character. This defaults to ‘✔’.
 */
void rut_toggle_set_tick(rut_object_t *toggle, const char *tick);

const char *rut_toggle_get_tick(rut_object_t *toggle);

/**
 * rut_toggle_set_tick_color:
 * @toggle: A #rut_toggle_t
 * @color: The new color
 *
 * Sets the color that will be used to display the tick character.
 * This defaults to black
 */
void rut_toggle_set_tick_color(rut_object_t *toggle, const cg_color_t *color);

const cg_color_t *rut_toggle_get_tick_color(rut_object_t *toggle);

#endif /* _RUT_TOGGLE_H_ */
