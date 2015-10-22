/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012-2015 Intel Corporation.
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
 */

#include <cglib-config.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "cg-loop.h"
#include "cg-loop-private.h"
#include "cg-winsys-private.h"
#include "cg-renderer-private.h"

struct _cg_poll_source_t {
    int fd;
    cg_poll_prepare_callback_t prepare;
    cg_poll_dispatch_callback_t dispatch;
    void *user_data;
};

int
cg_loop_get_info(cg_renderer_t *renderer,
                 cg_poll_fd_t **poll_fds,
                 int *n_poll_fds,
                 int64_t *timeout)
{
    c_llist_t *l, *next;

    c_return_val_if_fail(cg_is_renderer(renderer), 0);
    c_return_val_if_fail(timeout != NULL, 0);

    *timeout = -1;

    if (!c_list_empty(&renderer->idle_closures))
        *timeout = 0;

    /* This loop needs to cope with the prepare callback removing its
     * own fd */
    for (l = renderer->poll_sources; l; l = next) {
        cg_poll_source_t *source = l->data;

        next = l->next;

        if (source->prepare) {
            int64_t source_timeout = source->prepare(source->user_data);
            if (source_timeout >= 0 &&
                (*timeout == -1 || *timeout > source_timeout))
                *timeout = source_timeout;
        }
    }

    /* We should only be passing NULL for n_poll_fds or poll_fds on
     * platforms we assume don't support fd polling... */
    c_return_val_if_fail(n_poll_fds != NULL || renderer->poll_fds->len == 0, 0);

    /* This is deliberately set after calling the prepare callbacks in
     * case one of them removes its fd */
    if (poll_fds)
        *poll_fds = (void *)renderer->poll_fds->data;
    if (n_poll_fds)
        *n_poll_fds = renderer->poll_fds->len;

    return renderer->poll_fds_age;
}

void
cg_loop_dispatch(cg_renderer_t *renderer,
                 const cg_poll_fd_t *poll_fds,
                 int n_poll_fds)
{
    c_llist_t *l, *next;

    c_return_if_fail(cg_is_renderer(renderer));

    _cg_closure_list_invoke_no_args(&renderer->idle_closures);

    /* This loop needs to cope with the dispatch callback removing its
     * own fd */
    for (l = renderer->poll_sources; l; l = next) {
        cg_poll_source_t *source = l->data;
        int i;

        next = l->next;

        if (source->fd == -1) {
            source->dispatch(source->user_data, 0);
            continue;
        }

        for (i = 0; i < n_poll_fds; i++) {
            const cg_poll_fd_t *pollfd = &poll_fds[i];

            if (pollfd->fd == source->fd) {
                source->dispatch(source->user_data, pollfd->revents);
                break;
            }
        }

        if (i == n_poll_fds)
            source->dispatch(source->user_data, 0);
    }
}

void
cg_loop_dispatch_fd(cg_renderer_t *renderer, int fd, int events)
{
    c_llist_t *l;

    for (l = renderer->poll_sources; l; l = l->next) {
        cg_poll_source_t *source = l->data;

        if (source->fd == fd) {
            source->dispatch(source->user_data, events);
            return;
        }
    }
}

static int
find_pollfd(cg_renderer_t *renderer, int fd)
{
    int i;

    for (i = 0; i < renderer->poll_fds->len; i++) {
        cg_poll_fd_t *pollfd =
            &c_array_index(renderer->poll_fds, cg_poll_fd_t, i);

        if (pollfd->fd == fd)
            return i;
    }

    return -1;
}

void
_cg_loop_remove_fd(cg_renderer_t *renderer, int fd)
{
    int i = find_pollfd(renderer, fd);
    c_llist_t *l;

    if (i < 0)
        return;

    c_array_remove_index_fast(renderer->poll_fds, i);
    renderer->poll_fds_age++;

    for (l = renderer->poll_sources; l; l = l->next) {
        cg_poll_source_t *source = l->data;
        if (source->fd == fd) {
            renderer->poll_sources =
                c_llist_delete_link(renderer->poll_sources, l);
            c_slice_free(cg_poll_source_t, source);
            break;
        }
    }
}

void
_cg_loop_modify_fd(cg_renderer_t *renderer,
                   int fd,
                   cg_poll_fd_event_t events)
{
    int fd_index = find_pollfd(renderer, fd);

    if (fd_index == -1)
        c_warn_if_reached();
    else {
        cg_poll_fd_t *pollfd =
            &c_array_index(renderer->poll_sources, cg_poll_fd_t, fd_index);

        pollfd->events = events;
        renderer->poll_fds_age++;
    }
}

void
_cg_loop_add_fd(cg_renderer_t *renderer,
                int fd,
                cg_poll_fd_event_t events,
                cg_poll_prepare_callback_t prepare,
                cg_poll_dispatch_callback_t dispatch,
                void *user_data)
{
    cg_poll_fd_t pollfd = { fd, events };
    cg_poll_source_t *source;

    _cg_loop_remove_fd(renderer, fd);

    source = c_slice_new0(cg_poll_source_t);
    source->fd = fd;
    source->prepare = prepare;
    source->dispatch = dispatch;
    source->user_data = user_data;

    renderer->poll_sources = c_llist_prepend(renderer->poll_sources, source);

    c_array_append_val(renderer->poll_fds, pollfd);
    renderer->poll_fds_age++;
}

#ifdef __EMSCRIPTEN__
static void browser_queue_callback(cg_renderer_t *renderer);

static void
_cg_browser_callback(void *user_data)
{
    cg_renderer_t *renderer = user_data;

    cg_loop_dispatch(renderer, NULL /* fds */, 0 /* n fds */);

    browser_queue_callback(renderer);
}

static void
browser_queue_callback(cg_renderer_t *renderer)
{
    int64_t timeout_us;

    if (renderer->browser_idle_queued)
        return;

    cg_loop_get_info(renderer, NULL, NULL, &timeout_us);

    if (timeout_us < 0)
        return;

    if (timeout_us == 0) {
        emscripten_async_call(_cg_browser_callback, renderer, 0 /* timeout */);
        renderer->browser_idle_queued = true;
    } else {
        int timeout_ms = timeout_us / 1000;
        emscripten_async_call(_cg_browser_callback, renderer, timeout_ms);
    }
}
#endif

cg_poll_source_t *
_cg_loop_add_source(cg_renderer_t *renderer,
                    cg_poll_prepare_callback_t prepare,
                    cg_poll_dispatch_callback_t dispatch,
                    void *user_data)
{
    cg_poll_source_t *source;

    source = c_slice_new0(cg_poll_source_t);
    source->fd = -1;
    source->prepare = prepare;
    source->dispatch = dispatch;
    source->user_data = user_data;

    renderer->poll_sources = c_llist_prepend(renderer->poll_sources, source);

#ifdef __EMSCRIPTEN__
    browser_queue_callback(renderer);
#endif

    return source;
}

void
_cg_loop_remove_source(cg_renderer_t *renderer,
                       cg_poll_source_t *source)
{
    c_llist_t *l;

    for (l = renderer->poll_sources; l; l = l->next) {
        if (l->data == source) {
            renderer->poll_sources =
                c_llist_delete_link(renderer->poll_sources, l);
            c_slice_free(cg_poll_source_t, source);
            break;
        }
    }
}

cg_closure_t *
_cg_loop_add_idle(cg_renderer_t *renderer,
                  cg_idle_callback_t idle_cb,
                  void *user_data,
                  cg_user_data_destroy_callback_t destroy_cb)
{
    cg_closure_t *closure = _cg_closure_list_add(
        &renderer->idle_closures, idle_cb, user_data, destroy_cb);

#ifdef __EMSCRIPTEN__
    browser_queue_callback(renderer);
#endif

    return closure;
}
