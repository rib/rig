/*
 * Rut
 *
 * Copyright (C) 2014 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

#include <config.h>

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <glib.h>

#include "rut-exception.h"

static RutException *
exception_new_valist (GQuark domain, int code, const char *format, va_list ap)
{
  RutException *err = g_new (RutException, 1);

  err->domain = domain;
  err->code = code;

  err->message = g_strdup_vprintf (format, ap);

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
  RutException *copy = g_new (RutException, 1);
  copy->domain = error->domain;
  copy->code = error->code;
  copy->message = g_strdup (error->message);
  return copy;
}

void
rut_exception_free (RutException *error)
{
  g_free (error->message);
  g_free (error);
}
