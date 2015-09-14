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
#include <clib-config.h>
#include <stdio.h>

#include <test-fixtures/test.h>

#include <clib.h>
#include <errno.h>

#ifdef WIN32
#include <direct.h>
#endif

#ifdef C_PLATFORM_UNIX
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

bool
is_separator(const char c)
{
    if (c == C_DIR_SEPARATOR)
        return true;
#ifdef WIN32
    if (c == '/')
        return true;
#endif
    return false;
}

static char *
strrchr_seperator(const char *filename)
{
#ifdef WIN32
    char *p2;
#endif
    char *p;

    p = strrchr(filename, C_DIR_SEPARATOR);
#ifdef WIN32
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

static void
normalize_slashes(char *filename, int len)
{
    int i;

    for (i = 0; i < len; i++)
        if (is_separator(filename[i]))
            filename[i] = C_DIR_SEPARATOR;
}

static void
squash_duplicate_separators(char *filename, int *len_in_out)
{
    int len = *len_in_out;
    int out = 1;
    int i;

    for (i = 1; i < len; i++) {
        if (filename[i] == C_DIR_SEPARATOR &&
            filename[out - 1] == C_DIR_SEPARATOR)
            continue;
        else
            filename[out++] = filename[i];
    }

    filename[out] = '\0';
    *len_in_out = out;
}

static void
squash_redundant_dot_dirs(char *filename, int *len_in_out)
{
    int len = *len_in_out;
    int out = 0;
    int i;

    for (i = 0; i < len; ) {
        if (filename[i] == '.' &&
            filename[i + 1] == C_DIR_SEPARATOR)
        {
            if (out == 0 || filename[out - 1] == C_DIR_SEPARATOR) {
                i += 2;
                continue;
            }
        } else
            filename[out++] = filename[i++];
    }

    if (out > 1 && filename[out - 1] == '.' && filename[out - 2] == C_DIR_SEPARATOR)
        out--;

    filename[out] = '\0';
    *len_in_out = out;
}

static bool
squash_dotdot_dirs(char *filename, int *len_in_out)
{
    int len = *len_in_out;
    int out = 0;
    int i;

    for (i = 0; i < len; ) {
        if (filename[i] == C_DIR_SEPARATOR &&
            filename[i + 1] == '.' &&
            filename[i + 2] == '.')
        {
            if (out == 0)
                return false;
            for (; out >= 0; out--)
                if (filename[out] == C_DIR_SEPARATOR)
                    break;
            if (out < 0)
                out = 0;
            i += 3;
        } else
            filename[out++] = filename[i++];
    }

    filename[out] = '\0';
    *len_in_out = out;
    return true;
}

static void
remove_trailing_slash(char *filename, int *len_in_out)
{
    int len = *len_in_out;

    if (len > 1 && filename[len - 1] == C_DIR_SEPARATOR) {
        filename[len - 1] = '\0';
        *len_in_out = len - 1;
    }
}

static bool
do_path_normalize(char *filename, int *len_in_out)
{
    int len = *len_in_out;

    if (len == 0)
        return false;

    normalize_slashes(filename, len);
    squash_duplicate_separators(filename, len_in_out);
    squash_redundant_dot_dirs(filename, len_in_out);
    if (!squash_dotdot_dirs(filename, len_in_out))
        return false;
    remove_trailing_slash(filename, len_in_out);

    return true;
}

char *
c_path_normalize(char *filename, int *len_in_out)
{
    int prefix_len = 0;
    int path_len;
    bool status;

    if (filename[0] == C_DIR_SEPARATOR && filename[1] == filename[0])
        prefix_len++;
    else if (c_ascii_isalpha(filename[0]) && filename[1] == ':')
        prefix_len += 2;

    path_len = *len_in_out - prefix_len;

    status = do_path_normalize(filename + prefix_len, &path_len);
    *len_in_out = path_len + prefix_len;

    return status ? filename : NULL;
}

TEST(check_normalize_filename)
{
    struct {
        const char *a;
        const char *b;
    } tests[] = {
        { "./test", "test" },
        { ".///test", "test" },
        { ".///test///", "test" },
        { ".///test///a/", "test/a" },
        { ".///test///a/b///", "test/a/b" },
        { ".///test//./a/b///", "test/a/b" },
        { "././/test//./a/b///", "test/a/b" },
        { "././/test//./a/b///.", "test/a/b" },
    };
    int i;

    for (i = 0; i < C_N_ELEMENTS(tests); i++) {
        char *str = c_strdup(tests[i].a);
        int len = strlen(str);
        c_assert_cmpstr(c_path_normalize(str, &len), ==, tests[i].b);
        c_free(str);
    }
}

static bool
c_path_is_relative(const char *path)
{
#ifdef WIN32
    if (path[0] == '/' || path[0] == '\\')
        return false;

    if (c_ascii_isalpha(path[0]) &&
        path[1] == ':' &&
        (path[2] == '/' || path[2] == '\\'))
        return false;
#else
    if (path[0] == '/')
        return false;
#endif

    return true;
}

char *
c_path_get_relative_path(const char *parent,
                         const char *descendant)
{
    int parent_len = strlen(parent);
    char *parent_norm = alloca(parent_len + 1);
    int descendant_len = strlen(descendant);
    char *descendant_norm = alloca(descendant_len + 1);

    memcpy(parent_norm, parent, parent_len + 1);
    memcpy(descendant_norm, descendant, descendant_len + 1);

    if (!c_path_normalize(descendant_norm, &descendant_len))
        return NULL;

    if (!c_path_normalize(parent_norm, &parent_len))
        return NULL;

    if (c_path_is_relative(descendant_norm) && strcmp(parent_norm, ".") == 0)
        return c_strdup(descendant_norm);

    if (strncmp(parent_norm, descendant_norm, parent_len) == 0) {
        char *ret = descendant_norm + parent_len;
        if (ret[0] == '\0')
            return c_strdup(".");
        else if (ret[0] != C_DIR_SEPARATOR)
            return NULL;
        else
            return c_strdup(ret + 1);
    }

    return NULL;
}

char *
c_find_program_in_path(const char *program)
{
    char *p;
    char *x, *l;
    char *curdir = NULL;
    char *save = NULL;
#ifdef WIN32
    char *program_exe;
    char *suffix_list[5] = { ".exe", ".cmd", ".bat", ".com", NULL };
    bool has_suffix;
#endif

    c_return_val_if_fail(program != NULL, NULL);
    x = p = c_strdup(c_getenv("PATH"));

    if (x == NULL || *x == '\0') {
        curdir = c_get_current_dir();
        x = curdir;
    }

#ifdef WIN32
	has_suffix = false;
	for (int i = 0; suffix_list[i]; i++) {
		if (c_str_has_suffix(program, suffix_list[i])) {
			has_suffix = true;
			break;
		}
	}
#else
#endif

    while ((l = strtok_r(x, C_SEARCHPATH_SEPARATOR_S, &save)) != NULL) {
        char *probe_path;

        x = NULL;
        probe_path = c_build_path(C_DIR_SEPARATOR_S, l, program, NULL);
        if (access(probe_path, X_OK) == 0) {
            c_free(curdir);
            c_free(p);
            return probe_path;
        }
        c_free(probe_path);

#ifdef WIN32
        /* check for program with a suffix attached */
        if (!has_suffix) {
			for (int i = 0; suffix_list[i]; i++) {
                program_exe =
                    c_strjoin(NULL, program, suffix_list[i], NULL);
                probe_path =
                    c_build_path(C_DIR_SEPARATOR_S, l, program_exe, NULL);
                if (access(probe_path, R_OK) == 0) {
                    c_free(curdir);
                    c_free(p);
                    c_free(program_exe);
                    return probe_path;
                }
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
