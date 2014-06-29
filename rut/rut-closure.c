/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012 Intel Corporation.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <clib.h>

#include "rut-closure.h"

void
rut_closure_disconnect(rut_closure_t *closure)
{
    rut_list_remove(&closure->list_node);

    if (closure->destroy_cb)
        closure->destroy_cb(closure->user_data);

    c_slice_free(rut_closure_t, closure);
}

void
rut_closure_list_disconnect_all(rut_list_t *list)
{
    while (!rut_list_empty(list)) {
        rut_closure_t *closure =
            rut_container_of(list->next, closure, list_node);
        rut_closure_disconnect(closure);
    }
}

rut_closure_t *
rut_closure_list_add(rut_list_t *list,
                     void *function,
                     void *user_data,
                     rut_closure_destroy_callback_t destroy_cb)
{
    rut_closure_t *closure = c_slice_new(rut_closure_t);

    closure->function = function;
    closure->user_data = user_data;
    closure->destroy_cb = destroy_cb;

    rut_list_insert(list->prev, &closure->list_node);

    return closure;
}
