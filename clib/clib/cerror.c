/*
 * Error support.
 *
 * Author:
 *   Miguel de Icaza (miguel@novell.com)
 *
 * (C) 2006 Novell, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <clib-config.h>

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <clib.h>

c_error_t *
c_error_new(c_quark_t domain, int code, const char *format, ...)
{
    va_list args;
    c_error_t *err = c_new(c_error_t, 1);

    err->domain = domain;
    err->code = code;

    va_start(args, format);
    if (vasprintf(&err->message, format, args) == -1)
        err->message =
            c_strdup_printf("internal: invalid format string %s", format);
    va_end(args);

    return err;
}

c_error_t *
c_error_new_valist(c_quark_t domain, int code, const char *format, va_list ap)
{
    c_error_t *err = c_new(c_error_t, 1);

    err->domain = domain;
    err->code = code;

    err->message = c_strdup_vprintf(format, ap);

    return err;
}

void
c_clear_error(c_error_t **error)
{
    if (error && *error) {
        c_error_free(*error);
        *error = NULL;
    }
}

void
c_error_free(c_error_t *error)
{
    c_return_if_fail(error != NULL);

    c_free(error->message);
    c_free(error);
}

void
c_set_error(
    c_error_t **err, c_quark_t domain, int code, const char *format, ...)
{
    va_list args;

    if (err) {
        va_start(args, format);
        *err = c_error_new_valist(domain, code, format, args);
        va_end(args);
    }
}

void
c_propagate_error(c_error_t **dest, c_error_t *src)
{
    if (dest == NULL) {
        if (src)
            c_error_free(src);
    } else {
        *dest = src;
    }
}

c_error_t *
c_error_copy(const c_error_t *error)
{
    c_error_t *copy = c_new(c_error_t, 1);
    copy->domain = error->domain;
    copy->code = error->code;
    copy->message = c_strdup(error->message);
    return copy;
}

bool
c_error_matches(const c_error_t *error, c_quark_t domain, int code)
{
    if (error) {
        if (error->domain == domain && error->code == code)
            return true;
        return false;
    } else
        return false;
}
