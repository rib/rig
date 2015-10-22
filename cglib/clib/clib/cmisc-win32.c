/*
 * Misc functions with no place to go (right now)
 *
 * Author:
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

#include <stdlib.h>
#include <clib.h>

#include <windows.h>
#include <direct.h>
#include <io.h>

const char *
c_getenv(const char *variable)
{
    c_utf16_t *var, *buffer;
    char *val = NULL;
    int32_t buffer_size = 1024;
    int32_t retval;
    var = u8to16(variable);
    buffer = c_malloc(buffer_size * sizeof(c_utf16_t));
    retval = GetEnvironmentVariableW(var, buffer, buffer_size);
    if (retval != 0) {
        if (retval > buffer_size) {
            c_free(buffer);
            buffer_size = retval;
            buffer = c_malloc(buffer_size * sizeof(c_utf16_t));
            retval = GetEnvironmentVariableW(var, buffer, buffer_size);
        }
        val = u16to8(buffer);
    } else {
        if (GetLastError() != ERROR_ENVVAR_NOT_FOUND) {
            val = c_malloc(1);
            *val = 0;
        }
    }
    c_free(var);
    c_free(buffer);
    return val;
}

bool
c_setenv(const char *variable, const char *value, bool overwrite)
{
    c_utf16_t *var, *val;
    bool result;
    var = u8to16(variable);
    val = u8to16(value);
    result = (SetEnvironmentVariableW(var, val) != 0) ? true : false;
    c_free(var);
    c_free(val);
    return result;
}

void
c_unsetenv(const char *variable)
{
    c_utf16_t *var;
    var = u8to16(variable);
    SetEnvironmentVariableW(var, L"");
    c_free(var);
}

char *
c_win32_getlocale(void)
{
    LCID lcid = GetThreadLocale();
    char buf[19];
    int ccBuf = GetLocaleInfo(lcid, LOCALE_SISO639LANGNAME, buf, 9);
    buf[ccBuf - 1] = '-';
    ccBuf += GetLocaleInfo(lcid, LOCALE_SISO3166CTRYNAME, buf + ccBuf, 9);
    return strdup(buf);
}

bool
c_path_is_absolute(const char *filename)
{
    c_return_val_if_fail(filename != NULL, false);

    if (filename[0] != '\0' && filename[1] != '\0') {
        if (filename[1] == ':' && filename[2] != '\0' &&
            (filename[2] == '\\' || filename[2] == '/'))
            return true;
        /* UNC paths */
        else if (filename[0] == '\\' && filename[1] == '\\' &&
                 filename[2] != '\0')
            return true;
    }

    return false;
}

const char *
c_get_home_dir(void)
{
    /* FIXME */
    const char *drive = c_getenv("HOMEDRIVE");
    const char *path = c_getenv("HOMEPATH");
    char *home_dir = NULL;

    if (drive && path) {
        home_dir = c_malloc(strlen(drive) + strlen(path) + 1);
        if (home_dir) {
            sprintf(home_dir, "%s%s", drive, path);
        }
    }

    return home_dir;
}

const char *
c_get_user_name(void)
{
    const char *retName = c_getenv("USER");
    if (!retName)
        retName = c_getenv("USERNAME");
    return retName;
}

static const char *tmp_dir;

const char *
c_get_tmp_dir(void)
{
    if (tmp_dir == NULL) {
        if (tmp_dir == NULL) {
            tmp_dir = c_getenv("TMPDIR");
            if (tmp_dir == NULL) {
                tmp_dir = c_getenv("TMP");
                if (tmp_dir == NULL) {
                    tmp_dir = c_getenv("TEMP");
                    if (tmp_dir == NULL)
                        tmp_dir = "C:\\temp";
                }
            }
        }
    }
    return tmp_dir;
}
