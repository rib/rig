/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
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
 *
 */

#ifndef __CG_RENDERER_XLIB_PRIVATE_H
#define __CG_RENDERER_XLIB_PRIVATE_H

#include "cg-object-private.h"
#include "cg-x11-renderer-private.h"
#include "cg-device.h"
#include "cg-output.h"

typedef struct _cg_xlib_trap_state_t cg_xlib_trap_state_t;

struct _cg_xlib_trap_state_t {
    /* These values are intended to be internal to
     * _cg_xlib_{un,}trap_errors but they need to be in the header so
     * that the struct can be allocated on the stack */
    int (*old_error_handler)(Display *, XErrorEvent *);
    int trapped_error_code;
    cg_xlib_trap_state_t *old_state;
};

typedef struct _cg_xlib_renderer_t {
    cg_x11_renderer_t _parent;

    Display *xdpy;

    /* Current top of the XError trap state stack. The actual memory for
       these is expected to be allocated on the stack by the caller */
    cg_xlib_trap_state_t *trap_state;

    unsigned long outputs_update_serial;
} cg_xlib_renderer_t;

bool _cg_xlib_renderer_connect(cg_renderer_t *renderer, cg_error_t **error);

void _cg_xlib_renderer_disconnect(cg_renderer_t *renderer);

/*
 * cg_xlib_renderer_trap_errors:
 * @state: A temporary place to store data for the trap.
 *
 * Traps every X error until _cg_xlib_renderer_untrap_errors()
 * called. You should allocate an uninitialised cg_xlib_trap_state_t
 * struct on the stack to pass to this function. The same pointer
 * should later be passed to _cg_xlib_renderer_untrap_errors().
 *
 * Calls to _cg_xlib_renderer_trap_errors() can be nested as long as
 * _cg_xlib_renderer_untrap_errors() is called with the
 * corresponding state pointers in reverse order.
 */
void _cg_xlib_renderer_trap_errors(cg_renderer_t *renderer,
                                   cg_xlib_trap_state_t *state);

/*
 * cg_xlib_renderer_untrap_errors:
 * @state: The state that was passed to _cg_xlib_renderer_trap_errors().
 *
 * Removes the X error trap and returns the current status.
 *
 * Return value: the trapped error code, or 0 for success
 */
int _cg_xlib_renderer_untrap_errors(cg_renderer_t *renderer,
                                    cg_xlib_trap_state_t *state);

cg_xlib_renderer_t *_cg_xlib_renderer_get_data(cg_renderer_t *renderer);

int64_t _cg_xlib_renderer_get_dispatch_timeout(cg_renderer_t *renderer);

cg_output_t *_cg_xlib_renderer_output_for_rectangle(
    cg_renderer_t *renderer, int x, int y, int width, int height);

int _cg_xlib_renderer_get_damage_base(cg_renderer_t *renderer);

#endif /* __CG_RENDERER_XLIB_PRIVATE_H */
