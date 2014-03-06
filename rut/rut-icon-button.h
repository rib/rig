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

#ifndef __RUT_ICON_BUTTON_H__
#define __RUT_ICON_BUTTON_H__

typedef struct _RutIconButton RutIconButton;
extern RutType rut_icon_button_type;

typedef enum
{
  RUT_ICON_BUTTON_POSITION_ABOVE,
  RUT_ICON_BUTTON_POSITION_BELOW,
  RUT_ICON_BUTTON_POSITION_SIDE,
} RutIconButtonPosition;

RutIconButton *
rut_icon_button_new (RutContext *ctx,
                     const char *label,
                     RutIconButtonPosition label_position,
                     const char *normal_icon,
                     const char *hover_icon,
                     const char *active_icon,
                     const char *disable_icon);

typedef void (*RutIconButtonClickCallback) (RutIconButton *button, void *user_data);

RutClosure *
rut_icon_button_add_on_click_callback (RutIconButton *button,
                                  RutIconButtonClickCallback callback,
                                  void *user_data,
                                  RutClosureDestroyCallback destroy_cb);

void
rut_icon_button_set_normal (RutIconButton *button,
                            const char *icon);

void
rut_icon_button_set_hover (RutIconButton *button,
                           const char *icon);

void
rut_icon_button_set_active (RutIconButton *button,
                            const char *icon);

void
rut_icon_button_set_disabled (RutIconButton *button,
                              const char *icon);

void
rut_icon_button_set_label_color (RutIconButton *button,
                                 const CoglColor *color);

#endif /* __RUT_ICON_BUTTON_H__ */
