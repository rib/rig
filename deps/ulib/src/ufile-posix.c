/*
 * File utility functions.
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
#include <config.h>
#include <ulib.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>
#include <errno.h>

#ifdef _MSC_VER
#include <direct.h>
#endif
#ifdef U_OS_WIN32
int mkstemp (char *tmp_template);
#endif

#ifndef O_LARGEFILE
#define OPEN_FLAGS (O_RDONLY)
#else
#define OPEN_FLAGS (O_RDONLY | O_LARGEFILE)
#endif
uboolean
u_file_get_contents (const char *filename, char **contents, size_t *length, UError **error)
{
	char *str;
	int fd;
	struct stat st;
	long offset;
	int nread;

	u_return_val_if_fail (filename != NULL, FALSE);
	u_return_val_if_fail (contents != NULL, FALSE);
	u_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	*contents = NULL;
	if (length)
		*length = 0;

	fd = open (filename, OPEN_FLAGS);
	if (fd == -1) {
		if (error != NULL) {
			int err = errno;
			*error = u_error_new (U_FILE_ERROR,
                                              u_file_error_from_errno (err),
                                              "Error opening file '%s': %s",
                                              filename,
                                              u_strerror (err));
		}
		return FALSE;
	}

	if (fstat (fd, &st) != 0) {
		if (error != NULL) {
			int err = errno;
			*error = u_error_new (U_FILE_ERROR,
                                              u_file_error_from_errno (err),
                                              "Error in fstat() for file '%s': %s",
                                              filename,
                                              u_strerror (err));
		}
		close (fd);
		return FALSE;
	}

	str = u_malloc (st.st_size + 1);
	offset = 0;
	do {
		nread = read (fd, str + offset, st.st_size - offset);
		if (nread > 0) {
			offset += nread;
		}
	} while ((nread > 0 && offset < st.st_size) || (nread == -1 && errno == EINTR));

	close (fd);
	str [st.st_size] = '\0';
	if (length) {
		*length = st.st_size;
	}
	*contents = str;
	return TRUE;
}

int
u_file_open_tmp (const char *tmpl, char **name_used, UError **error)
{
	const static char *default_tmpl = ".XXXXXX";
	char *t;
	int fd;
	size_t len;

	u_return_val_if_fail (error == NULL || *error == NULL, -1);

	if (tmpl == NULL)
		tmpl = default_tmpl;

	if (strchr (tmpl, U_DIR_SEPARATOR) != NULL) {
		if (error) {
			*error = u_error_new (U_FILE_ERROR,
                                              U_FILE_ERROR_FAILED,
                                              "Template should not have any " U_DIR_SEPARATOR_S);
		}
		return -1;
	}

	len = strlen (tmpl);
	if (len < 6 || strcmp (tmpl + len - 6, "XXXXXX")) {
		if (error) {
			*error = u_error_new (U_FILE_ERROR,
                                              U_FILE_ERROR_FAILED,
                                              "Template should end with XXXXXX");
		}
		return -1;
	}

	t = u_build_filename (u_get_tmp_dir (), tmpl, NULL);

	fd = mkstemp (t);

	if (fd == -1) {
		if (error) {
			int err = errno;
			*error = u_error_new (U_FILE_ERROR,
                                              u_file_error_from_errno (err),
                                              "Error in mkstemp(): %s",
                                              u_strerror (err));
		}
		u_free (t);
		return -1;
	}

	if (name_used) {
		*name_used = t;
	} else {
		u_free (t);
	}
	return fd;
}

char *
u_get_current_dir (void)
{
#ifdef __native_client__
	char *buffer;
	if ((buffer = u_getenv("NACL_PWD"))) {
		buffer = u_strdup(buffer);
	} else {
		buffer = u_strdup(".");
	}
	return buffer;
#else
	int s = 32;
	char *buffer = NULL, *r;
	uboolean fail;
	
	do {
		buffer = u_realloc (buffer, s);
		r = getcwd (buffer, s);
		fail = (r == NULL && errno == ERANGE);
		if (fail) {
			s <<= 1;
		}
	} while (fail);

	/* On amd64 sometimes the bottom 32-bits of r == the bottom 32-bits of buffer
	 * but the top 32-bits of r have overflown to 0xffffffff (seriously wtf getcwd
	 * so we return the buffer here since it has a pointer to the valid string
	 */
	return buffer;
#endif
}
