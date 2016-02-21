/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Intel Corporation.
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

#include <cglib-config.h>

#include <clib.h>
#include <uv.h>

#include "cg-loop.h"
#include "cg-device-private.h"

struct poll_source {
    c_list_t link;

    int fd;
    int64_t (*prepare)(void *user_data);
    void (*dispatch)(void *user_data, int fd, int revents);
    void *user_data;

    uv_timer_t uv_timer;
    uv_poll_t uv_poll;
    uv_prepare_t uv_prepare;
    uv_check_t uv_check;

    /* n handles that need to close before the source
     * can be safely freed... */
    int uv_handle_ref;
};

static enum uv_poll_event
poll_fd_events_to_uv_events(cg_poll_fd_event_t events)
{
    enum uv_poll_event uv_events = 0;

    if (events & CG_POLL_FD_EVENT_IN)
        uv_events |= UV_READABLE;

    if (events & CG_POLL_FD_EVENT_OUT)
        uv_events |= UV_WRITABLE;

    return uv_events;
}

static cg_poll_fd_event_t
uv_events_to_poll_fd_events(enum uv_poll_event events)
{
    cg_poll_fd_event_t poll_fd_events = 0;
    if (events & UV_READABLE)
        poll_fd_events |= CG_POLL_FD_EVENT_IN;
    if (events & UV_WRITABLE)
        poll_fd_events |= CG_POLL_FD_EVENT_OUT;

    return poll_fd_events;
}

/* We use dummy timers as a way to affect the timeout value used
 * while polling for events, but rely on the other callbacks
 * to dispatch work.
 */
static void
dummy_timer_cb(uv_timer_t *timer)
{
    /* NOP */
}

static void
dummy_source_timer_check_cb(uv_check_t *check)
{
    struct poll_source *source = check->data;

    uv_timer_stop(&source->uv_timer);
    uv_check_stop(check);
}

static void
dummy_dev_timer_check_cb(uv_check_t *check)
{
    uv_timer_t *timer = check->data;
    uv_timer_stop(timer);
    uv_check_stop(check);
}

static void
on_event_cb(void *user_data, int fd, int revents)
{
    cg_device_t *dev = user_data;
    cg_renderer_t *renderer = cg_device_get_renderer(dev);

    cg_loop_dispatch_fd(renderer, fd, revents);
}

static struct poll_source *
find_fd_source(cg_device_t *dev, int fd)
{
    struct poll_source *tmp;

    c_list_for_each(tmp, &dev->uv_poll_sources, link) {
        if (tmp->fd == fd)
            return tmp;
    }

    return NULL;
}

static void
handle_close_cb(uv_handle_t *handle)
{
    struct poll_source *source = handle->data;

    if (--source->uv_handle_ref)
        c_slice_free(struct poll_source, source);
}

static void
close_source(struct poll_source *source)
{
    int handle_count = 0;

    uv_timer_stop(&source->uv_timer);
    uv_close((uv_handle_t *)&source->uv_timer, handle_close_cb);
    handle_count++;

    if (source->prepare) {
        uv_prepare_stop(&source->uv_prepare);
        uv_close((uv_handle_t *)&source->uv_prepare, handle_close_cb);
        handle_count++;
    }

    if (source->fd > 0) {
        uv_poll_stop(&source->uv_poll);
        uv_close((uv_handle_t *)&source->uv_poll, handle_close_cb);
        handle_count++;
    }

    uv_check_stop(&source->uv_check);
    uv_close((uv_handle_t *)&source->uv_check, handle_close_cb);
    handle_count++;

    c_warn_if_fail(handle_count == source->uv_handle_ref);
}

static void
remove_fd(cg_device_t *dev, int fd)
{
    struct poll_source *source = find_fd_source(dev, fd);

    if (!source)
        return;

    dev->uv_poll_sources_age++;

    c_list_remove(&source->link);
    close_source(source);
}

static void
source_poll_cb(uv_poll_t *poll, int status, int events)
{
    struct poll_source *source = poll->data;

    cg_poll_fd_event_t poll_fd_events = uv_events_to_poll_fd_events(events);
    source->dispatch(source->user_data, source->fd, poll_fd_events);
}

static void
source_prepare_cb(uv_prepare_t *prepare)
{
    struct poll_source *source = prepare->data;
    int64_t timeout = source->prepare(source->user_data);

    if (timeout == 0)
        source->dispatch(source->user_data, source->fd, 0);

    if (timeout >= 0) {
        timeout /= 1000;
        uv_timer_start(&source->uv_timer, dummy_timer_cb, timeout, 0 /* no repeat */);
        uv_check_start(&source->uv_check, dummy_source_timer_check_cb);
    }
}

static struct poll_source *
add_fd(cg_device_t *dev,
       int fd,
       cg_poll_fd_event_t events,
       int64_t (*prepare)(void *user_data),
       void (*dispatch)(void *user_data, int fd, int revents),
       void *user_data)
{
    struct poll_source *source;

    if (fd > 0)
        remove_fd(dev, fd);

    source = c_slice_new0(struct poll_source);
    source->fd = fd;
    source->prepare = prepare;
    source->dispatch = dispatch;
    source->user_data = user_data;

    uv_timer_init(dev->uv_mainloop, &source->uv_timer);
    source->uv_timer.data = source;
    uv_check_init(dev->uv_mainloop, &source->uv_check);
    source->uv_check.data = source;
    source->uv_handle_ref+=2;

    if (prepare) {
        uv_prepare_init(dev->uv_mainloop, &source->uv_prepare);
        source->uv_prepare.data = source;
        uv_prepare_start(&source->uv_prepare, source_prepare_cb);
        source->uv_handle_ref++;
    }

    if (fd > 0) {
        enum uv_poll_event uv_events = poll_fd_events_to_uv_events(events);

        uv_poll_init(dev->uv_mainloop, &source->uv_poll, fd);
        source->uv_poll.data = source;
        uv_poll_start(&source->uv_poll, uv_events, source_poll_cb);
        source->uv_handle_ref++;
    }

    c_list_insert(dev->uv_poll_sources.prev, &source->link);

    dev->uv_poll_sources_age++;

    return source;
}

static void
update_poll_fds(cg_device_t *dev)
{
    cg_renderer_t *renderer = cg_device_get_renderer(dev);
    cg_poll_fd_t *poll_fds;
    int n_poll_fds;
    int64_t timeout;
    int age;

    age = cg_loop_get_info(renderer, &poll_fds, &n_poll_fds, &timeout);

    if (age != dev->uv_poll_sources_age) {
        int i;

        /* Remove any existing CGlib fds before adding the new ones */
        for (i = 0; i < dev->uv_poll_fds->len; i++) {
            cg_poll_fd_t *poll_fd =
                &c_array_index(dev->uv_poll_fds, cg_poll_fd_t, i);
            remove_fd(dev, poll_fd->fd);
        }

        for (i = 0; i < n_poll_fds; i++) {
            cg_poll_fd_t *poll_fd = &poll_fds[i];
            add_fd(dev,
                   poll_fd->fd,
                   poll_fd->events, /* assume equivalent */
                   NULL, /* prepare */
                   on_event_cb, /* dispatch */
                   dev);
            c_array_append_val(dev->uv_poll_fds, poll_fd);
        }
    }

    dev->uv_poll_sources_age = age;

    if (timeout >= 0) {
        timeout /= 1000;
        uv_timer_start(&dev->uv_mainloop_timer, dummy_timer_cb, timeout, 0);
        dev->uv_mainloop_check.data = &dev->uv_mainloop_timer;
        uv_check_start(&dev->uv_mainloop_check, dummy_dev_timer_check_cb);
    }
}

static void
uv_mainloop_prepare_cb(uv_prepare_t *prepare)
{
    cg_device_t *dev = prepare->data;
    cg_renderer_t *renderer = cg_device_get_renderer(dev);

    cg_loop_dispatch(renderer, NULL, 0);

    update_poll_fds(dev);
}

void
cg_uv_set_mainloop(cg_device_t *dev, uv_loop_t *loop)
{
    c_return_if_fail(dev->uv_mainloop == NULL);
    c_return_if_fail(loop != NULL);

    dev->uv_mainloop = loop;

    c_list_init(&dev->uv_poll_sources);

    uv_timer_init(loop, &dev->uv_mainloop_timer);

    uv_prepare_init(loop, &dev->uv_mainloop_prepare);
    dev->uv_mainloop_prepare.data = dev;
    uv_prepare_start(&dev->uv_mainloop_prepare, uv_mainloop_prepare_cb);

    uv_check_init(loop, &dev->uv_mainloop_check);

    dev->uv_poll_fds = c_array_new(false, true, sizeof(cg_poll_fd_t));

    update_poll_fds(dev);
}

static void
dev_handle_close_cb(uv_handle_t *handle)
{
    /* FIXME: currently cg device closure is synchronous and doesn't
     * wait for the mainloop_xyz handles to close! */
}

void
_cg_uv_cleanup(cg_device_t *dev)
{
    if (dev->uv_poll_fds) {
        struct poll_source *tmp, *source;

        c_list_for_each_safe(tmp, source, &dev->uv_poll_sources, link)
            remove_fd(dev, source->fd);

        c_array_free(dev->uv_poll_fds, true);
    }

    if (dev->uv_mainloop) {
        uv_close((uv_handle_t *)&dev->uv_mainloop_timer, dev_handle_close_cb);
        uv_close((uv_handle_t *)&dev->uv_mainloop_prepare, dev_handle_close_cb);
        uv_close((uv_handle_t *)&dev->uv_mainloop_check, dev_handle_close_cb);
    }
}
