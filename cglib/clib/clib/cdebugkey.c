/*
 * Copyright (C) 2014 Intel Corporation.
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
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#include <clib-config.h>

#include <clib.h>

unsigned int
c_parse_debug_string(const char *string,
                     const c_debug_key_t *keys,
                     unsigned int nkeys)
{
    char **strv = c_strsplit_set(string, ":;, \t", 0);
    bool needs_invert = false;
    unsigned int value = 0;
    int i;

    if (c_strcasecmp(string, "help") == 0) {
        c_printerr("Supported debug keys:\n");
        for (i = 0; strv[i]; i++) {
            c_printerr("  %s:\n", keys[i].key);
        }
        c_printerr("  all\n");
        c_printerr("  help\n");
    }

    for (i = 0; strv[i]; i++) {
        unsigned int j;

        for (j = 0; j < nkeys; j++)
            if (c_strcasecmp(keys[j].key, strv[i]) == 0)
                value |= keys[j].value;
            else if (c_strcasecmp(keys[j].key, "all") == 0)
                needs_invert = true;
    }

    if (needs_invert)
        value = value ^ (~0);

    c_strfreev(strv);

    return value;
}
