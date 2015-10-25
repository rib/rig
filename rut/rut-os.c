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

#include <rut-config.h>

#include <errno.h>

#ifdef _MSC_VER
#include <io.h>
#define write _write
#define read _read
#else
#include <unistd.h>
#include <netinet/in.h>
#endif

#ifdef __linux__
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#endif

#include "rut-os.h"

int
read_write_errno_to_exception_code(int err)
{
    switch (err) {
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
rut_os_read(int fd, void *data, int *len, rut_exception_t **e)
{
    uint8_t *buffer = data;

    do {
        int ret = read(fd, buffer, *len);
        if (ret < 0) {
            if (ret == EAGAIN || ret == EINTR)
                continue;

            rut_throw(e,
                      RUT_IO_EXCEPTION,
                      read_write_errno_to_exception_code(errno),
                      "Failed to read file: %s",
                      strerror(errno));
            return false;
        }
        *len = ret;
        break;
    } while (1);

    return true;
}

bool
rut_os_read_len(int fd, void *data, int len, rut_exception_t **e)
{
    int remaining = len;
    uint8_t *buffer = data;

    do {
        int ret = read(fd, buffer, len);
        if (ret < 0) {
            if (ret == EAGAIN || ret == EINTR)
                continue;

            rut_throw(e,
                      RUT_IO_EXCEPTION,
                      read_write_errno_to_exception_code(errno),
                      "Failed to read file: %s",
                      strerror(errno));
            return false;
        }
        remaining -= ret;
        buffer += ret;
    } while (remaining);

    return true;
}

bool
rut_os_write(int fd, void *data, int len, rut_exception_t **e)
{
    int remaining = len;
    uint8_t *buffer = data;

    do {
        int ret = write(fd, buffer, len);
        if (ret < 0) {
            if (ret == EAGAIN || ret == EINTR)
                continue;

            rut_throw(e,
                      RUT_IO_EXCEPTION,
                      read_write_errno_to_exception_code(errno),
                      "Failed to write file: %s",
                      strerror(errno));
            return false;
        }
        remaining -= ret;
        buffer += ret;
    } while (remaining);

    return true;
}

#ifdef __linux__
int
rut_os_connect_to_abstract_socket(const char *socket_name)
{
    int fd;
    struct sockaddr_un addr;
    socklen_t size;
    int name_size;
    int flags;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        c_warning("Failed to create PF_LOCAL socket fd");
        return -1;
    }

    /* XXX: Android doesn't seem to support SOCK_CLOEXEC so we use
     * fcntl() instead */
    flags = fcntl(fd, F_GETFD);
    if (flags == -1) {
        c_warning("Failed to get fd flags for setting O_CLOEXEC on socket\n");
        return 1;
    }

    if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {
        c_warning("Failed to set O_CLOEXEC on abstract socket\n");
        return 1;
    }

    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    name_size = snprintf(
        addr.sun_path, sizeof addr.sun_path, "%c%s", '\0', socket_name);

    if (name_size > (int)sizeof addr.sun_path) {
        c_warning("socket path \"%crig-simulator\" plus null terminator"
                  " exceeds 108 bytes\n",
                  '\0');
        close(fd);
        return -1;
    };

    size = offsetof(struct sockaddr_un, sun_path) + name_size;

    if (connect(fd, (struct sockaddr *)&addr, size) < 0) {
        const char *msg = strerror(errno);
        c_warning("Failed to connect to abstract socket: %s\n", msg);
        close(fd);
        return -1;
    }

    return fd;
}

int
rut_os_listen_on_abstract_socket(const char *name, rut_exception_t **e)
{
    struct sockaddr_un addr;
    socklen_t size, name_size;
    int fd;
    int flags;

    fd = socket(PF_LOCAL, SOCK_STREAM, 0);
    if (fd < 0) {
        rut_throw(e,
                  RUT_IO_EXCEPTION,
                  RUT_IO_EXCEPTION_IO,
                  "Failed to create socket for listening: %s",
                  strerror(errno));
        return false;
    }

    /* XXX: Android doesn't seem to support SOCK_CLOEXEC so we use
     * fcntl() instead */
    flags = fcntl(fd, F_GETFD);
    if (flags == -1) {
        rut_throw(e,
                  RUT_IO_EXCEPTION,
                  RUT_IO_EXCEPTION_IO,
                  "Failed to get fd flags for setting O_CLOEXEC: %s\n",
                  strerror(errno));
        return false;
    }

    if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {
        rut_throw(e,
                  RUT_IO_EXCEPTION,
                  RUT_IO_EXCEPTION_IO,
                  "Failed to set O_CLOEXEC on abstract socket: %s\n",
                  strerror(errno));
        return false;
    }

    /* FIXME: Use a more unique name otherwise multiple Rig based
     * applications won't run at the same time! */
    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    name_size =
        snprintf(addr.sun_path, sizeof addr.sun_path, "%c%s", '\0', name);
    size = offsetof(struct sockaddr_un, sun_path) + name_size;
    if (bind(fd, (struct sockaddr *)&addr, size) < 0) {
        rut_throw(e,
                  RUT_IO_EXCEPTION,
                  RUT_IO_EXCEPTION_IO,
                  "failed to bind to @%s: %s\n",
                  addr.sun_path + 1,
                  strerror(errno));
        close(fd);
        return false;
    }

    if (listen(fd, 1) < 0) {
        rut_throw(e,
                  RUT_IO_EXCEPTION,
                  RUT_IO_EXCEPTION_IO,
                  "Failed to start listening on socket: %s\n",
                  strerror(errno));
        close(fd);
        return false;
    }

    return fd;
}
#endif /* linux */

int
rut_os_listen_on_tcp_socket(int port, rut_exception_t **e)
{
    int fd = -1;
    struct sockaddr *address;
    socklen_t address_len;
    struct sockaddr_in addr_in;
    bool need_bind = true;

    memset(&addr_in, 0, sizeof(addr_in));
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(port);
    address_len = sizeof(addr_in);
    address = (struct sockaddr *)(&addr_in);
    if (addr_in.sin_port == 0)
        need_bind = false;

    fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        rut_throw(e,
                  RUT_IO_EXCEPTION,
                  RUT_IO_EXCEPTION_IO,
                  "pb_rpc_server_new: socket() failed: %s\n",
                  strerror(errno));
        return -1;
    }
    if (need_bind && bind(fd, address, address_len) < 0) {
        rut_throw(e,
                  RUT_IO_EXCEPTION,
                  RUT_IO_EXCEPTION_IO,
                  "pb_rpc_server_new: error binding to port: %s\n",
                  strerror(errno));
        close(fd);
        return -1;
    }
    if (listen(fd, 255) < 0) {
        rut_throw(e,
                  RUT_IO_EXCEPTION,
                  RUT_IO_EXCEPTION_IO,
                  "pb_rpc_server_new: listen() failed: %s\n",
                  strerror(errno));
        close(fd);
        return -1;
    }

#ifdef RIG_ENABLE_DEBUG
    {
        if (getsockname(fd, address, &address_len) < 0) {
            c_warning("Failed to query back the address of the listening "
                      "socket: %s",
                      strerror(errno));
        } else {
            int port = ntohs(addr_in.sin_port);
            const uint8_t *ip = (const uint8_t *)&(addr_in.sin_addr);
            c_message("Listening on socket: %u.%u.%u.%u:%d",
                      ip[0],
                      ip[1],
                      ip[2],
                      ip[3],
                      port);
        }
    }
#endif

    return fd;
}
