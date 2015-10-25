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

#include <rut-config.h>

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <clib.h>

#include "rut-exception.h"

static rut_exception_t *
exception_new_valist(c_quark_t domain, int code, const char *format, va_list ap)
{
    rut_exception_t *err = c_new(rut_exception_t, 1);

    err->domain = domain;
    err->code = code;

    err->message = c_strdup_vprintf(format, ap);

    err->backtrace = c_backtrace_new();

    return err;
}

void
rut_throw(rut_exception_t **err, int domain, int code, const char *format, ...)
{
    va_list args;

    va_start(args, format);

    if (err)
        *err = exception_new_valist(domain, code, format, args);
    else {
        c_backtrace_t *bt;

        c_logv(NULL, C_LOG_DOMAIN, C_LOG_LEVEL_ERROR, format, args);

        bt = c_backtrace_new();
        c_backtrace_log(bt, NULL, C_LOG_DOMAIN, C_LOG_LEVEL_ERROR);
        c_backtrace_free(bt);
    }

    va_end(args);
}

bool
rut_catch(const rut_exception_t *error, int domain, int code)
{
    if (error) {
        if (error->domain == domain && error->code == code)
            return true;
        return false;
    } else
        return false;
}

void
rut_propagate_exception(rut_exception_t **dest, rut_exception_t *src)
{
    if (dest == NULL) {
        if (src)
            rut_exception_free(src);
    } else {
        *dest = src;
    }
}

rut_exception_t *
rut_exception_copy(const rut_exception_t *error)
{
    rut_exception_t *copy = c_new(rut_exception_t, 1);
    copy->domain = error->domain;
    copy->code = error->code;
    copy->message = c_strdup(error->message);
    copy->backtrace = c_backtrace_copy(error->backtrace);
    return copy;
}

void
rut_exception_free(rut_exception_t *error)
{
    c_backtrace_free(error->backtrace);
    c_free(error->message);
    c_free(error);
}
