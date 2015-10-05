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

#ifndef _RUT_EXCEPTION_H_
#define _RUT_EXCEPTION_H_

#include <stdbool.h>

#include <clib.h>

/* XXX: hack; these aren't GQuarks and aren't extensible... */
typedef enum _rut_exception_domain_t {
    RUT_IO_EXCEPTION = 1,
    RUT_ADB_EXCEPTION,
    RUT_N_EXCEPTION_DOMAINS
} rut_exception_domain_t;

typedef struct _rut_exception_t {
    c_quark_t domain;
    int code;
    char *message;
    c_backtrace_t *backtrace;
} rut_exception_t;

void
rut_throw(rut_exception_t **err, int klass, int code, const char *format, ...);

bool rut_catch(const rut_exception_t *error, int klass, int code);

void rut_propagate_exception(rut_exception_t **dest, rut_exception_t *src);

void rut_exception_free(rut_exception_t *error);

#endif /* _RUT_EXCEPTION_H_ */
