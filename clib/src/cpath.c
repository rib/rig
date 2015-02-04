/*
 * Portable Utility Functions
 *
 * Author:
 *   Miguel de Icaza (miguel@novell.com)
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
#include <config.h>
#include <stdio.h>
#include <clib.h>
#include <errno.h>

#ifdef C_OS_WIN32
#include <direct.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

char *
c_build_path(const char *separator, const char *first_element, ...)
{
    const char *elem, *next, *endptr;
    bool trimmed;
    c_string_t *path;
    va_list args;
    size_t slen;

    c_return_val_if_fail(separator != NULL, NULL);

    path = c_string_sized_new(48);
    slen = strlen(separator);

    va_start(args, first_element);
    for (elem = first_element; elem != NULL; elem = next) {
        /* trim any trailing separators from @elem */
        endptr = elem + strlen(elem);
        trimmed = false;

        while (endptr >= elem + slen) {
            if (strncmp(endptr - slen, separator, slen) != 0)
                break;

            endptr -= slen;
            trimmed = true;
        }

        /* append elem, not including any trailing separators */
        if (endptr > elem)
            c_string_append_len(path, elem, endptr - elem);

        /* get the next element */
        do {
            if (!(next = va_arg(args, char *)))
                break;

            /* remove leading separators */
            while (!strncmp(next, separator, slen))
                next += slen;
        } while (*next == '\0');

        if (next || trimmed)
            c_string_append_len(path, separator, slen);
    }
    va_end(args);

    return c_string_free(path, false);
}

static char *
strrchr_seperator(const char *filename)
{
#ifdef C_OS_WIN32
    char *p2;
#endif
    char *p;

    p = strrchr(filename, C_DIR_SEPARATOR);
#ifdef C_OS_WIN32
    p2 = strrchr(filename, '/');
    if (p2 > p)
        p = p2;
#endif

    return p;
}

char *
c_path_get_dirname(const char *filename)
{
    char *p, *r;
    size_t count;
    c_return_val_if_fail(filename != NULL, NULL);

    p = strrchr_seperator(filename);
    if (p == NULL)
        return c_strdup(".");
    if (p == filename)
        return c_strdup("/");
    count = p - filename;
    r = c_malloc(count + 1);
    strncpy(r, filename, count);
    r[count] = 0;

    return r;
}

char *
c_path_get_basename(const char *filename)
{
    char *r;
    c_return_val_if_fail(filename != NULL, NULL);

    /* Empty filename -> . */
    if (!*filename)
        return c_strdup(".");

    /* No separator -> filename */
    r = strrchr_seperator(filename);
    if (r == NULL)
        return c_strdup(filename);

    /* Trailing slash, remove component */
    if (r[1] == 0) {
        char *copy = c_strdup(filename);
        copy[r - filename] = 0;
        r = strrchr_seperator(copy);

        if (r == NULL) {
            c_free(copy);
            return c_strdup("/");
        }
        r = c_strdup(&r[1]);
        c_free(copy);
        return r;
    }

    return c_strdup(&r[1]);
}

char *
c_find_program_in_path(const char *program)
{
    char *p;
    char *x, *l;
    char *curdir = NULL;
    char *save = NULL;
#ifdef C_OS_WIN32
    char *program_exe;
    char *suffix_list[5] = { ".exe", ".cmd", ".bat", ".com", NULL };
    int listx;
    bool hasSuffix;
#endif

    c_return_val_if_fail(program != NULL, NULL);
    x = p = c_strdup(c_getenv("PATH"));

    if (x == NULL || *x == '\0') {
        curdir = c_get_current_dir();
        x = curdir;
    }

#ifdef C_OS_WIN32
    /* see if program already has a suffix */
    listx = 0;
    hasSuffix = false;
    while (!hasSuffix && suffix_list[listx]) {
        hasSuffix = c_str_has_suffix(program, suffix_list[listx++]);
    }
#endif

    while ((l = strtok_r(x, C_SEARCHPATH_SEPARATOR_S, &save)) != NULL) {
        char *probe_path;

        x = NULL;
        probe_path = c_build_path(C_DIR_SEPARATOR_S, l, program, NULL);
        if (access(probe_path, X_OK) ==
            0) { /* FIXME: on windows this is just a read permissions test */
            c_free(curdir);
            c_free(p);
            return probe_path;
        }
        c_free(probe_path);

#ifdef C_OS_WIN32
        /* check for program with a suffix attached */
        if (!hasSuffix) {
            listx = 0;
            while (suffix_list[listx]) {
                program_exe =
                    c_strjoin(NULL, program, suffix_list[listx], NULL);
                probe_path =
                    c_build_path(C_DIR_SEPARATOR_S, l, program_exe, NULL);
                if (access(probe_path, X_OK) == 0) { /* FIXME: on windows this
                                                        is just a read
                                                        permissions test */
                    c_free(curdir);
                    c_free(p);
                    c_free(program_exe);
                    return probe_path;
                }
                listx++;
                c_free(probe_path);
                c_free(program_exe);
            }
        }
#endif
    }
    c_free(curdir);
    c_free(p);
    return NULL;
}

static char *name;

void
c_set_prgname(const char *prgname)
{
    name = c_strdup(prgname);
}

char *
c_get_prgname(void)
{
    return name;
}
