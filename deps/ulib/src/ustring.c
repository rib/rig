/*
 * String functions
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
#include <ulib.h>

#define UROW_IF_NECESSARY(s,l) { \
	if(s->len + l >= s->allocated_len) { \
		s->allocated_len = (s->allocated_len + l + 16) * 2; \
		s->str = u_realloc(s->str, s->allocated_len); \
	} \
}

UString *
u_string_new_len (const char *init, ussize len)
{
	UString *ret = u_new (UString, 1);

	if (init == NULL)
		ret->len = 0;
	else
		ret->len = len < 0 ? strlen(init) : len;
	ret->allocated_len = MAX(ret->len + 1, 16);
	ret->str = u_malloc(ret->allocated_len);
	if (init)
		memcpy(ret->str, init, ret->len);
	ret->str[ret->len] = 0;

	return ret;
}

UString *
u_string_new (const char *init)
{
	return u_string_new_len(init, -1);
}

UString *
u_string_sized_new (size_t default_size)
{
	UString *ret = u_new (UString, 1);

	ret->str = u_malloc (default_size);
	ret->str [0] = 0;
	ret->len = 0;
	ret->allocated_len = default_size;

	return ret;
}

char *
u_string_free (UString *string, uboolean free_segment)
{
	char *data;
	
	u_return_val_if_fail (string != NULL, NULL);

	data = string->str;
	u_free(string);
	
	if(!free_segment) {
		return data;
	}

	u_free(data);
	return NULL;
}

UString *
u_string_assign (UString *string, const char *val)
{
	u_return_val_if_fail(string != NULL, NULL);
	u_return_val_if_fail(val != NULL, string);
	
        if (string->str == val)
          return string;

        u_string_truncate (string, 0);
        u_string_append (string, val);
        return string;
}

UString *
u_string_append_len (UString *string, const char *val, ussize len)
{
	u_return_val_if_fail(string != NULL, NULL);
	u_return_val_if_fail(val != NULL, string);

	if(len < 0) {
		len = strlen(val);
	}

	UROW_IF_NECESSARY(string, len);
	memcpy(string->str + string->len, val, len);
	string->len += len;
	string->str[string->len] = 0;

	return string;
}

UString *
u_string_append (UString *string, const char *val)
{
	u_return_val_if_fail(string != NULL, NULL);
	u_return_val_if_fail(val != NULL, string);

	return u_string_append_len(string, val, -1);
}

UString *
u_string_append_c (UString *string, char c)
{
	u_return_val_if_fail(string != NULL, NULL);

	UROW_IF_NECESSARY(string, 1);
	
	string->str[string->len] = c;
	string->str[string->len + 1] = 0;
	string->len++;

	return string;
}

UString *
u_string_append_unichar (UString *string, uunichar c)
{
	char utf8[6];
	int len;
	
	u_return_val_if_fail (string != NULL, NULL);
	
	if ((len = u_unichar_to_utf8 (c, utf8)) <= 0)
		return string;
	
	return u_string_append_len (string, utf8, len);
}

UString *
u_string_prepend (UString *string, const char *val)
{
	ussize len;
	
	u_return_val_if_fail (string != NULL, string);
	u_return_val_if_fail (val != NULL, string);

	len = strlen (val);
	
	UROW_IF_NECESSARY(string, len);	
	memmove(string->str + len, string->str, string->len + 1);
	memcpy(string->str, val, len);

	return string;
}

UString *
u_string_insert (UString *string, ussize pos, const char *val)
{
	ussize len;
	
	u_return_val_if_fail (string != NULL, string);
	u_return_val_if_fail (val != NULL, string);
	u_return_val_if_fail (pos <= string->len, string);

	len = strlen (val);
	
	UROW_IF_NECESSARY(string, len);	
	memmove(string->str + pos + len, string->str + pos, string->len - pos - len + 1);
	memcpy(string->str + pos, val, len);

	return string;
}

void
u_string_append_printf (UString *string, const char *format, ...)
{
	char *ret;
	va_list args;
	
	u_return_if_fail (string != NULL);
	u_return_if_fail (format != NULL);

	va_start (args, format);
	ret = u_strdup_vprintf (format, args);
	va_end (args);
	u_string_append (string, ret);

	u_free (ret);
}

void
u_string_append_vprintf (UString *string, const char *format, va_list args)
{
	char *ret;

	u_return_if_fail (string != NULL);
	u_return_if_fail (format != NULL);

	ret = u_strdup_vprintf (format, args);
	u_string_append (string, ret);
	u_free (ret);
}

void
u_string_printf (UString *string, const char *format, ...)
{
	va_list args;
	
	u_return_if_fail (string != NULL);
	u_return_if_fail (format != NULL);

	u_free (string->str);
	
	va_start (args, format);
	string->str = u_strdup_vprintf (format, args);
	va_end (args);

	string->len = strlen (string->str);
	string->allocated_len = string->len+1;
}

UString *
u_string_truncate (UString *string, size_t len)
{
	u_return_val_if_fail (string != NULL, string);

	/* Silent return */
	if (len >= string->len)
		return string;
	
	string->len = len;
	string->str[len] = 0;
	return string;
}

UString *
u_string_set_size (UString *string, size_t len)
{
	u_return_val_if_fail (string != NULL, string);

	UROW_IF_NECESSARY(string, len);
	
	string->len = len;
	string->str[len] = 0;
	return string;
}

UString *
u_string_erase (UString *string, ussize pos, ussize len)
{
	u_return_val_if_fail (string != NULL, string);

	/* Silent return */
	if (pos >= string->len)
		return string;

	if (len == -1 || (pos + len) >= string->len) {
		string->str[pos] = 0;
	}
	else {
		memmove (string->str + pos, string->str + pos + len, string->len - (pos + len) + 1);
		string->len -= len;
	}

	return string;
}
