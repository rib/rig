/*
 * C Utilities
 *
 * Copyright (C) 2015 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "clib-config.h"

#include <clib.h>

const char *
c_get_xdg_data_home(void)
{
    static const char *resolved = NULL;

    if (!resolved) {
        resolved = getenv("XDG_DATA_HOME");
        if (!resolved) {
            const char *home = getenv("HOME");
            if (home)
                resolved = c_strconcat(home, "/.local/share/", NULL);
        }
    }

    return resolved;
}

const char *
c_get_xdg_data_dirs(void)
{
    static const char *resolved = NULL;

    if (!resolved) {
        resolved = getenv("XDG_DATA_DIRS");
        if (!resolved)
            resolved = "/usr/local/share/:/usr/share/";
    }

    return resolved;
}

void
c_foreach_xdg_data_dir(c_xdg_dir_callback_t callback, void *user_data)
{
    static char **split_data_dirs = NULL;
    int i;

    if (!split_data_dirs) {
        const char *data_dirs = c_get_xdg_data_dirs();

        if (!data_dirs)
            return;

        split_data_dirs = c_strsplit(data_dirs, ":", -1);
    }

    for (i = 0; split_data_dirs[i]; i++)
        callback(split_data_dirs[i], user_data);
}
