/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013  Intel Corporation.
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

#ifndef __RUT_ICON_TOGGLE_H__
#define __RUT_ICON_TOGGLE_H__

typedef struct _RutIconToggle RutIconToggle;
extern RutType rut_icon_toggle_type;

RutIconToggle *
rut_icon_toggle_new (RutContext *ctx,
                     const char *set_icon,
                     const char *unset_icon);

typedef void (*RutIconToggleCallback) (RutIconToggle *toggle,
                                       bool state,
                                       void *user_data);

RutClosure *
rut_icon_toggle_add_on_toggle_callback (RutIconToggle *toggle,
                                  RutIconToggleCallback callback,
                                  void *user_data,
                                  RutClosureDestroyCallback destroy_cb);

void
rut_icon_toggle_set_set_icon (RutIconToggle *toggle,
                              const char *icon);

void
rut_icon_toggle_set_unset_icon (RutIconToggle *toggle,
                                const char *icon);

void
rut_icon_toggle_set_state (RutObject *toggle,
                           bool state);

/* If a toggle is part of a toggle-set then there should always be one
 * toggle set in the toggle-set and so the only way to unset a toggle
 * is to set another one. This is a simple way for the
 * RutIconToggleSet widget to disable being able to directly unset a
 * toggle... */
void
rut_icon_toggle_set_interactive_unset_enable (RutIconToggle *toggle,
                                              bool enabled);

#endif /* __RUT_ICON_TOGGLE_H__ */
