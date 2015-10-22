/*
 * Shell utility functions.
 *
 * Author:
 *   Gonzalo Paniagua Javier (gonzalo@novell.com
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
#include <clib.h>

c_quark_t
c_shell_error_get_quark(void)
{
    return c_quark_from_static_string("g-shell-error-quark");
}

static int
split_cmdline(const char *cmdline, c_ptr_array_t *array, c_error_t **error)
{
    char *ptr;
    char c;
    bool escaped = false, fresh = true;
    char quote_char = '\0';
    c_string_t *str;

    str = c_string_new("");
    ptr = (char *)cmdline;
    while ((c = *ptr++) != '\0') {
        if (escaped) {
            /*
             * \CHAR is only special inside a double quote if CHAR is
             * one of: $`"\ and newline
             */
            if (quote_char == '\"') {
                if (!(c == '$' || c == '`' || c == '"' || c == '\\'))
                    c_string_append_c(str, '\\');
                c_string_append_c(str, c);
            } else {
                if (!c_ascii_isspace(c))
                    c_string_append_c(str, c);
            }
            escaped = false;
        } else if (quote_char) {
            if (c == quote_char) {
                quote_char = '\0';
                if (fresh && (c_ascii_isspace(*ptr) || *ptr == '\0')) {
                    c_ptr_array_add(array, c_string_free(str, false));
                    str = c_string_new("");
                }
            } else if (c == '\\') {
                escaped = true;
            } else
                c_string_append_c(str, c);
        } else if (c_ascii_isspace(c)) {
            if (str->len > 0) {
                c_ptr_array_add(array, c_string_free(str, false));
                str = c_string_new("");
            }
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '\'' || c == '"') {
            fresh = str->len == 0;
            quote_char = c;
        } else {
            c_string_append_c(str, c);
        }
    }

    if (escaped) {
        if (error)
            *error = c_error_new(
                C_SHELL_ERROR, C_SHELL_ERROR_BAD_QUOTING, "Unfinished escape.");
        c_string_free(str, true);
        return -1;
    }

    if (quote_char) {
        if (error)
            *error = c_error_new(
                C_SHELL_ERROR, C_SHELL_ERROR_BAD_QUOTING, "Unfinished quote.");
        c_string_free(str, true);
        return -1;
    }

    if (str->len > 0) {
        c_ptr_array_add(array, c_string_free(str, false));
    } else {
        c_string_free(str, true);
    }
    c_ptr_array_add(array, NULL);
    return 0;
}

bool
c_shell_parse_argv(const char *command_line,
                   int *argcp,
                   char ***argvp,
                   c_error_t **error)
{
    c_ptr_array_t *array;
    int argc;
    char **argv;

    c_return_val_if_fail(command_line, false);
    c_return_val_if_fail(error == NULL || *error == NULL, false);

    array = c_ptr_array_new();
    if (split_cmdline(command_line, array, error)) {
        c_ptr_array_add(array, NULL);
        c_strfreev((char **)array->pdata);
        c_ptr_array_free(array, false);
        return false;
    }

    argc = array->len;
    argv = (char **)array->pdata;

    if (argc == 1) {
        c_strfreev(argv);
        c_ptr_array_free(array, false);
        return false;
    }

    if (argcp) {
        *argcp = array->len - 1;
    }

    if (argvp) {
        *argvp = argv;
    } else {
        c_strfreev(argv);
    }

    c_ptr_array_free(array, false);
    return true;
}

char *
c_shell_quote(const char *unquoted_string)
{
    c_string_t *result = c_string_new("'");
    const char *p;

    for (p = unquoted_string; *p; p++) {
        if (*p == '\'')
            c_string_append(result, "'\\'");
        c_string_append_c(result, *p);
    }
    c_string_append_c(result, '\'');
    return c_string_free(result, false);
}

char *
c_shell_unquote(const char *quoted_string, c_error_t **error)
{
    c_string_t *result;
    const char *p;
    int do_unquote = 0;

    if (quoted_string == NULL)
        return NULL;

    /* Quickly try to determine if we need to unquote or not */
    for (p = quoted_string; *p; p++) {
        if (*p == '\'' || *p == '"' || *p == '\\') {
            do_unquote = 1;
            break;
        }
    }

    if (!do_unquote)
        return c_strdup(quoted_string);

    /* We do need to unquote */
    result = c_string_new("");
    for (p = quoted_string; *p; p++) {

        if (*p == '\'') {
            /* Process single quote, not even \ is processed by glib's version
             */
            for (p++; *p; p++) {
                if (*p == '\'')
                    break;
                c_string_append_c(result, *p);
            }
            if (!*p) {
                c_set_error(error, 0, 0, "Open quote");
                return NULL;
            }
        } else if (*p == '"') {
            /* Process double quote, allows some escaping */
            for (p++; *p; p++) {
                if (*p == '"')
                    break;
                if (*p == '\\') {
                    p++;
                    if (*p == 0) {
                        c_set_error(error, 0, 0, "Open quote");
                        return NULL;
                    }
                    switch (*p) {
                    case '$':
                    case '"':
                    case '\\':
                    case '`':
                        break;
                    default:
                        c_string_append_c(result, '\\');
                        break;
                    }
                }
                c_string_append_c(result, *p);
            }
            if (!*p) {
                c_set_error(error, 0, 0, "Open quote");
                return NULL;
            }
        } else if (*p == '\\') {
            char c = *(++p);
            if (!(c == '$' || c == '"' || c == '\\' || c == '`' || c == '\'' ||
                  c == 0))
                c_string_append_c(result, '\\');
            if (c == 0)
                break;
            else
                c_string_append_c(result, c);
        } else
            c_string_append_c(result, *p);
    }
    return c_string_free(result, false);
}

#if JOINT_TEST
/*
 * This test is designed to be built with the 2 glib/cglib to compare
 */

char *args[] = {
    "\\",                "\"Foo'bar\"",           "'foo'",
    "'fo\'b'",           "'foo\"bar'",            "'foo' dingus bar",
    "'foo' 'bar' 'baz'", "\"foo\" 'bar' \"baz\"", "\"f\\$\\\'",
    "\"\\",              "\\\\",                  "'\\\\'",
    "\"f\\$\"\\\"\\\\", //  /\\\"\\\\"
    "'f\\$'\\\"\\\\",    "'f\\$\\\\'",            NULL
};

int
main()
{
    char **s = args;
    int i;

    while (*s) {
        char *r1 = c_shell_unquote(*s, NULL);
        char *r2 = g2_shell_unquote(*s, NULL);
        char *ok =
            r1 == r2 ? "ok" : (r1 != NULL && r2 != NULL && strcmp(r1, r2) == 0)
            ? "ok"
            : "fail";

        printf("%s [%s] -> [%s] - [%s]\n", ok, *s, r1, r2);
        s++;
    }
    return;
    char buffer[10];
    buffer[0] = '\"';
    buffer[1] = '\\';
    buffer[3] = '\"';
    buffer[4] = 0;

    for (i = 32; i < 255; i++) {
        buffer[2] = i;
        printf("%d [%s] -> [%s]\n", i, buffer, c_shell_unquote(buffer, NULL));
    }
}
#endif
