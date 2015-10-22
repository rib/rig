/*
 * Copyright (C) 2015 Intel Corporation.
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

#include <stdbool.h>

#include <clib.h>

const char *
c_getenv(const char *variable)
{
    c_return_val_if_reached(NULL);
}

bool
c_setenv(const char *variable, const char *value, bool overwrite)
{
    c_return_val_if_reached(false);
}

void
c_unsetenv(const char *variable)
{
    c_return_if_reached();
}

char *
c_win32_getlocale(void)
{
    return NULL;
}

bool
c_path_is_absolute(const char *filename)
{
    c_return_val_if_reached(false);
}

const char *
c_get_home_dir(void)
{
    c_return_val_if_reached(NULL);
}

const char *
c_get_user_name(void)
{
    c_return_val_if_reached(NULL);
}

const char *
c_get_tmp_dir(void)
{
    c_return_val_if_reached(NULL);
}
