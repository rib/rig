/*
 * gstr.c: String Utility Functions.
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
#include <config.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <ulib.h>

/* This is not a macro, because I dont want to put _GNU_SOURCE in the glib.h header */
char *
u_strndup (const char *str, size_t n)
{
#ifdef HAVE_STRNDUP
	return strndup (str, n);
#else
	if (str) {
		char *retval = u_malloc(n+1);
		if (retval) {
			strncpy(retval, str, n)[n] = 0;
		}
		return retval;
	}
	return NULL;
#endif
}

void
u_strfreev (char **str_array)
{
	char **orig = str_array;
	if (str_array == NULL)
		return;
	while (*str_array != NULL){
		u_free (*str_array);
		str_array++;
	}
	u_free (orig);
}

char **
u_strdupv (char **str_array)
{
	unsigned int length;
	char **ret;
	unsigned int i;

	if (!str_array)
		return NULL;

	length = u_strv_length(str_array);
	ret = u_new0(char *, length + 1);
	for (i = 0; str_array[i]; i++) {
		ret[i] = u_strdup(str_array[i]);
	}
	ret[length] = NULL;
	return ret;
}

unsigned int
u_strv_length(char **str_array)
{
	int length = 0;
	u_return_val_if_fail(str_array != NULL, 0);
	for(length = 0; str_array[length] != NULL; length++);
	return length;
}

uboolean
u_str_has_suffix(const char *str, const char *suffix)
{
	size_t str_length;
	size_t suffix_length;
	
	u_return_val_if_fail(str != NULL, FALSE);
	u_return_val_if_fail(suffix != NULL, FALSE);

	str_length = strlen(str);
	suffix_length = strlen(suffix);

	return suffix_length <= str_length ?
		strncmp(str + str_length - suffix_length, suffix, suffix_length) == 0 :
		FALSE;
}

uboolean
u_str_has_prefix(const char *str, const char *prefix)
{
	size_t str_length;
	size_t prefix_length;
	
	u_return_val_if_fail(str != NULL, FALSE);
	u_return_val_if_fail(prefix != NULL, FALSE);

	str_length = strlen(str);
	prefix_length = strlen(prefix);

	return prefix_length <= str_length ?
		strncmp(str, prefix, prefix_length) == 0 :
		FALSE;
}

char *
u_strdup_vprintf (const char *format, va_list args)
{
	int n;
	char *ret;
	
	n = vasprintf (&ret, format, args);
	if (n == -1)
		return NULL;

	return ret;
}

char *
u_strdup_printf (const char *format, ...)
{
	char *ret;
	va_list args;
	int n;

	va_start (args, format);
	n = vasprintf (&ret, format, args);
	va_end (args);
	if (n == -1)
		return NULL;

	return ret;
}

const char *
u_strerror (int errnum)
{
	return strerror (errnum);
}

char *
u_strconcat (const char *first, ...)
{
	va_list args;
	size_t total = 0;
	char *s, *ret;
	u_return_val_if_fail (first != NULL, NULL);

	total += strlen (first);
	va_start (args, first);
	for (s = va_arg (args, char *); s != NULL; s = va_arg(args, char *)){
		total += strlen (s);
	}
	va_end (args);
	
	ret = u_malloc (total + 1);
	if (ret == NULL)
		return NULL;

	ret [total] = 0;
	strcpy (ret, first);
	va_start (args, first);
	for (s = va_arg (args, char *); s != NULL; s = va_arg(args, char *)){
		strcat (ret, s);
	}
	va_end (args);

	return ret;
}

static void
add_to_vector (char ***vector, int size, char *token)
{
	*vector = *vector == NULL ? 
		(char **)u_malloc(2 * sizeof(*vector)) :
		(char **)u_realloc(*vector, (size + 1) * sizeof(*vector));
		
	(*vector)[size - 1] = token;
}

char ** 
u_strsplit (const char *string, const char *delimiter, int max_tokens)
{
	const char *c;
	char *token, **vector;
	int size = 1;
	
	u_return_val_if_fail (string != NULL, NULL);
	u_return_val_if_fail (delimiter != NULL, NULL);
	u_return_val_if_fail (delimiter[0] != 0, NULL);
	
	if (strncmp (string, delimiter, strlen (delimiter)) == 0) {
		vector = (char **)u_malloc (2 * sizeof(vector));
		vector[0] = u_strdup ("");
		size++;
		string += strlen (delimiter);
	} else {
		vector = NULL;
	}

	while (*string && !(max_tokens > 0 && size >= max_tokens)) {
		c = string;
		if (strncmp (string, delimiter, strlen (delimiter)) == 0) {
			token = u_strdup ("");
			string += strlen (delimiter);
		} else {
			while (*string && strncmp (string, delimiter, strlen (delimiter)) != 0) {
				string++;
			}

			if (*string) {
				size_t toklen = (string - c);
				token = u_strndup (c, toklen);

				/* Need to leave a trailing empty
				 * token if the delimiter is the last
				 * part of the string
				 */
				if (strcmp (string, delimiter) != 0) {
					string += strlen (delimiter);
				}
			} else {
				token = u_strdup (c);
			}
		}
			
		add_to_vector (&vector, size, token);
		size++;
	}

	if (*string) {
		if (strcmp (string, delimiter) == 0)
			add_to_vector (&vector, size, u_strdup (""));
		else {
			/* Add the rest of the string as the last element */
			add_to_vector (&vector, size, u_strdup (string));
		}
		size++;
	}
	
	if (vector == NULL) {
		vector = (char **) u_malloc (2 * sizeof (vector));
		vector [0] = NULL;
	} else if (size > 0) {
		vector[size - 1] = NULL;
	}
	
	return vector;
}

static uboolean
charcmp (char testchar, const char *compare)
{
	while(*compare) {
		if (*compare == testchar) {
			return TRUE;
		}
		compare++;
	}
	
	return FALSE;
}

char ** 
u_strsplit_set (const char *string, const char *delimiter, int max_tokens)
{
	const char *c;
	char *token, **vector;
	int size = 1;
	
	u_return_val_if_fail (string != NULL, NULL);
	u_return_val_if_fail (delimiter != NULL, NULL);
	u_return_val_if_fail (delimiter[0] != 0, NULL);
	
	if (charcmp (*string, delimiter)) {
		vector = (char **)u_malloc (2 * sizeof(vector));
		vector[0] = u_strdup ("");
		size++;
		string++;
	} else {
		vector = NULL;
	}

	c = string;
	while (*string && !(max_tokens > 0 && size >= max_tokens)) {
		if (charcmp (*string, delimiter)) {
			size_t toklen = (string - c);
			if (toklen == 0) {
				token = u_strdup ("");
			} else {
				token = u_strndup (c, toklen);
			}
			
			c = string + 1;
			
			add_to_vector (&vector, size, token);
			size++;
		}

		string++;
	}
	
	if (max_tokens > 0 && size >= max_tokens) {
		if (*string) {
			/* Add the rest of the string as the last element */
			add_to_vector (&vector, size, u_strdup (string));
			size++;
		}
	} else {
		if (*c) {
			/* Fill in the trailing last token */
			add_to_vector (&vector, size, u_strdup (c));
			size++;
		} else {
			/* Need to leave a trailing empty token if the
			 * delimiter is the last part of the string
			 */
			add_to_vector (&vector, size, u_strdup (""));
			size++;
		}
	}
	
	if (vector == NULL) {
		vector = (char **) u_malloc (2 * sizeof (vector));
		vector [0] = NULL;
	} else if (size > 0) {
		vector[size - 1] = NULL;
	}
	
	return vector;
}

char *
u_strreverse (char *str)
{
	size_t i, j;
	char c;

	if (str == NULL)
		return NULL;

	if (*str == 0)
		return str;

	for (i = 0, j = strlen (str) - 1; i < j; i++, j--) {
		c = str [i];
		str [i] = str [j];
		str [j] = c;
	}

	return str;
}

char *
u_strjoin (const char *separator, ...)
{
	va_list args;
	char *res, *s, *r;
	size_t len, slen;

	if (separator != NULL)
		slen = strlen (separator);
	else
		slen = 0;
	
	len = 0;
	va_start (args, separator);
	for (s = va_arg (args, char *); s != NULL; s = va_arg (args, char *)){
		len += strlen (s);
		len += slen;
	}
	va_end (args);

	if (len == 0)
		return u_strdup ("");
	
	/* Remove the last separator */
	if (slen > 0 && len > 0)
		len -= slen;

	res = u_malloc (len + 1);
	va_start (args, separator);
	s = va_arg (args, char *);
	r = u_stpcpy (res, s);
	for (s = va_arg (args, char *); s != NULL; s = va_arg (args, char *)){
		if (separator != NULL)
			r = u_stpcpy (r, separator);
		r = u_stpcpy (r, s);
	}
	va_end (args);

	return res;
}

char *
u_strjoinv (const char *separator, char **str_array)
{
	char *res, *r;
	size_t slen, len, i;
	
	if (separator != NULL)
		slen = strlen (separator);
	else
		slen = 0;
	
	len = 0;
	for (i = 0; str_array [i] != NULL; i++){
		len += strlen (str_array [i]);
		len += slen;
	}

	if (len == 0)
		return u_strdup ("");

	if (slen > 0 && len > 0)
		len -= slen;

	res = u_malloc (len + 1);
	r = u_stpcpy (res, str_array [0]);
	for (i = 1; str_array [i] != NULL; i++){
		if (separator != NULL)
			r = u_stpcpy (r, separator);
		r = u_stpcpy (r, str_array [i]);
	}

	return res;
}

char *
u_strchug (char *str)
{
	size_t len;
	char *tmp;

	if (str == NULL)
		return NULL;

	tmp = str;
	while (*tmp && isspace (*tmp)) tmp++;
	if (str != tmp) {
		len = strlen (str) - (tmp - str - 1);
		memmove (str, tmp, len);
	}
	return str;
}

char *
u_strchomp (char *str)
{
	char *tmp;

	if (str == NULL)
		return NULL;

	tmp = str + strlen (str) - 1;
	while (*tmp && isspace (*tmp)) tmp--;
	*(tmp + 1) = '\0';
	return str;
}

int
u_printf(char const *format, ...)
{
	va_list args;
	int ret;

	va_start(args, format);
	ret = vprintf(format, args);
	va_end(args);

	return ret;
}

int
u_fprintf(FILE *file, char const *format, ...)
{
	va_list args;
	int ret;

	va_start(args, format);
	ret = vfprintf(file, format, args);
	va_end(args);

	return ret;
}

int
u_sprintf(char *string, char const *format, ...)
{
	va_list args;
	int ret;

	va_start(args, format);
	ret = vsprintf(string, format, args);
	va_end(args);

	return ret;
}

int
u_snprintf(char *string, unsigned long n, char const *format, ...)
{
	va_list args;
	int ret;
	
	va_start(args, format);
	ret = vsnprintf(string, n, format, args);
	va_end(args);

	return ret;
}

static const char hx [] = { '0', '1', '2', '3', '4', '5', '6', '7',
				  '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

static uboolean
char_needs_encoding (char c)
{
	if (((unsigned char)c) >= 0x80)
		return TRUE;
	
	if ((c >= '@' && c <= 'Z') ||
	    (c >= 'a' && c <= 'z') ||
	    (c >= '&' && c < 0x3b) ||
	    (c == '!') || (c == '$') || (c == '_') || (c == '=') || (c == '~'))
		return FALSE;
	return TRUE;
}

char *
u_filename_to_uri (const char *filename, const char *hostname, UError **error)
{
	size_t n;
	char *ret, *rp;
	const char *p;
#ifdef U_OS_WIN32
	const char *uriPrefix = "file:///";
#else
	const char *uriPrefix = "file://";
#endif
	
	u_return_val_if_fail (filename != NULL, NULL);

	if (hostname != NULL)
		u_warning ("%s", "eglib: u_filename_to_uri: hostname not handled");

	if (!u_path_is_absolute (filename)){
		if (error != NULL) {
			*error = u_error_new (U_CONVERT_ERROR,
                                              U_CONVERT_ERROR_NOT_ABSOLUTE_PATH,
                                              "Not an absolute filename");
                }
		
		return NULL;
	}
	
	n = strlen (uriPrefix) + 1;
	for (p = filename; *p; p++){
#ifdef U_OS_WIN32
		if (*p == '\\') {
			n++;
			continue;
		}
#endif
		if (char_needs_encoding (*p))
			n += 3;
		else
			n++;
	}
	ret = u_malloc (n);
	strcpy (ret, uriPrefix);
	for (p = filename, rp = ret + strlen (ret); *p; p++){
#ifdef U_OS_WIN32
		if (*p == '\\') {
			*rp++ = '/';
			continue;
		}
#endif
		if (char_needs_encoding (*p)){
			*rp++ = '%';
			*rp++ = hx [((unsigned char)(*p)) >> 4];
			*rp++ = hx [((unsigned char)(*p)) & 0xf];
		} else
			*rp++ = *p;
	}
	*rp = 0;
	return ret;
}

static int
decode (char p)
{
	if (p >= '0' && p <= '9')
		return p - '0';
	if (p >= 'A' && p <= 'F')
		return p - 'A';
	if (p >= 'a' && p <= 'f')
		return p - 'a';
	u_assert_not_reached ();
	return 0;
}

char *
u_filename_from_uri (const char *uri, char **hostname, UError **error)
{
	const char *p;
	char *r, *result;
	int flen = 0;
	
	u_return_val_if_fail (uri != NULL, NULL);

	if (hostname != NULL)
		u_warning ("%s", "eglib: u_filename_from_uri: hostname not handled");

	if (strncmp (uri, "file:///", 8) != 0){
		if (error != NULL) {
			*error = u_error_new (U_CONVERT_ERROR,
                                              U_CONVERT_ERROR_BAD_URI,
                                              "URI does not start with the file: scheme");
                }
		return NULL;
	}

	for (p = uri + 8; *p; p++){
		if (*p == '%'){
			if (p [1] && p [2] && isxdigit (p [1]) && isxdigit (p [2])){
				p += 2;
			} else {
				if (error != NULL) {
					*error = u_error_new (U_CONVERT_ERROR,
                                                              U_CONVERT_ERROR_BAD_URI,
                                                              "URI contains an invalid escape sequence");
                                }
				return NULL;
			}
		} 
		flen++;
	}
#ifndef U_OS_WIN32
	flen++;
#endif

	result = u_malloc (flen + 1);
	result [flen] = 0;

#ifndef U_OS_WIN32
	*result = '/';
	r = result + 1;
#else
	r = result;
#endif

	for (p = uri + 8; *p; p++){
		if (*p == '%'){
			*r++ = (char)((decode (p [1]) << 4) | decode (p [2]));
			p += 2;
		} else
			*r++ = *p;
		flen++;
	}
	return result;
}

void
u_strdown (char *string)
{
	u_return_if_fail (string != NULL);

	while (*string){
		*string = (char)tolower (*string);
		string++;
	}
}

char
u_ascii_tolower (char c)
{
	return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

char *
u_ascii_strdown (const char *str, ussize len)
{
	char *ret;
	int i;
	
	u_return_val_if_fail  (str != NULL, NULL);

	if (len == -1)
		len = strlen (str);
	
	ret = u_malloc (len + 1);
	for (i = 0; i < len; i++)
		ret [i] = (unsigned char) u_ascii_tolower (str [i]);
	ret [i] = 0;
	
	return ret;
}

char
u_ascii_toupper (char c)
{
	return c >= 'a' && c <= 'z' ? c + ('A' - 'a') : c;
}

char *
u_ascii_strup (const char *str, ussize len)
{
	char *ret;
	int i;
	
	u_return_val_if_fail  (str != NULL, NULL);

	if (len == -1)
		len = strlen (str);
	
	ret = u_malloc (len + 1);
	for (i = 0; i < len; i++)
		ret [i] = (unsigned char) u_ascii_toupper (str [i]);
	ret [i] = 0;
	
	return ret;
}

int
u_ascii_strncasecmp (const char *s1, const char *s2, size_t n)
{
	size_t i;
	
	u_return_val_if_fail (s1 != NULL, 0);
	u_return_val_if_fail (s2 != NULL, 0);

	for (i = 0; i < n; i++) {
		char c1 = u_ascii_tolower (*s1++);
		char c2 = u_ascii_tolower (*s2++);
		
		if (c1 != c2)
			return c1 - c2;
	}
	
	return 0;
}

int
u_ascii_strcasecmp (const char *s1, const char *s2)
{
	const char *sp1 = s1;
	const char *sp2 = s2;
	
	u_return_val_if_fail (s1 != NULL, 0);
	u_return_val_if_fail (s2 != NULL, 0);
	
	while (*sp1 != '\0') {
		char c1 = u_ascii_tolower (*sp1++);
		char c2 = u_ascii_tolower (*sp2++);
		
		if (c1 != c2)
			return c1 - c2;
	}
	
	return (*sp1) - (*sp2);
}

char *
u_strdelimit (char *string, const char *delimiters, char new_delimiter)
{
	char *ptr;

	u_return_val_if_fail (string != NULL, NULL);

	if (delimiters == NULL)
		delimiters = U_STR_DELIMITERS;

	for (ptr = string; *ptr; ptr++) {
		if (strchr (delimiters, *ptr))
			*ptr = new_delimiter;
	}
	
	return string;
}

size_t 
u_strlcpy (char *dest, const char *src, size_t dest_size)
{
#ifdef HAVE_STRLCPY
	return strlcpy (dest, src, dest_size);
#else
	char *d;
	const char *s;
	char c;
	size_t len;
	
	u_return_val_if_fail (src != NULL, 0);
	u_return_val_if_fail (dest != NULL, 0);

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
	while (*s++) ; /* instead of a plain strlen, we use 's' */
	return s - src - 1;
#endif
}

char *
u_stpcpy (char *dest, const char *src)
{
	u_return_val_if_fail (dest != NULL, dest);
	u_return_val_if_fail (src != NULL, dest);

#if HAVE_STPCPY
	return stpcpy (dest, src);
#else
	while (*src)
		*dest++ = *src++;
	
	*dest = '\0';
	
	return dest;
#endif
}

static const char escaped_dflt [256] = {
	1, 1, 1, 1, 1, 1, 1, 1, 'b', 't', 'n', 1, 'f', 'r', 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	0, 0, '"', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '\\', 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

char *
u_strescape (const char *source, const char *exceptions)
{
	char escaped [256];
	const char *ptr;
	char c;
	char op;
	char *result;
	char *res_ptr;

	u_return_val_if_fail (source != NULL, NULL);

	memcpy (escaped, escaped_dflt, 256);
	if (exceptions != NULL) {
		for (ptr = exceptions; *ptr; ptr++)
			escaped [(int) *ptr] = 0;
	}
	result = u_malloc (strlen (source) * 4 + 1); /* Worst case: everything octal. */
	res_ptr = result;
	for (ptr = source; *ptr; ptr++) {
		c = *ptr;
		op = escaped [(int) c];
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
u_ascii_xdigit_value (char c)
{
	return ((isxdigit (c) == 0) ? -1 :
		((c >= '0' && c <= '9') ? (c - '0') :
		 ((c >= 'a' && c <= 'f') ? (c - 'a' + 10) :
		  (c - 'A' + 10))));
}

char *
u_strnfill (size_t length, char fill_char)
{
	char *ret = u_new (char, length + 1);

	memset (ret, fill_char, length);
	ret [length] = 0;
	return ret;
}
