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
