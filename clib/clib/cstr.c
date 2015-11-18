/*
 * String Utility Functions.
 *
 * Author:
 *   Miguel de Icaza (miguel@novell.com)
 *   Aaron Bockover (abockover@novell.com)
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
#include <string.h>
#include <ctype.h>
#include <locale.h>

#include <clib.h>

#ifdef C_HAVE_STRLCPY
#include <bsd/string.h>
#endif

/* This is not a macro, because I dont want to put _GNC_SOURCE in the glib.h
 * header */
char *
c_strndup(const char *str, size_t n)
{
#ifdef C_HAVE_STRNDUP
    return strndup(str, n);
#else
    if (str) {
        char *retval = c_malloc(n + 1);
        if (retval) {
            strncpy(retval, str, n)[n] = 0;
        }
        return retval;
    }
    return NULL;
#endif
}

void
c_strfreev(char **str_array)
{
    char **orig = str_array;
    if (str_array == NULL)
        return;
    while (*str_array != NULL) {
        c_free(*str_array);
        str_array++;
    }
    c_free(orig);
}

char **
c_strdupv(char **str_array)
{
    unsigned int length;
    char **ret;
    unsigned int i;

    if (!str_array)
        return NULL;

    length = c_strv_length(str_array);
    ret = c_new0(char *, length + 1);
    for (i = 0; str_array[i]; i++) {
        ret[i] = c_strdup(str_array[i]);
    }
    ret[length] = NULL;
    return ret;
}

unsigned int
c_strv_length(char **str_array)
{
    int length = 0;
    c_return_val_if_fail(str_array != NULL, 0);
    for (length = 0; str_array[length] != NULL; length++)
        ;
    return length;
}

bool
c_str_has_suffix(const char *str, const char *suffix)
{
    size_t str_length;
    size_t suffix_length;

    c_return_val_if_fail(str != NULL, false);
    c_return_val_if_fail(suffix != NULL, false);

    str_length = strlen(str);
    suffix_length = strlen(suffix);

    return suffix_length <= str_length
           ? strncmp(str + str_length - suffix_length,
                     suffix,
                     suffix_length) == 0
           : false;
}

bool
c_str_has_prefix(const char *str, const char *prefix)
{
    size_t str_length;
    size_t prefix_length;

    c_return_val_if_fail(str != NULL, false);
    c_return_val_if_fail(prefix != NULL, false);

    str_length = strlen(str);
    prefix_length = strlen(prefix);

    return prefix_length <= str_length
           ? strncmp(str, prefix, prefix_length) == 0
           : false;
}

char *
c_strdup_vprintf(const char *format, va_list args)
{
    int n;
    char *ret;

    n = vasprintf(&ret, format, args);
    if (n == -1)
        return NULL;

    return ret;
}

char *
c_strdup_printf(const char *format, ...)
{
    char *ret;
    va_list args;
    int n;

    va_start(args, format);
    n = vasprintf(&ret, format, args);
    va_end(args);
    if (n == -1)
        return NULL;

    return ret;
}

const char *
c_strerror(int errnum)
{
    return strerror(errnum);
}

char *
c_strconcat(const char *first, ...)
{
    va_list args;
    size_t total = 0;
    char *s, *ret;
    c_return_val_if_fail(first != NULL, NULL);

    total += strlen(first);
    va_start(args, first);
    for (s = va_arg(args, char *); s != NULL; s = va_arg(args, char *)) {
        total += strlen(s);
    }
    va_end(args);

    ret = c_malloc(total + 1);
    if (ret == NULL)
        return NULL;

    ret[total] = 0;
    strcpy(ret, first);
    va_start(args, first);
    for (s = va_arg(args, char *); s != NULL; s = va_arg(args, char *)) {
        strcat(ret, s);
    }
    va_end(args);

    return ret;
}

static void
add_to_vector(char ***vector, int size, char *token)
{
    *vector = *vector == NULL
              ? (char **)c_malloc(2 * sizeof(*vector))
              : (char **)c_realloc(*vector, (size + 1) * sizeof(*vector));

    (*vector)[size - 1] = token;
}

char **
c_strsplit(const char *string, const char *delimiter, int max_tokens)
{
    const char *c;
    char *token, **vector;
    int size = 1;

    c_return_val_if_fail(string != NULL, NULL);
    c_return_val_if_fail(delimiter != NULL, NULL);
    c_return_val_if_fail(delimiter[0] != 0, NULL);

    if (strncmp(string, delimiter, strlen(delimiter)) == 0) {
        vector = (char **)c_malloc(2 * sizeof(vector));
        vector[0] = c_strdup("");
        size++;
        string += strlen(delimiter);
    } else {
        vector = NULL;
    }

    while (*string && !(max_tokens > 0 && size >= max_tokens)) {
        c = string;
        if (strncmp(string, delimiter, strlen(delimiter)) == 0) {
            token = c_strdup("");
            string += strlen(delimiter);
        } else {
            while (*string &&
                   strncmp(string, delimiter, strlen(delimiter)) != 0) {
                string++;
            }

            if (*string) {
                size_t toklen = (string - c);
                token = c_strndup(c, toklen);

                /* Need to leave a trailing empty
                 * token if the delimiter is the last
                 * part of the string
                 */
                if (strcmp(string, delimiter) != 0) {
                    string += strlen(delimiter);
                }
            } else {
                token = c_strdup(c);
            }
        }

        add_to_vector(&vector, size, token);
        size++;
    }

    if (*string) {
        if (strcmp(string, delimiter) == 0)
            add_to_vector(&vector, size, c_strdup(""));
        else {
            /* Add the rest of the string as the last element */
            add_to_vector(&vector, size, c_strdup(string));
        }
        size++;
    }

    if (vector == NULL) {
        vector = (char **)c_malloc(2 * sizeof(vector));
        vector[0] = NULL;
    } else if (size > 0) {
        vector[size - 1] = NULL;
    }

    return vector;
}

static bool
charcmp(char testchar, const char *compare)
{
    while (*compare) {
        if (*compare == testchar) {
            return true;
        }
        compare++;
    }

    return false;
}

char **
c_strsplit_set(const char *string, const char *delimiter, int max_tokens)
{
    const char *c;
    char *token, **vector;
    int size = 1;

    c_return_val_if_fail(string != NULL, NULL);
    c_return_val_if_fail(delimiter != NULL, NULL);
    c_return_val_if_fail(delimiter[0] != 0, NULL);

    if (charcmp(*string, delimiter)) {
        vector = (char **)c_malloc(2 * sizeof(vector));
        vector[0] = c_strdup("");
        size++;
        string++;
    } else {
        vector = NULL;
    }

    c = string;
    while (*string && !(max_tokens > 0 && size >= max_tokens)) {
        if (charcmp(*string, delimiter)) {
            size_t toklen = (string - c);
            if (toklen == 0) {
                token = c_strdup("");
            } else {
                token = c_strndup(c, toklen);
            }

            c = string + 1;

            add_to_vector(&vector, size, token);
            size++;
        }

        string++;
    }

    if (max_tokens > 0 && size >= max_tokens) {
        if (*string) {
            /* Add the rest of the string as the last element */
            add_to_vector(&vector, size, c_strdup(string));
            size++;
        }
    } else {
        if (*c) {
            /* Fill in the trailing last token */
            add_to_vector(&vector, size, c_strdup(c));
            size++;
        } else {
            /* Need to leave a trailing empty token if the
             * delimiter is the last part of the string
             */
            add_to_vector(&vector, size, c_strdup(""));
            size++;
        }
    }

    if (vector == NULL) {
        vector = (char **)c_malloc(2 * sizeof(vector));
        vector[0] = NULL;
    } else if (size > 0) {
        vector[size - 1] = NULL;
    }

    return vector;
}

char *
c_strreverse(char *str)
{
    size_t i, j;
    char c;

    if (str == NULL)
        return NULL;

    if (*str == 0)
        return str;

    for (i = 0, j = strlen(str) - 1; i < j; i++, j--) {
        c = str[i];
        str[i] = str[j];
        str[j] = c;
    }

    return str;
}

char *
c_strrstr(const char *haystack, const char *needle)
{
    const char *buffer = haystack;
    const char *last_occurence = NULL;
    size_t needle_size;

    c_return_val_if_fail(haystack, NULL);
    c_return_val_if_fail(needle, NULL);

    needle_size = strlen(needle);

    while (buffer != NULL) {
        buffer = strstr(buffer, needle);
        if (buffer != NULL) {
            last_occurence = buffer;
            buffer += needle_size;
        }
    }

    return (char *)last_occurence;
}

char *
c_strjoin(const char *separator, ...)
{
    va_list args;
    char *res, *s, *r;
    size_t len, slen;

    if (separator != NULL)
        slen = strlen(separator);
    else
        slen = 0;

    len = 0;
    va_start(args, separator);
    for (s = va_arg(args, char *); s != NULL; s = va_arg(args, char *)) {
        len += strlen(s);
        len += slen;
    }
    va_end(args);

    if (len == 0)
        return c_strdup("");

    /* Remove the last separator */
    if (slen > 0 && len > 0)
        len -= slen;

    res = c_malloc(len + 1);
    va_start(args, separator);
    s = va_arg(args, char *);
    r = c_stpcpy(res, s);
    for (s = va_arg(args, char *); s != NULL; s = va_arg(args, char *)) {
        if (separator != NULL)
            r = c_stpcpy(r, separator);
        r = c_stpcpy(r, s);
    }
    va_end(args);

    return res;
}

char *
c_strjoinv(const char *separator, char **str_array)
{
    char *res, *r;
    size_t slen, len, i;

    if (separator != NULL)
        slen = strlen(separator);
    else
        slen = 0;

    len = 0;
    for (i = 0; str_array[i] != NULL; i++) {
        len += strlen(str_array[i]);
        len += slen;
    }

    if (len == 0)
        return c_strdup("");

    if (slen > 0 && len > 0)
        len -= slen;

    res = c_malloc(len + 1);
    r = c_stpcpy(res, str_array[0]);
    for (i = 1; str_array[i] != NULL; i++) {
        if (separator != NULL)
            r = c_stpcpy(r, separator);
        r = c_stpcpy(r, str_array[i]);
    }

    return res;
}

char *
c_strchug(char *str)
{
    size_t len;
    char *tmp;

    if (str == NULL)
        return NULL;

    tmp = str;
    while (*tmp && isspace(*tmp))
        tmp++;
    if (str != tmp) {
        len = strlen(str) - (tmp - str - 1);
        memmove(str, tmp, len);
    }
    return str;
}

char *
c_strchomp(char *str)
{
    char *tmp;

    if (str == NULL)
        return NULL;

    tmp = str + strlen(str) - 1;
    while (*tmp && isspace(*tmp))
        tmp--;
    *(tmp + 1) = '\0';
    return str;
}

int
c_printf(char const *format, ...)
{
    va_list args;
    int ret;

    va_start(args, format);
    ret = vprintf(format, args);
    va_end(args);

    return ret;
}

int
c_fprintf(FILE *file, char const *format, ...)
{
    va_list args;
    int ret;

    va_start(args, format);
    ret = vfprintf(file, format, args);
    va_end(args);

    return ret;
}

int
c_sprintf(char *string, char const *format, ...)
{
    va_list args;
    int ret;

    va_start(args, format);
    ret = vsprintf(string, format, args);
    va_end(args);

    return ret;
}

int
c_snprintf(char *string, unsigned long n, char const *format, ...)
{
    va_list args;
    int ret;

    va_start(args, format);
    ret = vsnprintf(string, n, format, args);
    va_end(args);

    return ret;
}

static const char hx[] = { '0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

static bool
char_needs_encoding(char c)
{
    if (((unsigned char)c) >= 0x80)
        return true;

    if ((c >= '@' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '&' && c < 0x3b) || (c == '!') || (c == '$') || (c == '_') ||
        (c == '=') || (c == '~'))
        return false;
    return true;
}

char *
c_filename_to_uri(const char *filename, const char *hostname, c_error_t **error)
{
    size_t n;
    char *ret, *rp;
    const char *p;
#ifdef WIN32
    const char *uriPrefix = "file:///";
#else
    const char *uriPrefix = "file://";
#endif

    c_return_val_if_fail(filename != NULL, NULL);

    if (hostname != NULL)
        c_warning("%s", "c_filename_to_uri: hostname not handled");

    if (!c_path_is_absolute(filename)) {
        if (error != NULL) {
            *error = c_error_new(C_CONVERT_ERROR,
                                 C_CONVERT_ERROR_NOT_ABSOLUTE_PATH,
                                 "Not an absolute filename");
        }

        return NULL;
    }

    n = strlen(uriPrefix) + 1;
    for (p = filename; *p; p++) {
#ifdef WIN32
        if (*p == '\\') {
            n++;
            continue;
        }
#endif
        if (char_needs_encoding(*p))
            n += 3;
        else
            n++;
    }
    ret = c_malloc(n);
    strcpy(ret, uriPrefix);
    for (p = filename, rp = ret + strlen(ret); *p; p++) {
#ifdef WIN32
        if (*p == '\\') {
            *rp++ = '/';
            continue;
        }
#endif
        if (char_needs_encoding(*p)) {
            *rp++ = '%';
            *rp++ = hx[((unsigned char)(*p)) >> 4];
            *rp++ = hx[((unsigned char)(*p)) & 0xf];
        } else
            *rp++ = *p;
    }
    *rp = 0;
    return ret;
}

static int
decode(char p)
{
    if (p >= '0' && p <= '9')
        return p - '0';
    if (p >= 'A' && p <= 'F')
        return p - 'A';
    if (p >= 'a' && p <= 'f')
        return p - 'a';
    c_assert_not_reached();
    return 0;
}

char *
c_filename_from_uri(const char *uri, char **hostname, c_error_t **error)
{
    const char *p;
    char *r, *result;
    int flen = 0;

    c_return_val_if_fail(uri != NULL, NULL);

    if (hostname != NULL)
        c_warning("%s", "c_filename_from_uri: hostname not handled");

    if (strncmp(uri, "file:///", 8) != 0) {
        if (error != NULL) {
            *error = c_error_new(C_CONVERT_ERROR,
                                 C_CONVERT_ERROR_BAD_URI,
                                 "URI does not start with the file: scheme");
        }
        return NULL;
    }

    for (p = uri + 8; *p; p++) {
        if (*p == '%') {
            if (p[1] && p[2] && isxdigit(p[1]) && isxdigit(p[2])) {
                p += 2;
            } else {
                if (error != NULL) {
                    *error =
                        c_error_new(C_CONVERT_ERROR,
                                    C_CONVERT_ERROR_BAD_URI,
                                    "URI contains an invalid escape sequence");
                }
                return NULL;
            }
        }
        flen++;
    }
#ifndef WIN32
    flen++;
#endif

    result = c_malloc(flen + 1);
    result[flen] = 0;

#ifndef WIN32
    *result = '/';
    r = result + 1;
#else
    r = result;
#endif

    for (p = uri + 8; *p; p++) {
        if (*p == '%') {
            *r++ = (char)((decode(p[1]) << 4) | decode(p[2]));
            p += 2;
        } else
            *r++ = *p;
        flen++;
    }
    return result;
}

void
c_strdown(char *string)
{
    c_return_if_fail(string != NULL);

    while (*string) {
        *string = (char)tolower(*string);
        string++;
    }
}

char
c_ascii_tolower(char c)
{
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

char *
c_ascii_strdown(char *str, c_ssize_t len)
{
    int i;

    c_return_val_if_fail(str != NULL, NULL);

    if (len == -1)
        len = strlen(str);

    for (i = 0; i < len; i++)
        str[i] = c_ascii_tolower(str[i]);

    return str;
}

char
c_ascii_toupper(char c)
{
    return c >= 'a' && c <= 'z' ? c + ('A' - 'a') : c;
}

char *
c_ascii_strup(char *str, c_ssize_t len)
{
    int i;

    c_return_val_if_fail(str != NULL, NULL);

    if (len == -1)
        len = strlen(str);

    for (i = 0; i < len; i++)
        str[i] = c_ascii_toupper(str[i]);

    return str;
}

int
c_ascii_strncasecmp(const char *s1, const char *s2, size_t n)
{
    size_t i;

    c_return_val_if_fail(s1 != NULL, 0);
    c_return_val_if_fail(s2 != NULL, 0);

    for (i = 0; i < n; i++) {
        char c1 = c_ascii_tolower(*s1++);
        char c2 = c_ascii_tolower(*s2++);

        if (c1 != c2)
            return c1 - c2;
    }

    return 0;
}

int
c_ascii_strcasecmp(const char *s1, const char *s2)
{
    const char *sp1 = s1;
    const char *sp2 = s2;

    c_return_val_if_fail(s1 != NULL, 0);
    c_return_val_if_fail(s2 != NULL, 0);

    while (*sp1 != '\0') {
        char c1 = c_ascii_tolower(*sp1++);
        char c2 = c_ascii_tolower(*sp2++);

        if (c1 != c2)
            return c1 - c2;
    }

    return (*sp1) - (*sp2);
}

char *
c_strdelimit(char *string, const char *delimiters, char new_delimiter)
{
    char *ptr;

    c_return_val_if_fail(string != NULL, NULL);

    if (delimiters == NULL)
        delimiters = C_STR_DELIMITERS;

    for (ptr = string; *ptr; ptr++) {
        if (strchr(delimiters, *ptr))
            *ptr = new_delimiter;
    }

    return string;
}

size_t
c_strlcpy(char *dest, const char *src, size_t dest_size)
{
    char *d;
    const char *s;
    char c;
    size_t len;

    c_return_val_if_fail(src != NULL, 0);
    c_return_val_if_fail(dest != NULL, 0);

    len = dest_size;
    if (len == 0)
        return 0;

    s = src;
    d = dest;
    while (--len) {
        c = *s++;
        *d++ = c;
        if (c == '\0')
            return (dest_size - len - 1);
    }

    /* len is 0 i we get here */
    *d = '\0';
    /* we need to return the length of src here */
    while (*s++)
        ; /* instead of a plain strlen, we use 's' */
    return s - src - 1;
}

char *
c_stpcpy(char *dest, const char *src)
{
    c_return_val_if_fail(dest != NULL, dest);
    c_return_val_if_fail(src != NULL, dest);

#if HAVE_STPCPY
    return stpcpy(dest, src);
#else
    while (*src)
        *dest++ = *src++;

    *dest = '\0';

    return dest;
#endif
}

static const char escaped_dflt[256] = {
    1, 1, 1, 1, 1, 1, 1, 1, 'b', 't', 'n', 1, 'f',  'r', 1,   1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,   1,   1,   1, 0,    0,   '"', 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0,   0,   0,   0, 0,    0,   0,   0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0,   0,   0,   0, 0,    0,   0,   0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0,   0,   0,   0, '\\', 0,   0,   0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0,   0,   0,   0, 0,    0,   0,   0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1,   1,   1,   1, 1,    1,   1,   1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,   1,   1,   1, 1,    1,   1,   1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,   1,   1,   1, 1,    1,   1,   1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,   1,   1,   1, 1,    1,   1,   1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,   1,   1,   1, 1,    1,   1,   1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,   1,   1,   1, 1,    1,   1,   1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,   1,   1,   1, 1,    1,   1,   1
};

char *
c_strescape(const char *source, const char *exceptions)
{
    char escaped[256];
    const char *ptr;
    char c;
    char op;
    char *result;
    char *res_ptr;

    c_return_val_if_fail(source != NULL, NULL);

    memcpy(escaped, escaped_dflt, 256);
    if (exceptions != NULL) {
        for (ptr = exceptions; *ptr; ptr++)
            escaped[(int)*ptr] = 0;
    }
    result =
        c_malloc(strlen(source) * 4 + 1); /* Worst case: everything octal. */
    res_ptr = result;
    for (ptr = source; *ptr; ptr++) {
        c = *ptr;
        op = escaped[(int)c];
        if (op == 0) {
            *res_ptr++ = c;
        } else {
            *res_ptr++ = '\\';
            if (op != 1) {
                *res_ptr++ = op;
            } else {
                *res_ptr++ = '0' + ((c >> 6) & 3);
                *res_ptr++ = '0' + ((c >> 3) & 7);
                *res_ptr++ = '0' + (c & 7);
            }
        }
    }
    *res_ptr = '\0';
    return result;
}

int
c_ascii_xdigit_value(char c)
{
    return ((isxdigit(c) == 0) ? -1 : ((c >= '0' && c <= '9')
                                       ? (c - '0')
                                       : ((c >= 'a' && c <= 'f')
                                          ? (c - 'a' + 10)
                                          : (c - 'A' + 10))));
}

char *
c_strnfill(size_t length, char fill_char)
{
    char *ret = c_new(char, length + 1);

    memset(ret, fill_char, length);
    ret[length] = 0;
    return ret;
}

int
c_strcmp0(const char *str1, const char *str2)
{
    if (!str1) {
        if (!str2)
            return 0;
        return -1;
    }

    if (!str2)
        return 1;

    return strcmp(str1, str2);
}

double
c_ascii_strtod(const char *nptr, char **endptr)
{
    char *saved_locale = setlocale(LC_NUMERIC, "C");
    double ret = strtod(nptr, endptr);
    setlocale(LC_NUMERIC, saved_locale);

    return ret;
}
