/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013,2014  Intel Corporation.
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
 */

#ifndef _RUT_SETTINGS_H_
#define _RUT_SETTINGS_H_

#include "rut-object.h"
#include "rut-shell.h"
#include "rut-property.h"
#include "rut-closure.h"

extern uint8_t _rut_nine_slice_indices_data[54];

/*
 * Note: The size and padding for this circle texture have been
 * carefully chosen so it has a power of two size and we have enough
 * padding to scale down the circle to a size of 2 pixels and still
 * have a 1 texel transparent border which we rely on for
 * anti-aliasing.
 */
#define CIRCLE_TEX_RADIUS 256
#define CIRCLE_TEX_PADDING 256

typedef enum {
    RUT_TEXT_DIRECTION_LEFT_TO_RIGHT = 1,
    RUT_TEXT_DIRECTION_RIGHT_TO_LEFT
} rut_text_direction_t;

typedef struct _rut_settings_t rut_settings_t;

C_BEGIN_DECLS

rut_settings_t *rut_settings_new(void);

typedef void (*rut_settings_changed_callback_t)(rut_settings_t *settings,
                                                void *user_data);

void rut_settings_add_changed_callback(rut_settings_t *settings,
                                       rut_settings_changed_callback_t callback,
                                       c_destroy_func_t destroy_notify,
                                       void *user_data);

void
rut_settings_remove_changed_callback(rut_settings_t *settings,
                                     rut_settings_changed_callback_t callback);

unsigned int rut_settings_get_password_hint_time(rut_settings_t *settings);

char *rut_settings_get_font_name(rut_settings_t *settings);

void rut_settings_destroy(rut_settings_t *settings);

C_END_DECLS

#endif /* _RUT_SETTINGS_H_ */
