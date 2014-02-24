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

#ifndef _RUT_EXCEPTION_H_
#define _RUT_EXCEPTION_H_

#include <stdbool.h>

#include <glib.h>

typedef struct _RutException
{
  GQuark domain;
  int code;
  char *message;
  //TODO: char *backtrace;
} RutException;

void
rut_throw (RutException **err, int klass, int code, const char *format, ...);

bool
rut_catch (const RutException *error, int klass, int code);

void
rut_propagate_exception (RutException **dest, RutException *src);

void
rut_exception_free (RutException *error);

#endif /* _RUT_EXCEPTION_H_ */
