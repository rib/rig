/*
 * Directory utility functions.
 *
 * Author:
 *   Gonzalo Paniagua Javier (gonzalo@novell.com)
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

#include <clib-config.h>

#include <clib.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>

#include <winsock2.h>

struct _c_dir_t {
    HANDLE handle;
    char *current;
    char *next;
};

c_dir_t *
c_dir_open(const char *path, unsigned int flags, c_error_t **error)
{
    c_dir_t *dir;
    c_utf16_t *path_utf16;
    c_utf16_t *path_utf16_search;
    WIN32_FIND_DATAW find_data;

    c_return_val_if_fail(path != NULL, NULL);
    c_return_val_if_fail(error == NULL || *error == NULL, NULL);

    dir = c_new0(c_dir_t, 1);
    path_utf16 = u8to16(path);
    path_utf16_search =
        c_malloc((wcslen((wchar_t *)path_utf16) + 3) * sizeof(c_utf16_t));
    wcscpy(path_utf16_search, path_utf16);
    wcscat(path_utf16_search, L"\\*");

    dir->handle = FindFirstFileW(path_utf16_search, &find_data);
    if (dir->handle == INVALID_HANDLE_VALUE) {
        if (error) {
            int err = errno;
            *error = c_error_new(
                C_FILE_ERROR, c_file_error_from_errno(err), c_strerror(err));
        }
        c_free(path_utf16_search);
        c_free(path_utf16);
        c_free(dir);
        return NULL;
    }
    c_free(path_utf16_search);
    c_free(path_utf16);

    while ((wcscmp((wchar_t *)find_data.cFileName, L".") == 0) ||
           (wcscmp((wchar_t *)find_data.cFileName, L"..") == 0)) {
        if (!FindNextFileW(dir->handle, &find_data)) {
            if (error) {
                int err = errno;
                *error = c_error_new(C_FILE_ERROR,
                                     c_file_error_from_errno(err),
                                     c_strerror(err));
            }
            c_free(dir);
            return NULL;
        }
    }

    dir->current = NULL;
    dir->next = u16to8(find_data.cFileName);
    return dir;
}

const char *
c_dir_read_name(c_dir_t *dir)
{
    WIN32_FIND_DATAW find_data;

    c_return_val_if_fail(dir != NULL && dir->handle != 0, NULL);

    if (dir->current)
        c_free(dir->current);
    dir->current = NULL;

    dir->current = dir->next;

    if (!dir->current)
        return NULL;

    dir->next = NULL;

    do {
        if (!FindNextFileW(dir->handle, &find_data)) {
            dir->next = NULL;
            return dir->current;
        }
    } while ((wcscmp((wchar_t *)find_data.cFileName, L".") == 0) ||
             (wcscmp((wchar_t *)find_data.cFileName, L"..") == 0));

    dir->next = u16to8(find_data.cFileName);
    return dir->current;
}

void
c_dir_rewind(c_dir_t *dir)
{
}

void
c_dir_close(c_dir_t *dir)
{
    c_return_if_fail(dir != NULL && dir->handle != 0);

    if (dir->current)
        c_free(dir->current);
    dir->current = NULL;
    if (dir->next)
        c_free(dir->next);
    dir->next = NULL;
    FindClose(dir->handle);
    dir->handle = 0;
    c_free(dir);
}
