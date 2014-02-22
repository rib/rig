/*
 * gunicode.c: Some Unicode routines
 *
 * Author:
 *   Miguel de Icaza (miguel@novell.com)
 *
 * (C) 2006 Novell, Inc.
 *
 * utf8 validation code came from:
 * 	libxml2-2.6.26 licensed under the MIT X11 license
 *
 * Authors credit in libxml's string.c:
 *   William Brack <wbrack@mmm.com.hk>
 *   daniel@veillard.com
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
 *
 */
#include <config.h>
#include <stdio.h>
#include <ulib.h>
#include <unicode-data.h>
#include <errno.h>

#if defined(_MSC_VER) || defined(U_OS_WIN32)
/* FIXME */
#  define CODESET 1
#  include <windows.h>
#else
#    ifdef HAVE_LANGINFO_H
#       include <langinfo.h>
#    endif
#    ifdef HAVE_LOCALCHARSET_H
#       include <localcharset.h>
#    endif
#endif

static const char *my_charset;
static uboolean is_utf8;

/*
 * Character set conversion
 */

UUnicodeType
u_unichar_type (uunichar c)
{
	int i;

	uint16_t cp = (uint16_t) c;
	for (i = 0; i < unicode_category_ranges_count; i++) {
		if (cp < unicode_category_ranges [i].start)
			continue;
		if (unicode_category_ranges [i].end <= cp)
			continue;
		return unicode_category [i] [cp - unicode_category_ranges [i].start];
	}

	/*
	// 3400-4DB5: OtherLetter
	// 4E00-9FC3: OtherLetter
	// AC00-D7A3: OtherLetter
	// D800-DFFF: OtherSurrogate
	// E000-F8FF: OtherPrivateUse
	// 20000-2A6D6 OtherLetter
	// F0000-FFFFD OtherPrivateUse
	// 100000-10FFFD OtherPrivateUse
	*/
	if (0x3400 <= cp && cp < 0x4DB5)
		return U_UNICODE_OTHER_LETTER;
	if (0x4E00 <= cp && cp < 0x9FC3)
		return U_UNICODE_OTHER_LETTER;
	if (0xAC00<= cp && cp < 0xD7A3)
		return U_UNICODE_OTHER_LETTER;
	if (0xD800 <= cp && cp < 0xDFFF)
		return U_UNICODE_SURROGATE;
	if (0xE000 <= cp && cp < 0xF8FF)
		return U_UNICODE_PRIVATE_USE;
	/* since the argument is UTF-16, we cannot check beyond FFFF */

	/* It should match any of above */
	return 0;
}

UUnicodeBreakType
u_unichar_break_type (uunichar c)
{
	// MOONLIGHT_FIXME
	return U_UNICODE_BREAK_UNKNOWN;
}

uunichar
u_unichar_case (uunichar c, uboolean upper)
{
	int8_t i, i2;
	uint32_t cp = (uint32_t) c, v;

	for (i = 0; i < simple_case_map_ranges_count; i++) {
		if (cp < simple_case_map_ranges [i].start)
			return c;
		if (simple_case_map_ranges [i].end <= cp)
			continue;
		if (c < 0x10000) {
			const uint16_t *tab = upper ? simple_upper_case_mapping_lowarea [i] : simple_lower_case_mapping_lowarea [i];
			v = tab [cp - simple_case_map_ranges [i].start];
		} else {
			const uint32_t *tab;
			i2 = (int8_t)(i - (upper ? simple_upper_case_mapping_lowarea_table_count : simple_lower_case_mapping_lowarea_table_count));
			tab = upper ? simple_upper_case_mapping_higharea [i2] : simple_lower_case_mapping_higharea [i2];
			v = tab [cp - simple_case_map_ranges [i].start];
		}
		return v != 0 ? (uunichar) v : c;
	}
	return c;
}

uunichar
u_unichar_toupper (uunichar c)
{
	return u_unichar_case (c, TRUE);
}

uunichar
u_unichar_tolower (uunichar c)
{
	return u_unichar_case (c, FALSE);
}

uunichar
u_unichar_totitle (uunichar c)
{
	uint8_t i;
	uint32_t cp;

	cp = (uint32_t) c;
	for (i = 0; i < simple_titlecase_mapping_count; i++) {
		if (simple_titlecase_mapping [i].codepoint == cp)
			return simple_titlecase_mapping [i].title;
		if (simple_titlecase_mapping [i].codepoint > cp)
			/* it is ordered, hence no more match */
			break;
	}
	return u_unichar_toupper (c);
}

uboolean
u_unichar_isxdigit (uunichar c)
{
	return (u_unichar_xdigit_value (c) != -1);

}

int
u_unichar_xdigit_value (uunichar c)
{
	if (c >= 0x30 && c <= 0x39) /*0-9*/
		return (c - 0x30);
	if (c >= 0x41 && c <= 0x46) /*A-F*/
		return (c - 0x37);
	if (c >= 0x61 && c <= 0x66) /*a-f*/
		return (c - 0x57);
	return -1;
}

uboolean
u_unichar_isspace (uunichar c)
{
	UUnicodeType type = u_unichar_type (c);
	if (type == U_UNICODE_LINE_SEPARATOR ||
	    type == U_UNICODE_PARAGRAPH_SEPARATOR ||
	    type == U_UNICODE_SPACE_SEPARATOR)
		return TRUE;

	return FALSE;
}


/*
 * This is broken, and assumes an UTF8 system, but will do for eglib's first user
 */
char *
u_filename_from_utf8 (const char *utf8string, ussize len, size_t *bytes_read, size_t *bytes_written, UError **error)
{
	char *res;

	if (len == -1)
		len = strlen (utf8string);

	res = u_malloc (len + 1);
	u_strlcpy (res, utf8string, len + 1);
	return res;
}

uboolean
u_get_charset (U_CONST_RETURN char **charset)
{
	if (my_charset == NULL) {
#ifdef U_OS_WIN32
		static char buf [14];
		sprintf (buf, "CP%u", UetACP ());
		my_charset = buf;
		is_utf8 = FALSE;
#else
		/* These shouldn't be heap allocated */
#if HAVE_LOCALCHARSET_H
		my_charset = locale_charset ();
#elif defined(HAVE_LANGINFO_H)
		my_charset = nl_langinfo (CODESET);
#else
		my_charset = "UTF-8";
#endif
		is_utf8 = strcmp (my_charset, "UTF-8") == 0;
#endif
	}

	if (charset != NULL)
		*charset = my_charset;

	return is_utf8;
}

char *
u_locale_to_utf8 (const char *opsysstring, ussize len, size_t *bytes_read, size_t *bytes_written, UError **error)
{
	u_get_charset (NULL);

	return u_convert (opsysstring, len, "UTF-8", my_charset, bytes_read, bytes_written, error);
}

char *
u_locale_from_utf8 (const char *utf8string, ussize len, size_t *bytes_read, size_t *bytes_written, UError **error)
{
	u_get_charset (NULL);

	return u_convert (utf8string, len, my_charset, "UTF-8", bytes_read, bytes_written, error);
}
