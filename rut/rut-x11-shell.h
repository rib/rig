/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2014 Intel Corporation.
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

#pragma once

#include "rut-shell.h"

typedef struct _rut_x11_event_t {
    XGenericEventCookie xcookie;

#if 0
    /* We track keyboard state via libxkbcommon's struct xkb_state api
     * but since we currently update the keyboard state immediatly
     * when recieve events, not synchronised with the deferred
     * processing of events we query the modifier state up-front and
     * save it in the event for processing later...
     */
#endif
    /* Just to avoid repeatedly calling into libxkbcommon and also
     * iterating the modifiers to map from xkb modifier indices to
     * rut_modifier_state_t bits we just resolve the modifiers once...
     */
    rut_modifier_state_t mod_state;

    xkb_keysym_t keysym;

    char *text;
} rut_x11_event_t;

#if 0
typedef void (*rut_x11_event_handler_t)(rut_shell_t *shell,
                                        XEvent *xevent,
                                        void *user_data);
#endif

bool
rut_x11_shell_init(rut_shell_t *shell);

void
rut_x11_shell_handle_x11_event(rut_shell_t *shell, XEvent *xevent);
