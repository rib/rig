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
#include <unistd.h>
#include <dirent.h>

struct _c_dir_t {
    DIR *dir;
#ifndef HAVE_REWINDDIR
    char *path;
#endif
};

c_dir_t *
c_dir_open(const char *path, unsigned int flags, c_error_t **error)
{
    c_dir_t *dir;

    c_return_val_if_fail(path != NULL, NULL);
    c_return_val_if_fail(error == NULL || *error == NULL, NULL);

    (void)flags; /* this is not used */
    dir = c_new(c_dir_t, 1);
    dir->dir = opendir(path);
    if (dir->dir == NULL) {
        if (error) {
            int err = errno;
            *error = c_error_new(
                C_FILE_ERROR, c_file_error_from_errno(err), c_strerror(err));
        }
        c_free(dir);
        return NULL;
    }
#ifndef HAVE_REWINDDIR
    dir->path = c_strdup(path);
#endif
    return dir;
}

const char *
c_dir_read_name(c_dir_t *dir)
{
    struct dirent *entry;

    c_return_val_if_fail(dir != NULL && dir->dir != NULL, NULL);
    do {
        entry = readdir(dir->dir);
        if (entry == NULL)
            return NULL;
    } while ((strcmp(entry->d_name, ".") == 0) ||
             (strcmp(entry->d_name, "..") == 0));

    return entry->d_name;
}

void
c_dir_rewind(c_dir_t *dir)
{
    c_return_if_fail(dir != NULL && dir->dir != NULL);
#ifndef HAVE_REWINDDIR
    closedir(dir->dir);
    dir->dir = opendir(dir->path);
#else
    rewinddir(dir->dir);
#endif
}

void
c_dir_close(c_dir_t *dir)
{
    c_return_if_fail(dir != NULL && dir->dir != 0);
    closedir(dir->dir);
#ifndef HAVE_REWINDDIR
    c_free(dir->path);
#endif
    dir->dir = NULL;
    c_free(dir);
}

int
c_mkdir_with_parents(const char *pathname, int mode)
{
    char *path, *d;
    int rv;

    if (!pathname || *pathname == '\0') {
        errno = EINVAL;
        return -1;
    }

    d = path = c_strdup(pathname);
    if (*d == '/')
        d++;

    while (true) {
        if (*d == '/' || *d == '\0') {
            char orig = *d;
            *d = '\0';
            rv = mkdir(path, mode);
            if (rv == -1 && errno != EEXIST) {
                c_free(path);
                return -1;
            }

            *d++ = orig;
            while (orig == '/' && *d == '/')
                d++;
            if (orig == '\0')
                break;
        } else {
            d++;
        }
    }

    c_free(path);

    return 0;
}
