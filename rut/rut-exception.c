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

#include <config.h>

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <clib.h>

#include "rut-exception.h"

static RutException *
exception_new_valist (GQuark domain, int code, const char *format, va_list ap)
{
  RutException *err = c_new (RutException, 1);

  err->domain = domain;
  err->code = code;

  err->message = c_strdup_vprintf (format, ap);

  return err;
}

void
rut_throw (RutException **err, int domain, int code, const char *format, ...)
{
  va_list args;

  va_start (args, format);

  if (err)
    *err = exception_new_valist (domain, code, format, args);
  else
    g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, format, args);

  va_end (args);
}

bool
rut_catch (const RutException *error, int domain, int code)
{
  if (error)
    {
      if (error->domain == domain && error->code == code)
        return true;
      return false;
    }
  else
    return false;
}

void
rut_propagate_exception (RutException **dest, RutException *src)
{
  if (dest == NULL)
    {
      if (src)
        rut_exception_free (src);
    }
  else
    {
      *dest = src;
    }
}

RutException *
rut_exception_copy (const RutException *error)
{
  RutException *copy = c_new (RutException, 1);
  copy->domain = error->domain;
  copy->code = error->code;
  copy->message = c_strdup (error->message);
  return copy;
}

void
rut_exception_free (RutException *error)
{
  c_free (error->message);
  c_free (error);
}
