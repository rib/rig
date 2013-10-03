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
