/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
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
 *
 *
 */

#include <cglib-config.h>

#include <glib.h>
#include <clib.h>

#include "cg-glib-source.h"
#include "cg-loop.h"

typedef struct _cg_glib_source_t {
    GSource source;

    cg_renderer_t *renderer;

    c_array_t *poll_fds;
    int poll_fds_age;

    int64_t expiration_time;
} cg_glib_source_t;

static gboolean
cg_glib_source_prepare(GSource *source, int *timeout)
{
    cg_glib_source_t *cg_source = (cg_glib_source_t *)source;
    cg_poll_fd_t *poll_fds;
    int n_poll_fds;
    int64_t cg_timeout;
    int age;
    int i;

    age = cg_loop_get_info(
        cg_source->renderer, &poll_fds, &n_poll_fds, &cg_timeout);

    /* We have to be careful not to call g_source_add/remove_poll unless
     * the FDs have changed because it will cause the main loop to
     * immediately wake up. If we call it every time the source is
     * prepared it will effectively never go idle. */
    if (age != cg_source->poll_fds_age) {
        /* Remove any existing polls before adding the new ones */
        for (i = 0; i < cg_source->poll_fds->len; i++) {
            GPollFD *poll_fd = &c_array_index(cg_source->poll_fds, GPollFD, i);
            g_source_remove_poll(source, poll_fd);
        }

        c_array_set_size(cg_source->poll_fds, n_poll_fds);

        for (i = 0; i < n_poll_fds; i++) {
            GPollFD *poll_fd = &c_array_index(cg_source->poll_fds, GPollFD, i);
            poll_fd->fd = poll_fds[i].fd;
            g_source_add_poll(source, poll_fd);
        }
    }

    cg_source->poll_fds_age = age;

    /* Update the events */
    for (i = 0; i < n_poll_fds; i++) {
        GPollFD *poll_fd = &c_array_index(cg_source->poll_fds, GPollFD, i);
        poll_fd->events = poll_fds[i].events;
        poll_fd->revents = 0;
    }

    if (cg_timeout == -1) {
        *timeout = -1;
        cg_source->expiration_time = -1;
    } else {
        /* Round up to ensure that we don't try again too early */
        *timeout = (cg_timeout + 999) / 1000;
        cg_source->expiration_time = (g_source_get_time(source) + cg_timeout);
    }

    return *timeout == 0;
}

static gboolean
cg_glib_source_check(GSource *source)
{
    cg_glib_source_t *cg_source = (cg_glib_source_t *)source;
    int i;

    if (cg_source->expiration_time >= 0 &&
        g_source_get_time(source) >= cg_source->expiration_time)
        return true;

    for (i = 0; i < cg_source->poll_fds->len; i++) {
        GPollFD *poll_fd = &c_array_index(cg_source->poll_fds, GPollFD, i);
        if (poll_fd->revents != 0)
            return true;
    }

    return false;
}

static gboolean
cg_glib_source_dispatch(GSource *source, GSourceFunc callback, void *user_data)
{
    cg_glib_source_t *cg_source = (cg_glib_source_t *)source;
    cg_poll_fd_t *poll_fds =
        (cg_poll_fd_t *)&c_array_index(cg_source->poll_fds, GPollFD, 0);

    cg_loop_dispatch(
        cg_source->renderer, poll_fds, cg_source->poll_fds->len);

    return true;
}

static void
cg_glib_source_finalize(GSource *source)
{
    cg_glib_source_t *cg_source = (cg_glib_source_t *)source;

    c_array_free(cg_source->poll_fds, true);
}

static GSourceFuncs cg_glib_source_funcs = { cg_glib_source_prepare,
                                             cg_glib_source_check,
                                             cg_glib_source_dispatch,
                                             cg_glib_source_finalize };

GSource *
cg_glib_renderer_source_new(cg_renderer_t *renderer, int priority)
{
    GSource *source;
    cg_glib_source_t *cg_source;

    source = g_source_new(&cg_glib_source_funcs, sizeof(cg_glib_source_t));
    cg_source = (cg_glib_source_t *)source;

    cg_source->renderer = renderer;
    cg_source->poll_fds = c_array_new(false, false, sizeof(GPollFD));

    if (priority != G_PRIORITY_DEFAULT)
        g_source_set_priority(source, priority);

    return source;
}

GSource *
cg_glib_source_new(cg_device_t *dev, int priority)
{
    return cg_glib_renderer_source_new(cg_device_get_renderer(dev),
                                       priority);
}
