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

#include <clib.h>

struct _c_backtrace
{
    int n_frames;
};

void **
c_backtrace(int *n_frames)
{
    *n_frames = 0;
    return NULL;
}

void
c_backtrace_symbols(void **addresses,
                    char **frames,
                    int n_frames)
{
    c_warn_if_reached();
}

c_backtrace_t *
c_backtrace_new(void)
{
    c_backtrace_t *bt = c_malloc(sizeof(*bt));

    bt->n_frames = 0;

    return bt;
}

void
c_backtrace_free(c_backtrace_t *backtrace)
{
    c_free(backtrace);
}

int
c_backtrace_get_n_frames(c_backtrace_t *bt)
{
    return bt->n_frames;
}

void
c_backtrace_get_frame_symbols(c_backtrace_t *bt,
                              char **frames,
                              int n_frames)
{
    c_warn_if_reached();
}

void
c_backtrace_log(c_backtrace_t *bt,
                c_log_context_t *lctx,
                const char *log_domain,
                c_log_level_flags_t log_level)
{
}

void
c_backtrace_log_error(c_backtrace_t *bt)
{
}

c_backtrace_t *
c_backtrace_copy(c_backtrace_t *bt)
{
    return c_memdup(bt, sizeof (*bt));
}
