/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
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
 */

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef __CG_ERROR_H__
#define __CG_ERROR_H__

#include "cg-types.h"

CG_BEGIN_DECLS

/**
 * SECTION:cg-error
 * @short_description: A way for CGlib to throw exceptions
 *
 * As a general rule CGlib shields non-recoverable errors from
 * developers, such as most heap allocation failures (unless for
 * exceptionally large resources which we might reasonably expect to
 * fail) and this reduces the burden on developers.
 *
 * There are some CGlib apis though that can fail for exceptional
 * reasons that can also potentially be recovered from at runtime
 * and for these apis we use a standard convention for reporting
 * runtime recoverable errors.
 *
 * As an example if we look at the cg_device_new() api which
 * takes an error argument:
 * |[
 *   cg_device_t *
 *   cg_device_new (cg_display_t *display, cg_error_t **error);
 * ]|
 *
 * A caller interested in catching any runtime error when creating a
 * new #cg_device_t would pass the address of a #cg_error_t pointer
 * that has first been initialized to %NULL as follows:
 *
 * |[
 *   cg_error_t *error = NULL;
 *   cg_device_t *context;
 *
 *   context = cg_device_new (NULL, &error);
 * ]|
 *
 * The return status should usually be enough to determine if there
 * was an error set (in this example we can check if context == %NULL)
 * but if it's not possible to tell from the function's return status
 * you can instead look directly at the error pointer which you
 * initialized to %NULL. In this example we now check the error,
 * report any error to the user, free the error and then simply
 * abort without attempting to recover.
 *
 * |[
 *   if (context == NULL)
 *     {
 *       fprintf (stderr, "Failed to create a CGlib context: %s\n",
 *                error->message);
 *       cg_error_free (error);
 *       abort ();
 *     }
 * ]|
 *
 * All CGlib APIs that accept an error argument can also be passed a
 * %NULL pointer. In this case if an exceptional error condition is hit
 * then CGlib will simply log the error message and abort the
 * application. This can be compared to language execeptions where the
 * developer has not attempted to catch the exception. This means the
 * above example is essentially redundant because it's what CGlib would
 * have done automatically and so, similarly, if your application has
 * no way to recover from a particular error you might just as well
 * pass a %NULL #cg_error_t pointer to save a bit of typing.
 *
 * <note>If you are used to using the GLib API you will probably
 * recognize that #cg_error_t is just like a #GError. In fact if CGlib
 * has been built with --enable-glib then it is safe to cast a
 * #cg_error_t to a #GError.</note>
 *
 * <note>An important detail to be aware of if you are used to using
 * GLib's GError API is that CGlib deviates from the GLib GError
 * conventions in one noteable way which is that a %NULL error pointer
 * does not mean you want to ignore the details of an error, it means
 * you are not trying to catch any exceptional errors the function might
 * throw which will result in the program aborting with a log message
 * if an error is thrown.</note>
 */

/**
 * cg_error_t:
 * @domain: A high-level domain identifier for the error
 * @code: A specific error code within a specified domain
 * @message: A human readable error message
 */
typedef struct _cg_error_t {
    uint32_t domain;
    int code;
    char *message;
} cg_error_t;

/**
 * cg_error_free:
 * @error: A #cg_error_t thrown by the CGlib api
 *
 * Frees a #cg_error_t and associated resources.
 */
void cg_error_free(cg_error_t *error);

/**
 * cg_error_copy:
 * @error: A #cg_error_t thrown by the CGlib api
 *
 * Makes a copy of @error which can later be freed using
 * cg_error_free().
 *
 * Return value: A newly allocated #cg_error_t initialized to match the
 *               contents of @error.
 */
cg_error_t *cg_error_copy(cg_error_t *error);

/**
 * cg_error_matches:
 * @error: A #cg_error_t thrown by the CGlib api or %NULL
 * @domain: The error domain
 * @code: The error code
 *
 * Returns %true if error matches @domain and @code, %false otherwise.
 * In particular, when error is %NULL, false will be returned.
 *
 * Return value: whether the @error corresponds to the given @domain
 *               and @code.
 */
bool cg_error_matches(cg_error_t *error, uint32_t domain, int code);

/**
 * CG_GLIB_ERROR:
 * @CG_ERROR: A #cg_error_t thrown by the CGlib api or %NULL
 *
 * Simply casts a #cg_error_t to a #GError
 *
 * If CGlib is built with GLib support then it can safely be assumed
 * that a cg_error_t is a GError and can be used directly with the
 * GError api.
 */
#ifdef CG_HAS_GLIB_SUPPORT
#define CG_GLIB_ERROR(CG_ERROR) ((GError *)CG_ERROR)
#endif

CG_END_DECLS

#endif /* __CG_ERROR_H__ */
