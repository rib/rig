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

#include <clib-config.h>

#include <clib.h>

#ifdef C_PLATFORM_UNIX
#include <unistd.h>
#endif

#ifdef _MSC_VER
#include <direct.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#ifdef WIN32
#include <io.h>
#define open _open

#ifndef S_ISREG
#define S_ISREG(x) ((x &_S_IFMT) == _S_IFREG)
#endif
#ifndef S_ISDIR
#define S_ISDIR(x) ((x &_S_IFMT) == _S_IFDIR)
#endif
#endif

c_quark_t
c_file_error_quark(void)
{
    return c_quark_from_static_string("c-file-error-quark");
}

c_file_error_t
c_file_error_from_errno(int err_no)
{
    switch (err_no) {
    case EEXIST:
        return C_FILE_ERROR_EXIST;
    case EISDIR:
        return C_FILE_ERROR_ISDIR;
    case EACCES:
        return C_FILE_ERROR_ACCES;
    case ENAMETOOLONG:
        return C_FILE_ERROR_NAMETOOLONG;
    case ENOENT:
        return C_FILE_ERROR_NOENT;
    case ENOTDIR:
        return C_FILE_ERROR_NOTDIR;
    case ENXIO:
        return C_FILE_ERROR_NXIO;
    case ENODEV:
        return C_FILE_ERROR_NODEV;
    case EROFS:
        return C_FILE_ERROR_ROFS;
#ifdef ETXTBSY
    case ETXTBSY:
        return C_FILE_ERROR_TXTBSY;
#endif
    case EFAULT:
        return C_FILE_ERROR_FAULT;
#ifdef ELOOP
    case ELOOP:
        return C_FILE_ERROR_LOOP;
#endif
    case ENOSPC:
        return C_FILE_ERROR_NOSPC;
    case ENOMEM:
        return C_FILE_ERROR_NOMEM;
    case EMFILE:
        return C_FILE_ERROR_MFILE;
    case ENFILE:
        return C_FILE_ERROR_NFILE;
    case EBADF:
        return C_FILE_ERROR_BADF;
    case EINVAL:
        return C_FILE_ERROR_INVAL;
    case EPIPE:
        return C_FILE_ERROR_PIPE;
    case EAGAIN:
        return C_FILE_ERROR_AGAIN;
    case EINTR:
        return C_FILE_ERROR_INTR;
    case EIO:
        return C_FILE_ERROR_IO;
    case EPERM:
        return C_FILE_ERROR_PERM;
    case ENOSYS:
        return C_FILE_ERROR_NOSYS;
    default:
        return C_FILE_ERROR_FAILED;
    }
}

#ifdef WIN32
#define TMP_FILE_FORMAT "%.*s%s.tmp"
#else
#define TMP_FILE_FORMAT "%.*s.%s~"
#endif

bool
c_file_set_contents(const char *filename,
                    const char *contents,
                    c_ssize_t length,
                    c_error_t **err)
{
    const char *name;
    char *path;
    FILE *fp;

    if (!(name = strrchr(filename, C_DIR_SEPARATOR)))
        name = filename;
    else
        name++;

    path = c_strdup_printf(TMP_FILE_FORMAT, name - filename, filename, name);
    if (!(fp = fopen(path, "wb"))) {
        c_set_error(err,
                    C_FILE_ERROR,
                    c_file_error_from_errno(errno),
                    "%s",
                    c_strerror(errno));
        c_free(path);
        return false;
    }

    if (length < 0)
        length = strlen(contents);

    if (fwrite(contents, 1, length, fp) < length) {
        c_set_error(err,
                    C_FILE_ERROR,
                    c_file_error_from_errno(ferror(fp)),
                    "%s",
                    c_strerror(ferror(fp)));
        c_unlink(path);
        c_free(path);
        fclose(fp);

        return false;
    }

    fclose(fp);

    if (c_rename(path, filename) != 0) {
        c_set_error(err,
                    C_FILE_ERROR,
                    c_file_error_from_errno(errno),
                    "%s",
                    c_strerror(errno));
        c_unlink(path);
        c_free(path);
        return false;
    }

    c_free(path);

    return true;
}

#ifndef O_LARGEFILE
#define OPEN_FLAGS (O_RDONLY)
#else
#define OPEN_FLAGS (O_RDONLY | O_LARGEFILE)
#endif

bool
c_file_get_contents(const char *filename,
                    char **contents,
                    size_t *length,
                    c_error_t **error)
{
    char *str;
    int fd;
    struct stat st;
    long offset;
    int nread;

    c_return_val_if_fail(filename != NULL, false);
    c_return_val_if_fail(contents != NULL, false);
    c_return_val_if_fail(error == NULL || *error == NULL, false);

    *contents = NULL;
    if (length)
        *length = 0;

    fd = open(filename, OPEN_FLAGS);
    if (fd == -1) {
        if (error != NULL) {
            int err = errno;
            *error = c_error_new(C_FILE_ERROR,
                                 c_file_error_from_errno(err),
                                 "Error opening file '%s': %s",
                                 filename,
                                 c_strerror(err));
        }
        return false;
    }

    if (fstat(fd, &st) != 0) {
        if (error != NULL) {
            int err = errno;
            *error = c_error_new(C_FILE_ERROR,
                                 c_file_error_from_errno(err),
                                 "Error in fstat() for file '%s': %s",
                                 filename,
                                 c_strerror(err));
        }
        close(fd);
        return false;
    }

    str = c_malloc(st.st_size + 1);
    offset = 0;
    do {
        nread = read(fd, str + offset, st.st_size - offset);
        if (nread > 0) {
            offset += nread;
        }
    } while ((nread > 0 && offset < st.st_size) ||
             (nread == -1 && errno == EINTR));

    close(fd);
    str[offset] = '\0';
    if (length) {
        *length = offset;
    }
    *contents = str;
    return true;
}

int
c_file_open_tmp(const char *tmpl, char **name_used, c_error_t **error)
{
    static const char *default_tmpl = ".XXXXXX";
    char *t;
    int fd;
    size_t len;

    c_return_val_if_fail(error == NULL || *error == NULL, -1);

    if (tmpl == NULL)
        tmpl = default_tmpl;

    if (strchr(tmpl, C_DIR_SEPARATOR) != NULL) {
        if (error) {
            *error =
                c_error_new(C_FILE_ERROR,
                            C_FILE_ERROR_FAILED,
                            "Template should not have any " C_DIR_SEPARATOR_S);
        }
        return -1;
    }

    len = strlen(tmpl);
    if (len < 6 || strcmp(tmpl + len - 6, "XXXXXX")) {
        if (error) {
            *error = c_error_new(C_FILE_ERROR,
                                 C_FILE_ERROR_FAILED,
                                 "Template should end with XXXXXX");
        }
        return -1;
    }

    t = c_build_filename(c_get_tmp_dir(), tmpl, NULL);

    fd = mkstemp(t);

    if (fd == -1) {
        if (error) {
            int err = errno;
            *error = c_error_new(C_FILE_ERROR,
                                 c_file_error_from_errno(err),
                                 "Error in mkstemp(): %s",
                                 c_strerror(err));
        }
        c_free(t);
        return -1;
    }

    if (name_used) {
        *name_used = t;
    } else {
        c_free(t);
    }
    return fd;
}

char *
c_get_current_dir(void)
{
#ifdef __native_client__
    char *buffer;
    if ((buffer = c_getenv("NACL_PWD"))) {
        buffer = c_strdup(buffer);
    } else {
        buffer = c_strdup(".");
    }
    return buffer;
#else
    int s = 32;
    char *buffer = NULL, *r;
    bool fail;

    do {
        buffer = c_realloc(buffer, s);
        r = getcwd(buffer, s);
        fail = (r == NULL && errno == ERANGE);
        if (fail) {
            s <<= 1;
        }
    } while (fail);

    /* On amd64 sometimes the bottom 32-bits of r == the bottom 32-bits of
     * buffer
     * but the top 32-bits of r have overflown to 0xffffffff (seriously wtf
     * getcwd
     * so we return the buffer here since it has a pointer to the valid string
     */
    return buffer;
#endif
}

#if defined(C_PLATFORM_UNIX) && !defined(C_PLATFORM_WEB)
bool
c_file_test(const char *filename, c_file_test_t test)
{
    struct stat st;
    bool have_stat;

    if (filename == NULL || test == 0)
        return false;

    have_stat = false;

    if ((test & C_FILE_TEST_EXISTS) != 0) {
        if (access(filename, F_OK) == 0)
            return true;
    }

    if ((test & C_FILE_TEST_IS_EXECUTABLE) != 0) {
        if (access(filename, X_OK) == 0)
            return true;
    }
    if ((test & C_FILE_TEST_IS_SYMLINK) != 0) {
        have_stat = (lstat(filename, &st) == 0);
        if (have_stat && S_ISLNK(st.st_mode))
            return true;
    }

    if ((test & C_FILE_TEST_IS_REGULAR) != 0) {
        if (!have_stat)
            have_stat = (stat(filename, &st) == 0);
        if (have_stat && S_ISREG(st.st_mode))
            return true;
    }
    if ((test & C_FILE_TEST_IS_DIR) != 0) {
        if (!have_stat)
            have_stat = (stat(filename, &st) == 0);
        if (have_stat && S_ISDIR(st.st_mode))
            return true;
    }
    return false;
}

#elif defined(WIN32)

#ifdef _MSC_VER
#pragma warning(disable : 4701)
#endif

bool
c_file_test(const char *filename, c_file_test_t test)
{
    c_utf16_t *utf16_filename = NULL;
    DWORD attr;

    if (filename == NULL || test == 0)
        return false;

    utf16_filename = u8to16(filename);
    attr = GetFileAttributesW(utf16_filename);
    c_free(utf16_filename);

    if (attr == INVALID_FILE_ATTRIBUTES)
        return false;

    if ((test & C_FILE_TEST_EXISTS) != 0) {
        return true;
    }

    if ((test & C_FILE_TEST_IS_EXECUTABLE) != 0) {
        size_t len = strlen(filename);
        if (len > 4 && strcmp(filename + len - 3, "exe"))
            return true;

        return false;
    }

    if ((test & C_FILE_TEST_IS_REGULAR) != 0) {
        if (attr & (FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_DIRECTORY))
            return false;
        return true;
    }

    if ((test & C_FILE_TEST_IS_DIR) != 0) {
        if (attr & FILE_ATTRIBUTE_DIRECTORY)
            return true;
    }

    /* make this last in case it is OR'd with something else */
    if ((test & C_FILE_TEST_IS_SYMLINK) != 0) {
        return false;
    }

    return false;
}

#else

bool
c_file_test(const char *filename, c_file_test_t test)
{
    return false;
}
#endif
