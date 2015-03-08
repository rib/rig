/*
 * Copyright (C) 2014  Intel Corporation
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

#include <config.h>

#include <errno.h>
#include <stdio.h>
#include <stdbool.h>

#include <clib.h>

typedef struct _cookie_t {
    char *buffer;
    bool user_buffer;
    size_t buffer_size;
    size_t file_size;
    size_t pos;
} cookie_t;

static int
read_callback(void *cookie_, char *buf, int len)
{
    cookie_t *cookie = cookie_;

    if (cookie->pos >= cookie->file_size)
        return 0;

    if ((cookie->pos + len) > cookie->file_size)
        len = cookie->file_size - cookie->pos;

    memcpy(buf, cookie->buffer + cookie->pos, len);

    cookie->pos += len;

    return len;
}

static int
write_callback(void *cookie_, const char *buf, int len)
{
    cookie_t *cookie = cookie_;

    /* For simplicitly we avoid doing a partial write when there isn't
     * enough space.
     *
     * We consider it an error if there isn't also room to write a null
     * byte after the write.
     */
    if ((cookie->pos + len) >= cookie->buffer_size) {
        errno = ENOSPC;
        return -1;
    }

    if (cookie->pos > cookie->file_size) {
        size_t extend = cookie->pos - cookie->file_size;
        memset(cookie->buffer + cookie->file_size, 0, extend);
        cookie->file_size = cookie->pos;
        cookie->buffer[cookie->pos] = '\0';
    }

    memcpy(cookie->buffer + cookie->pos, buf, len);

    cookie->pos += len;

    return len;
}

static off_t
seek_callback(void *cookie_, off_t pos, int whence)
{
    cookie_t *cookie = cookie_;
    off_t new_pos = 0;

    switch (whence) {
    case SEEK_SET:
        new_pos = pos;
        break;
    case SEEK_CUR:
        new_pos = cookie->pos + pos;
        break;
    case SEEK_END:
        new_pos = cookie->file_size - pos;
        break;
    default:
        errno = EINVAL;
        return -1;
    }

    if (new_pos < 0) {
        errno = EINVAL;
        return -1;
    }

    cookie->pos = new_pos;

    return new_pos;
}

static int
close_callback(void *cookie_)
{
    cookie_t *cookie = cookie_;

    if (!cookie->user_buffer)
        c_free(cookie->buffer);
    c_free(cookie);

    return 0;
}

FILE *
fmemopen(void *buf, size_t size, const char *mode)
{
    cookie_t *cookie;
    FILE *file = NULL;

    /* Behave like glibc... */
    if (size <= 0) {
        errno = EINVAL;
        return NULL;
    }

    cookie = c_new(cookie_t, 1);

    if (buf) {
        cookie->buffer = buf;
        cookie->user_buffer = true;
    } else {
        cookie->buffer = c_malloc(size);
        cookie->user_buffer = false;
        cookie->buffer[0] = '\0';
    }

    cookie->buffer_size = size;

    if (mode[0] == 'r') {
        cookie->file_size = cookie->buffer_size;
        cookie->pos = 0;
    } else if (mode[0] == 'w') {
        cookie->file_size = 0;
        cookie->pos = 0;

        if (cookie->pos < cookie->buffer_size)
            cookie->buffer[cookie->pos] = '\0';
    } else if (mode[0] == 'a') {
        /* Note: Like glibc a+ isn't handled specially */
        cookie->file_size = strnlen(cookie->buffer, cookie->buffer_size);
        cookie->pos = cookie->file_size;
    } else {
        c_free(cookie);
        errno = EINVAL;
        return NULL;
    }

    file = funopen(
        cookie, read_callback, write_callback, seek_callback, close_callback);

    if (!file)
        c_free(cookie);

    return file;
}
