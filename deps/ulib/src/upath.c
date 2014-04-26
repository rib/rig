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
#include <ulib.h>
#include <errno.h>

#ifdef U_OS_WIN32
#include <direct.h> 
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

char *
u_build_path (const char *separator, const char *first_element, ...)
{
	const char *elem, *next, *endptr;
	uboolean trimmed;
	UString *path;
	va_list args;
	size_t slen;
	
	u_return_val_if_fail (separator != NULL, NULL);
	
	path = u_string_sized_new (48);
	slen = strlen (separator);
	
	va_start (args, first_element);
	for (elem = first_element; elem != NULL; elem = next) {
		/* trim any trailing separators from @elem */
		endptr = elem + strlen (elem);
		trimmed = FALSE;
		
		while (endptr >= elem + slen) {
			if (strncmp (endptr - slen, separator, slen) != 0)
				break;
			
			endptr -= slen;
			trimmed = TRUE;
		}
		
		/* append elem, not including any trailing separators */
		if (endptr > elem)
			u_string_append_len (path, elem, endptr - elem);
		
		/* get the next element */
		do {
			if (!(next = va_arg (args, char *)))
				break;
			
			/* remove leading separators */
			while (!strncmp (next, separator, slen))
				next += slen;
		} while (*next == '\0');
		
		if (next || trimmed)
			u_string_append_len (path, separator, slen);
	}
	va_end (args);
	
	return u_string_free (path, FALSE);
}

static char*
strrchr_seperator (const char* filename)
{
#ifdef U_OS_WIN32
	char *p2;
#endif
	char *p;

	p = strrchr (filename, U_DIR_SEPARATOR);
#ifdef U_OS_WIN32
	p2 = strrchr (filename, '/');
	if (p2 > p)
		p = p2;
#endif

	return p;
}

char *
u_path_get_dirname (const char *filename)
{
	char *p, *r;
	size_t count;
	u_return_val_if_fail (filename != NULL, NULL);

	p = strrchr_seperator (filename);
	if (p == NULL)
		return u_strdup (".");
	if (p == filename)
		return u_strdup ("/");
	count = p - filename;
	r = u_malloc (count + 1);
	strncpy (r, filename, count);
	r [count] = 0;

	return r;
}

char *
u_path_get_basename (const char *filename)
{
	char *r;
	u_return_val_if_fail (filename != NULL, NULL);

	/* Empty filename -> . */
	if (!*filename)
		return u_strdup (".");

	/* No separator -> filename */
	r = strrchr_seperator (filename);
	if (r == NULL)
		return u_strdup (filename);

	/* Trailing slash, remove component */
	if (r [1] == 0){
		char *copy = u_strdup (filename);
		copy [r-filename] = 0;
		r = strrchr_seperator (copy);

		if (r == NULL){
			u_free (copy);			
			return u_strdup ("/");
		}
		r = u_strdup (&r[1]);
		u_free (copy);
		return r;
	}

	return u_strdup (&r[1]);
}

#ifndef HAVE_STRTOK_R
// This is from BSD's strtok_r

char *
strtok_r(char *s, const char *delim, char **last)
{
	char *spanp;
	int c, sc;
	char *tok;
	
	if (s == NULL && (s = *last) == NULL)
		return NULL;
	
	/*
	 * Skip (span) leading delimiters (s += strspn(s, delim), sort of).
	 */
cont:
	c = *s++;
	for (spanp = (char *)delim; (sc = *spanp++) != 0; ){
		if (c == sc)
			goto cont;
	}

	if (c == 0){         /* no non-delimiter characters */
		*last = NULL;
		return NULL;
	}
	tok = s - 1;

	/*
	 * Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
	 * Note that delim must have one NUL; we stop if we see that, too.
	 */
	for (;;){
		c = *s++;
		spanp = (char *)delim;
		do {
			if ((sc = *spanp++) == c) {
				if (c == 0)
					s = NULL;
				else {
					char *w = s - 1;
					*w = '\0';
				}
				*last = s;
				return tok;
			}
		}
		while (sc != 0);
	}
	/* NOTREACHED */
}
#endif

char *
u_find_program_in_path (const char *program)
{
	char *p;
	char *x, *l;
	char *curdir = NULL;
	char *save = NULL;
#ifdef U_OS_WIN32
	char *program_exe;
	char *suffix_list[5] = {".exe",".cmd",".bat",".com",NULL};
	int listx;
	uboolean hasSuffix;
#endif

	u_return_val_if_fail (program != NULL, NULL);
	x = p = u_strdup (u_getenv ("PATH"));

	if (x == NULL || *x == '\0') {
		curdir = u_get_current_dir ();
		x = curdir;
	}

#ifdef U_OS_WIN32
	/* see if program already has a suffix */
	listx = 0;
	hasSuffix = FALSE;
	while (!hasSuffix && suffix_list[listx]) {
		hasSuffix = u_str_has_suffix(program,suffix_list[listx++]);
	}
#endif

	while ((l = strtok_r (x, U_SEARCHPATH_SEPARATOR_S, &save)) != NULL){
		char *probe_path; 
		
		x = NULL;
		probe_path = u_build_path (U_DIR_SEPARATOR_S, l, program, NULL);
		if (access (probe_path, X_OK) == 0){ /* FIXME: on windows this is just a read permissions test */
			u_free (curdir);
			u_free (p);
			return probe_path;
		}
		u_free (probe_path);

#ifdef U_OS_WIN32
		/* check for program with a suffix attached */
		if (!hasSuffix) {
			listx = 0;
			while (suffix_list[listx]) {
				program_exe = u_strjoin(NULL,program,suffix_list[listx],NULL);
				probe_path = u_build_path (U_DIR_SEPARATOR_S, l, program_exe, NULL);
				if (access (probe_path, X_OK) == 0){ /* FIXME: on windows this is just a read permissions test */
					u_free (curdir);
					u_free (p);
					u_free (program_exe);
					return probe_path;
				}
				listx++;
				u_free (probe_path);
				u_free (program_exe);
			}
		}
#endif
	}
	u_free (curdir);
	u_free (p);
	return NULL;
}

static char *name;

void
u_set_prgname (const char *prgname)
{
	name = u_strdup (prgname);
}

char *
u_get_prgname (void)
{
	return name;
}
