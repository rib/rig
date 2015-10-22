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
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#include <cglib-config.h>

#include "cg-types.h"
#include "cg-util.h"
#include "cg-error-private.h"

#include <clib.h>

#ifdef CG_HAS_GLIB_SUPPORT
#include <glib.h>

#define _RealError GError
#define _real_error_free g_error_free
#define _real_error_copy g_error_copy
#define _real_error_matches g_error_matches
#define _real_error_new_valist g_error_new_valist

#else

#define _RealError c_error_t
#define _real_error_free c_error_free
#define _real_error_copy c_error_copy
#define _real_error_matches c_error_matches
#define _real_error_new_valist c_error_new_valist

#endif

void
cg_error_free(cg_error_t *error)
{
    _real_error_free((_RealError *)error);
}

cg_error_t *
cg_error_copy(cg_error_t *error)
{
    return (cg_error_t *)_real_error_copy((_RealError *)error);
}

bool
cg_error_matches(cg_error_t *error, uint32_t domain, int code)
{
    return _real_error_matches((_RealError *)error, domain, code);
}

#define ERROR_OVERWRITTEN_WARNING                                              \
    "cg_error_t set over the top of a previous cg_error_t or "                 \
    "uninitialized memory.\nThis indicates a bug in someone's "                \
    "code. You must ensure an error is NULL before it's set.\n"                \
    "The overwriting error message was: %s"

void
_cg_set_error(
    cg_error_t **error, uint32_t domain, int code, const char *format, ...)
{
    _RealError *new;

    va_list args;

    va_start(args, format);

    if (error == NULL) {
        c_logv(NULL, C_LOG_DOMAIN, C_LOG_LEVEL_ERROR, format, args);
        va_end(args);
        return;
    }

    new = _real_error_new_valist(domain, code, format, args);
    va_end(args);

    if (*error == NULL)
        *error = (cg_error_t *)new;
    else {
        c_warning(ERROR_OVERWRITTEN_WARNING, new->message);
#ifdef C_PLATFORM_WEB
        _c_web_console_trace();
#endif
    }
}

void
_cg_set_error_literal(cg_error_t **error,
                      uint32_t domain,
                      int code,
                      const char *message)
{
    _cg_set_error(error, domain, code, "%s", message);
}

void
_cg_propagate_error(cg_error_t **dest, cg_error_t *src)
{
    c_return_if_fail(src != NULL);

    if (dest == NULL) {
        c_log(NULL, C_LOG_DOMAIN, C_LOG_LEVEL_ERROR, "%s", src->message);
        cg_error_free(src);
    } else if (*dest)
        c_warning(ERROR_OVERWRITTEN_WARNING, src->message);
    else
        *dest = src;
}

/* This function is only used from the gdk-pixbuf image backend so it
 * should only be called if we are using the system GLib. It would be
 * difficult to get this to work without the system glib because we
 * would need to somehow call the same g_error_free function that
 * gdk-pixbuf is using */
#ifdef CG_HAS_GLIB_SUPPORT
void
_cg_propagate_gerror(cg_error_t **dest, GError *src)
{
    _cg_propagate_error(dest, (cg_error_t *)src);
}
#endif /* CG_HAS_GLIB_SUPPORT */
