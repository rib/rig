/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012, 2013 Intel Corporation.
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

#include <rut-config.h>

#include <clib.h>

#include <cglib/cglib.h>
#ifdef USE_SDL
#include <cglib/cg-sdl.h>
#endif

#include "rut-bitmask.h"
#include "rut-shell.h"
#include "rut-transform-private.h"
//#include "rut-text.h"
#include "rut-geometry.h"

uint8_t _rut_nine_slice_indices_data[] = {
    0, 4,  5,  0, 5,  1, 1, 5,  6,  1, 6,  2,  2,  6,  7,  2,  7,  3,
    4, 8,  9,  4, 9,  5, 5, 9,  10, 5, 10, 6,  6,  10, 11, 6,  11, 7,
    8, 12, 13, 8, 13, 9, 9, 13, 14, 9, 14, 10, 10, 14, 15, 10, 15, 11
};

typedef struct _settings_changed_callback_state_t {
    rut_settings_changed_callback_t callback;
    c_destroy_func_t destroy_notify;
    void *user_data;
} settings_changed_callback_state_t;

struct _rut_settings_t {
    c_llist_t *changed_callbacks;
};

void
rut_settings_destroy(rut_settings_t *settings)
{
    c_llist_t *l;

    for (l = settings->changed_callbacks; l; l = l->next)
        c_slice_free(settings_changed_callback_state_t, l->data);
    c_llist_free(settings->changed_callbacks);
}

rut_settings_t *
rut_settings_new(void)
{
    return c_slice_new0(rut_settings_t);
}

void
rut_settings_add_changed_callback(rut_settings_t *settings,
                                  rut_settings_changed_callback_t callback,
                                  c_destroy_func_t destroy_notify,
                                  void *user_data)
{
    c_llist_t *l;
    settings_changed_callback_state_t *state;

    for (l = settings->changed_callbacks; l; l = l->next) {
        state = l->data;

        if (state->callback == callback) {
            state->user_data = user_data;
            state->destroy_notify = destroy_notify;
            return;
        }
    }

    state = c_slice_new(settings_changed_callback_state_t);
    state->callback = callback;
    state->destroy_notify = destroy_notify;
    state->user_data = user_data;

    settings->changed_callbacks =
        c_llist_prepend(settings->changed_callbacks, state);
}

void
rut_settings_remove_changed_callback(rut_settings_t *settings,
                                     rut_settings_changed_callback_t callback)
{
    c_llist_t *l;

    for (l = settings->changed_callbacks; l; l = l->next) {
        settings_changed_callback_state_t *state = l->data;

        if (state->callback == callback) {
            settings->changed_callbacks =
                c_llist_delete_link(settings->changed_callbacks, l);
            c_slice_free(settings_changed_callback_state_t, state);
            return;
        }
    }
}

/* FIXME HACK */
unsigned int
rut_settings_get_password_hint_time(rut_settings_t *settings)
{
    return 10;
}

char *
rut_settings_get_font_name(rut_settings_t *settings)
{
    return c_strdup("Sans 12");
}
