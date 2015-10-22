/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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

#ifndef __CG_PROFILE_H__
#define __CG_PROFILE_H__

#ifdef ENABLE_PROFILE

#include <uprof.h>

extern UProfContext *_cg_uprof_context;

#define CG_STATIC_TIMER UPROF_STATIC_TIMER
#define CG_STATIC_COUNTER UPROF_STATIC_COUNTER
#define CG_COUNTER_INC UPROF_COUNTER_INC
#define CG_COUNTER_DEC UPROF_COUNTER_DEC
#define CG_TIMER_START UPROF_TIMER_START
#define CG_TIMER_STOP UPROF_TIMER_STOP

void _cg_uprof_init(void);

void _cg_profile_trace_message(const char *format, ...);

#else

#define CG_STATIC_TIMER(A, B, C, D, E) extern void _cg_dummy_decl(void)
#define CG_STATIC_COUNTER(A, B, C, D) extern void _cg_dummy_decl(void)
#define CG_COUNTER_INC(A, B)                                                   \
    C_STMT_START                                                               \
    {                                                                          \
        (void)0;                                                               \
    }                                                                          \
    C_STMT_END
#define CG_COUNTER_DEC(A, B)                                                   \
    C_STMT_START                                                               \
    {                                                                          \
        (void)0;                                                               \
    }                                                                          \
    C_STMT_END
#define CG_TIMER_START(A, B)                                                   \
    C_STMT_START                                                               \
    {                                                                          \
        (void)0;                                                               \
    }                                                                          \
    C_STMT_END
#define CG_TIMER_STOP(A, B)                                                    \
    C_STMT_START                                                               \
    {                                                                          \
        (void)0;                                                               \
    }                                                                          \
    C_STMT_END

#define _cg_profile_trace_message c_message

#endif

#endif /* __CG_PROFILE_H__ */
