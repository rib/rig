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

#ifndef _RUT_OS_H_
#define _RUT_OS_H_

#include <stdint.h>
#include <string.h>

#include "rut-exception.h"

#define RUT_IO_EXCEPTION 1

typedef enum _RutIOException
{
  RUT_IO_EXCEPTION_BAD_VALUE = 1,
  RUT_IO_EXCEPTION_NO_SPACE,
  RUT_IO_EXCEPTION_IO,
} RutIOException;

bool
rut_os_read (int fd, uint8_t *data, int len, RutException **e);

bool
rut_os_write (int fd, uint8_t *data, int len, RutException **e);

#endif /* _RUT_OS_H_ */
