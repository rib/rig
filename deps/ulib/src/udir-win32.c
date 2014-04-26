/*
 * Directory utility functions.
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
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>

#include <winsock2.h>

struct _UDir {
	HANDLE handle;
	char* current;
	char* next;
};

UDir *
u_dir_open (const char *path, unsigned int flags, UError **error)
{
	UDir *dir;
	uunichar2* path_utf16;
	uunichar2* path_utf16_search;
	WIN32_FIND_DATAW find_data;

	u_return_val_if_fail (path != NULL, NULL);
	u_return_val_if_fail (error == NULL || *error == NULL, NULL);

	dir = u_new0 (UDir, 1);
	path_utf16 = u8to16 (path);
	path_utf16_search = u_malloc ((wcslen((wchar_t *) path_utf16) + 3)*sizeof(uunichar2));
	wcscpy (path_utf16_search, path_utf16);
	wcscat (path_utf16_search, L"\\*");

	dir->handle = FindFirstFileW (path_utf16_search, &find_data);
	if (dir->handle == INVALID_HANDLE_VALUE) {
		if (error) {
			int err = errno;
			*error = u_error_new (U_FILE_ERROR,
                                              u_file_error_from_errno (err),
                                              u_strerror (err));
		}
		u_free (path_utf16_search);
		u_free (path_utf16);
		u_free (dir);
		return NULL;
	}
	u_free (path_utf16_search);
	u_free (path_utf16);

	while ((wcscmp ((wchar_t *) find_data.cFileName, L".") == 0) || (wcscmp ((wchar_t *) find_data.cFileName, L"..") == 0)) {
		if (!FindNextFileW (dir->handle, &find_data)) {
			if (error) {
				int err = errno;
				*error = u_error_new (U_FILE_ERROR,
                                                      u_file_error_from_errno (err),
                                                      u_strerror (err));
			}
			u_free (dir);
			return NULL;
		}
	}

	dir->current = NULL;
	dir->next = u16to8 (find_data.cFileName);
	return dir;
}

const char *
u_dir_read_name (UDir *dir)
{
	WIN32_FIND_DATAW find_data;

	u_return_val_if_fail (dir != NULL && dir->handle != 0, NULL);

	if (dir->current)
		u_free (dir->current);
	dir->current = NULL;

	dir->current = dir->next;

	if (!dir->current)
		return NULL;

	dir->next = NULL;

	do {
		if (!FindNextFileW (dir->handle, &find_data)) {
			dir->next = NULL;
			return dir->current;
		}
	} while ((wcscmp ((wchar_t *) find_data.cFileName, L".") == 0) || (wcscmp ((wchar_t *) find_data.cFileName, L"..") == 0));

	dir->next = u16to8 (find_data.cFileName);
	return dir->current;
}

void
u_dir_rewind (UDir *dir)
{
}

void
u_dir_close (UDir *dir)
{
	u_return_if_fail (dir != NULL && dir->handle != 0);
	
	if (dir->current)
		u_free (dir->current);
	dir->current = NULL;
	if (dir->next)
		u_free (dir->next);
	dir->next = NULL;
	FindClose (dir->handle);
	dir->handle = 0;
	u_free (dir);
}


