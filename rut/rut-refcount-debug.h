/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013 Intel Corporation.
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

#ifndef _RUT_REFCOUNT_DEBUG_H_
#define _RUT_REFCOUNT_DEBUG_H_

#include <clib.h>

C_BEGIN_DECLS

#ifdef RUT_ENABLE_REFCOUNT_DEBUG

void rut_refcount_debug_init(void);

void _rut_refcount_debug_object_created(void *object);

void _rut_refcount_debug_ref(void *object);

void _rut_refcount_debug_claim(void *object, void *owner);

void _rut_refcount_debug_unref(void *object);

void _rut_refcount_debug_release(void *object, void *owner);

#else /* RUT_ENABLE_REFCOUNT_DEBUG */

/* If recounting isn't enabled then we'll just no-op the debugging
 * code so that it won't be a performance burden */

#define _rut_refcount_debug_object_created(o)                                  \
    C_STMT_START                                                               \
    {                                                                          \
    }                                                                          \
    C_STMT_END

#define _rut_refcount_debug_ref(o)                                             \
    C_STMT_START                                                               \
    {                                                                          \
    }                                                                          \
    C_STMT_END
#define _rut_refcount_debug_claim(o, owner)                                    \
    C_STMT_START                                                               \
    {                                                                          \
    }                                                                          \
    C_STMT_END

#define _rut_refcount_debug_unref(o)                                           \
    C_STMT_START                                                               \
    {                                                                          \
    }                                                                          \
    C_STMT_END
#define _rut_refcount_debug_release(o, owner)                                  \
    C_STMT_START                                                               \
    {                                                                          \
    }                                                                          \
    C_STMT_END

#endif /* RUT_ENABLE_REFCOUNT_DEBUG */

C_END_DECLS

#endif /* _RUT_REFCOUNT_DEBUG_H_ */
