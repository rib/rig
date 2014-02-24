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

#include <errno.h>

#ifdef _MSC_VER
#include <io.h>
#define write _write
#define read _read
#else
#include <unistd.h>
#endif

#include "rut-os.h"

int
read_write_errno_to_exception_code (int err)
{
  switch (err)
    {
    case EBADF:
    case EINVAL:
    case EFAULT:
    case EDESTADDRREQ:
    case EFBIG:
      return RUT_IO_EXCEPTION_BAD_VALUE;
    case ENOSPC:
    case EDQUOT:
      return RUT_IO_EXCEPTION_NO_SPACE;
    case EIO:
    case EPIPE:
    default:
      return RUT_IO_EXCEPTION_IO;
    }
}

bool
rut_os_read (int fd, uint8_t *data, int len, RutException **e)
{
  int remaining = len;

  do {
    int ret = read (fd, data, len);
    if (ret < 0)
      {
        if (ret == EAGAIN || ret == EINTR)
          continue;

        rut_throw (e,
                   RUT_IO_EXCEPTION,
                   read_write_errno_to_exception_code (errno),
                   "Failed to read file: %s",
                   strerror (errno));
        return false;
      }
    remaining -= ret;
    data += ret;
  } while (remaining);

  return true;
}

bool
rut_os_write (int fd, uint8_t *data, int len, RutException **e)
{
  int remaining = len;

  do {
    int ret = write (fd, data, len);
    if (ret < 0)
      {
        if (ret == EAGAIN || ret == EINTR)
          continue;

        rut_throw (e,
                   RUT_IO_EXCEPTION,
                   read_write_errno_to_exception_code (errno),
                   "Failed to write file: %s",
                   strerror (errno));
        return false;
      }
    remaining -= ret;
    data += ret;
  } while (remaining);

  return true;
}
