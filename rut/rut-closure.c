/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012,2015 Intel Corporation.
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

#include "rut-closure.h"

void rut_closure_list_add(c_list_t *list, rut_closure_t *closure)
{
    if (closure->list_node.next != NULL) {
        c_warn_if_fail(closure->owner == list);
        return;
    }

    c_list_insert(list->prev, &closure->list_node);

#ifdef C_DEBUG
    closure->owner = list;
#endif
}

void rut_closure_remove(rut_closure_t *closure)
{
    if (closure->list_node.next) {
        c_list_remove(&closure->list_node);

        if (closure->removed_cb)
            closure->removed_cb(closure->user_data);
    }

    c_warn_if_fail(closure->used_add_FIXME == false);
}

void rut_closure_list_remove_all(c_list_t *list)
{
    while (!c_list_empty(list)) {
        rut_closure_t *closure =
            c_container_of(list->next, rut_closure_t, list_node);
        rut_closure_remove(closure);
    }
}


/*
 * XXX: Deprecated apis...
 */

void
rut_closure_disconnect_FIXME(rut_closure_t *closure)
{
#ifdef C_DEBUG
    c_return_if_fail(closure->used_add_FIXME);
#endif

    c_list_remove(&closure->list_node);

    if (closure->removed_cb)
        closure->removed_cb(closure->user_data);

    rut_closure_free(closure);
}

void
rut_closure_list_disconnect_all_FIXME(c_list_t *list)
{
    while (!c_list_empty(list)) {
        rut_closure_t *closure =
            c_container_of(list->next, rut_closure_t, list_node);
        rut_closure_disconnect_FIXME(closure);
    }
}

rut_closure_t *
rut_closure_list_add_FIXME(c_list_t *list,
                           void *function,
                           void *user_data,
                           rut_closure_destroy_callback_t destroy_cb)
{
    rut_closure_t *closure = rut_closure_alloc(function, user_data);

    if (destroy_cb)
        rut_closure_set_finalize(closure, destroy_cb);

    rut_closure_list_add(list, closure);

#ifdef C_DEBUG
    closure->used_add_FIXME = true;
#endif

    return closure;
}
