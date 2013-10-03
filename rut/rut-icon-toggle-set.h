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

#ifndef __RUT_ICON_TOGGLE_SET_H__
#define __RUT_ICON_TOGGLE_SET_H__

typedef struct _RutIconToggleSet RutIconToggleSet;
extern RutType rut_icon_toggle_set_type;

typedef enum _RutIconToggleSetPacking
{
  RUT_ICON_TOGGLE_SET_PACKING_LEFT_TO_RIGHT,
  RUT_ICON_TOGGLE_SET_PACKING_RIGHT_TO_LEFT,
  RUT_ICON_TOGGLE_SET_PACKING_TOP_TO_BOTTOM,
  RUT_ICON_TOGGLE_SET_PACKING_BOTTOM_TO_TOP
} RutIconToggleSetPacking;

RutIconToggleSet *
rut_icon_toggle_set_new (RutContext *ctx,
                         RutIconToggleSetPacking packing);

typedef void (*RutIconToggleSetChangedCallback) (RutIconToggleSet *toggle_set,
                                                 int selection_value,
                                                 void *user_data);

RutClosure *
rut_icon_toggle_set_add_on_change_callback (RutIconToggleSet *toggle_set,
                                  RutIconToggleSetChangedCallback callback,
                                  void *user_data,
                                  RutClosureDestroyCallback destroy_cb);

void
rut_icon_toggle_set_add (RutIconToggleSet *toggle_set,
                         RutIconToggle *toggle,
                         int selection_value);

int
rut_icon_toggle_set_get_selection (RutObject *toggle_set);

void
rut_icon_toggle_set_set_selection (RutObject *toggle_set,
                                   int selection_value);

#endif /* __RUT_ICON_TOGGLE_SET_H__ */
