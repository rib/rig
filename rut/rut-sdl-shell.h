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

#ifndef _RUT_SDL_SHELL_H_
#define _RUT_SDL_SHELL_H_

#include "rut-shell.h"

typedef struct _rut_sdl_event_t {
    SDL_Event sdl_event;

    /* SDL uses global state to report keyboard modifier and button states
     * which is a pain if events are being batched before processing them
     * on a per-frame basis since we want to be able to track how this
     * state changes relative to events. */
    SDL_Keymod mod_state;

    /* It could be nice if SDL_MouseButtonEvents had a buttons member
     * that had the full state of buttons as returned by
     * SDL_GetMouseState () */
    uint32_t buttons;
} rut_sdl_event_t;

typedef void (*rut_sdl_event_handler_t)(rut_shell_t *shell,
                                        SDL_Event *event,
                                        void *user_data);

bool
rut_sdl_shell_init(rut_shell_t *shell);

void
rut_sdl_shell_handle_sdl_event(rut_shell_t *shell, SDL_Event *sdl_event);

#endif /* _RUT_SDL_SHELL_H_ */
