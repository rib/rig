/*
 * gerror.c: Error support.
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
#include <config.h>

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <clib.h>

UError *
c_error_new (UQuark domain, int code, const char *format, ...)
{
	va_list args;
	UError *err = c_new (UError, 1);
	
	err->domain = domain;
	err->code = code;

	va_start (args, format);
	if (vasprintf (&err->message, format, args) == -1)
		err->message = c_strdup_printf ("internal: invalid format string %s", format); 
	va_end (args);

	return err;
}

UError *
c_error_new_valist (UQuark domain, int code, const char *format, va_list ap)
{
	UError *err = c_new (UError, 1);
	
	err->domain = domain;
	err->code = code;

        err->message = c_strdup_vprintf (format, ap);

	return err;
}

void
c_clear_error (UError **error)
{
	if (error && *error) {
		c_error_free (*error);
		*error = NULL;
	}
}

void
c_error_free (UError *error)
{
	c_return_if_fail (error != NULL);
	
	c_free (error->message);
	c_free (error);
}

void
c_set_error (UError **err, UQuark domain, int code, const char *format, ...)
{
	va_list args;

	if (err) {
		va_start (args, format);
		*err = c_error_new_valist (domain, code, format, args);
		va_end (args);
	}
}

void
c_propagate_error (UError **dest, UError *src)
{
	if (dest == NULL) {
		if (src)
			c_error_free (src);
	} else {
		*dest = src;
	}
}

UError *
c_error_copy (const UError *error)
{
	UError *copy = c_new (UError, 1);
        copy->domain = error->domain;
        copy->code = error->code;
        copy->message = c_strdup (error->message);
        return copy;
}

cboolean
c_error_matches (const UError *error, UQuark domain, int code)
{
  if (error)
    {
      if (error->domain == domain && error->code == code)
        return TRUE;
      return FALSE;
    }
  else
    return FALSE;
}
