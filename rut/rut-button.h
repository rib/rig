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

#ifndef __RUT_BUTTON_H__
#define __RUT_BUTTON_H__

typedef struct _rut_button_t rut_button_t;
extern rut_type_t rut_button_type;

rut_button_t *rut_button_new(rut_shell_t *shell, const char *label);

typedef void (*rut_button_click_callback_t)(rut_button_t *button,
                                            void *user_data);

rut_closure_t *
rut_button_add_on_click_callback(rut_button_t *button,
                                 rut_button_click_callback_t callback,
                                 void *user_data,
                                 rut_closure_destroy_callback_t destroy_cb);

void rut_button_set_size(rut_object_t *self, float width, float height);

void rut_button_get_size(rut_object_t *self, float *width, float *height);

#endif /* __RUT_BUTTON_H__ */
