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

#ifndef _RUT_X11_SHELL_H_
#define _RUT_X11_SHELL_H_

#include <emscripten.h>
#include <html5.h>

#include "rut-shell.h"

typedef struct _rut_emscripten_event_t {
    int em_type;

    union {
        EmscriptenKeyboardEvent key;
        EmscriptenMouseEvent mouse;
    };

    const char *text;
} rut_emscripten_event_t;

#if 0
typedef void (*rut_x11_event_handler_t)(rut_shell_t *shell,
                                        XEvent *xevent,
                                        void *user_data);
#endif

bool
rut_emscripten_shell_init(rut_shell_t *shell);

#endif /* _RUT_X11_SHELL_H_ */
