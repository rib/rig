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

#ifndef _CG_CLOSURE_LIST_PRIVATE_H_
#define _CG_CLOSURE_LIST_PRIVATE_H_

#include <clib.h>

#include "cg-object.h"

/*
 * This implements a list of callbacks that can be used a bit like
 * signals in GObject, but that don't have any marshalling overhead.
 *
 * The idea is that any CGlib code that wants to provide a callback
 * point will provide api to add a callback for that particular point.
 * The function can take a function pointer with the correct
 * signature.  Internally the CGlib code can use _cg_closure_list_add,
 * _cg_closure_disconnect and _cg_closure_list_disconnect_all
 *
 * In the future we could consider exposing the cg_closure_t type which
 * would allow applications to use _cg_closure_disconnect() directly
 * so we don't need to expose new disconnect apis for each callback
 * point.
 */

typedef struct _cg_closure_t {
    c_list_t link;

    void *function;
    void *user_data;
    cg_user_data_destroy_callback_t destroy_cb;
} cg_closure_t;

/*
 * _cg_closure_disconnect:
 * @closure: A closure connected to a CGlib closure list
 *
 * Removes the given closure from the callback list it is connected to
 * and destroys it. If the closure was created with a destroy function
 * then it will be invoked. */
void _cg_closure_disconnect(cg_closure_t *closure);

void _cg_closure_list_disconnect_all(c_list_t *list);

cg_closure_t *_cg_closure_list_add(c_list_t *list,
                                   void *function,
                                   void *user_data,
                                   cg_user_data_destroy_callback_t destroy_cb);

/*
 * _cg_closure_list_invoke:
 * @list: A pointer to a c_list_t containing cg_closure_ts
 * @cb_type: The name of a typedef for the closure callback function signature
 * @...: The the arguments to pass to the callback
 *
 * A convenience macro to invoke a closure list with a variable number
 * of arguments that will be passed to the closure callback functions.
 *
 * Note that the arguments will be evaluated multiple times so it is
 * not safe to pass expressions that have side-effects.
 *
 * Note also that this function ignores the return value from the
 * callbacks. If you want to handle the return value you should
 * manually iterate the list and invoke the callbacks yourself.
 */
#define _cg_closure_list_invoke(list, cb_type, ...)                            \
    C_STMT_START                                                               \
    {                                                                          \
        cg_closure_t *_c, *_tmp;                                               \
                                                                               \
        c_list_for_each_safe(_c, _tmp, (list), link)                         \
        {                                                                      \
            cb_type _cb = _c->function;                                        \
            _cb(__VA_ARGS__, _c->user_data);                                   \
        }                                                                      \
    }                                                                          \
    C_STMT_END

#define _cg_closure_list_invoke_no_args(list)                                  \
    C_STMT_START                                                               \
    {                                                                          \
        cg_closure_t *_c, *_tmp;                                               \
                                                                               \
        c_list_for_each_safe(_c, _tmp, (list), link)                         \
        {                                                                      \
            void (*_cb)(void *) = _c->function;                                \
            _cb(_c->user_data);                                                \
        }                                                                      \
    }                                                                          \
    C_STMT_END

#endif /* _CG_CLOSURE_LIST_PRIVATE_H_ */
