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
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>

UQuark
u_file_error_quark (void)
{
	return u_quark_from_static_string ("g-file-error-quark");
}

UFileError
u_file_error_from_errno (int err_no)
{
	switch (err_no) {
	case EEXIST:
		return U_FILE_ERROR_EXIST;
	case EISDIR:
		return U_FILE_ERROR_ISDIR;
	case EACCES:
		return U_FILE_ERROR_ACCES;
	case ENAMETOOLONG:
		return U_FILE_ERROR_NAMETOOLONG;
	case ENOENT:
		return U_FILE_ERROR_NOENT;
	case ENOTDIR:
		return U_FILE_ERROR_NOTDIR;
	case ENXIO:
		return U_FILE_ERROR_NXIO;
	case ENODEV:
		return U_FILE_ERROR_NODEV;
	case EROFS:
		return U_FILE_ERROR_ROFS;
#ifdef ETXTBSY
	case ETXTBSY:
		return U_FILE_ERROR_TXTBSY;
#endif
	case EFAULT:
		return U_FILE_ERROR_FAULT;
#ifdef ELOOP
	case ELOOP:
		return U_FILE_ERROR_LOOP;
#endif
	case ENOSPC:
		return U_FILE_ERROR_NOSPC;
	case ENOMEM:
		return U_FILE_ERROR_NOMEM;
	case EMFILE:
		return U_FILE_ERROR_MFILE;
	case ENFILE:
		return U_FILE_ERROR_NFILE;
	case EBADF:
		return U_FILE_ERROR_BADF;
	case EINVAL:
		return U_FILE_ERROR_INVAL;
	case EPIPE:
		return U_FILE_ERROR_PIPE;
	case EAGAIN:
		return U_FILE_ERROR_AGAIN;
	case EINTR:
		return U_FILE_ERROR_INTR;
	case EIO:
		return U_FILE_ERROR_IO;
	case EPERM:
		return U_FILE_ERROR_PERM;
	case ENOSYS:
		return U_FILE_ERROR_NOSYS;
	default:
		return U_FILE_ERROR_FAILED;
	}
}

#ifdef U_OS_WIN32
#define TMP_FILE_FORMAT "%.*s%s.tmp"
#else
#define TMP_FILE_FORMAT "%.*s.%s~"
#endif

uboolean
u_file_set_contents (const char *filename, const char *contents, ussize length, UError **err)
{
	const char *name;
	char *path;
	FILE *fp;
	
	if (!(name = strrchr (filename, U_DIR_SEPARATOR)))
		name = filename;
	else
		name++;
	
	path = u_strdup_printf (TMP_FILE_FORMAT, name - filename, filename, name);
	if (!(fp = fopen (path, "wb"))) {
		u_set_error (err, U_FILE_ERROR, u_file_error_from_errno (errno), "%s", u_strerror (errno));
		u_free (path);
		return FALSE;
	}
	
	if (length < 0)
		length = strlen (contents);
	
	if (fwrite (contents, 1, length, fp) < length) {
		u_set_error (err, U_FILE_ERROR, u_file_error_from_errno (ferror (fp)), "%s", u_strerror (ferror (fp)));
		u_unlink (path);
		u_free (path);
		fclose (fp);
		
		return FALSE;
	}
	
	fclose (fp);
	
	if (u_rename (path, filename) != 0) {
		u_set_error (err, U_FILE_ERROR, u_file_error_from_errno (errno), "%s", u_strerror (errno));
		u_unlink (path);
		u_free (path);
		return FALSE;
	}
	
	u_free (path);
	
	return TRUE;
}
