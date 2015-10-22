/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012,2013 Intel Corporation.
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

#include <cglib-config.h>

#include <clib.h>

#include "cg-closure-list-private.h"

void
_cg_closure_disconnect(cg_closure_t *closure)
{
    c_list_remove(&closure->link);

    if (closure->destroy_cb)
        closure->destroy_cb(closure->user_data);

    c_slice_free(cg_closure_t, closure);
}

void
_cg_closure_list_disconnect_all(c_list_t *list)
{
    cg_closure_t *closure, *next;

    c_list_for_each_safe(closure, next, list, link)
    _cg_closure_disconnect(closure);
}

cg_closure_t *
_cg_closure_list_add(c_list_t *list,
                     void *function,
                     void *user_data,
                     cg_user_data_destroy_callback_t destroy_cb)
{
    cg_closure_t *closure = c_slice_new(cg_closure_t);

    closure->function = function;
    closure->user_data = user_data;
    closure->destroy_cb = destroy_cb;

    c_list_insert(list, &closure->link);

    return closure;
}
