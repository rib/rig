/*
 * Copyright (C) 2014-2015  Intel Corporation
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

#include <clib-config.h>

#include <errno.h>
#include <stdio.h>
#include <stdbool.h>

#include <clib.h>

struct _c_mem_file_t {
    char *buffer;
    bool user_buffer;
    size_t buffer_size;
    size_t file_size;
    size_t pos;
};

int
c_mem_file_read(c_mem_file_t *file, char *buf, int len)
{
    if (file->pos >= file->file_size)
        return 0;

    if ((file->pos + len) > file->file_size)
        len = file->file_size - file->pos;

    memcpy(buf, file->buffer + file->pos, len);

    file->pos += len;

    return len;
}

int
c_mem_file_write(c_mem_file_t *file, const char *buf, int len)
{
    /* For simplicitly we avoid doing a partial write when there isn't
     * enough space.
     *
     * We consider it an error if there isn't also room to write a null
     * byte after the write.
     */
    if ((file->pos + len) >= file->buffer_size) {
        errno = ENOSPC;
        return -1;
    }

    if (file->pos > file->file_size) {
        size_t extend = file->pos - file->file_size;
        memset(file->buffer + file->file_size, 0, extend);
        file->file_size = file->pos;
        file->buffer[file->pos] = '\0';
    }

    memcpy(file->buffer + file->pos, buf, len);

    file->pos += len;

    return len;
}

int
c_mem_file_seek(c_mem_file_t *file, int pos, int whence)
{
    int new_pos = 0;

    switch (whence) {
    case SEEK_SET:
        new_pos = pos;
        break;
    case SEEK_CUR:
        new_pos = file->pos + pos;
        break;
    case SEEK_END:
        new_pos = file->file_size - pos;
        break;
    default:
        errno = EINVAL;
        return -1;
    }

    if (new_pos < 0) {
        errno = EINVAL;
        return -1;
    }

    file->pos = new_pos;

    return new_pos;
}

int
c_mem_file_close(c_mem_file_t *file)
{
    if (!file->user_buffer)
        c_free(file->buffer);
    c_free(file);

    return 0;
}

c_mem_file_t *
c_mem_file_open(void *buf, size_t size, const char *mode)
{
    c_mem_file_t *file;

    /* Behave like glibc... */
    if (size <= 0) {
        errno = EINVAL;
        return NULL;
    }

    file = c_new(c_mem_file_t, 1);

    if (buf) {
        file->buffer = buf;
        file->user_buffer = true;
    } else {
        file->buffer = c_malloc(size);
        file->user_buffer = false;
        file->buffer[0] = '\0';
    }

    file->buffer_size = size;

    if (mode[0] == 'r') {
        file->file_size = file->buffer_size;
        file->pos = 0;
    } else if (mode[0] == 'w') {
        file->file_size = 0;
        file->pos = 0;

        if (file->pos < file->buffer_size)
            file->buffer[file->pos] = '\0';
    } else if (mode[0] == 'a') {
        /* Note: Like glibc a+ isn't handled specially */
        file->file_size = strnlen(file->buffer, file->buffer_size);
        file->pos = file->file_size;
    } else {
        c_free(file);
        errno = EINVAL;
        return NULL;
    }

    return file;
}
