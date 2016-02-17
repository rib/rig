/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2014 Intel Corporation.
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

#include <rut-config.h>

#ifdef USE_UV
#include <uv.h>
#endif

#ifdef __ANDROID__
#include <android_native_app_glue.h>
#include "rut-android-shell.h"
#endif

#ifdef USE_GLIB
#include <glib.h>
#endif

#include <cglib/cglib.h>
#ifdef USE_SDL
#include <cglib/cg-sdl.h>
#include "rut-sdl-shell.h"
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "rut-poll.h"
#include "rut-shell.h"
#include "rut-shell.h"
#include "rut-os.h"

struct _rut_poll_source_t {
    c_list_t link;

    rut_shell_t *shell;
    int fd;
    int64_t (*prepare)(void *user_data);
    void (*dispatch)(void *user_data, int fd, int revents);
    void *user_data;

#ifdef USE_UV
    uv_timer_t uv_timer;
    uv_poll_t uv_poll;
    uv_prepare_t uv_prepare;
    uv_check_t uv_check;
    int n_uv_handles;
#endif
};

#ifdef USE_UV
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
    rut_poll_source_t *source = check->data;

    uv_timer_stop(&source->uv_timer);
    uv_check_stop(check);
}

static void
dummy_shell_timer_check_cb(uv_check_t *check)
{
    uv_timer_t *timer = check->data;
    uv_timer_stop(timer);
    uv_check_stop(check);
}

#ifndef RIG_SIMULATOR_ONLY
static void
on_cg_event_cb(void *user_data, int fd, int revents)
{
    rut_shell_t *shell = user_data;
    cg_renderer_t *renderer =
        cg_device_get_renderer(shell->cg_device);

    cg_loop_dispatch_fd(renderer, fd, revents);
}

static void
update_cg_sources(rut_shell_t *shell)
{
    cg_renderer_t *renderer =
        cg_device_get_renderer(shell->cg_device);
    cg_poll_fd_t *poll_fds;
    int n_poll_fds;
    int64_t cg_timeout;
    int age;

    age = cg_loop_get_info(renderer, &poll_fds, &n_poll_fds, &cg_timeout);

    if (age != shell->cg_poll_fds_age) {
        int i;

        /* Remove any existing Cogl fds before adding the new ones */
        for (i = 0; i < shell->cg_poll_fds->len; i++) {
            cg_poll_fd_t *poll_fd =
                &c_array_index(shell->cg_poll_fds, cg_poll_fd_t, i);
            rut_poll_shell_remove_fd(shell, poll_fd->fd);
        }

        for (i = 0; i < n_poll_fds; i++) {
            cg_poll_fd_t *poll_fd = &poll_fds[i];
            rut_poll_shell_add_fd(shell,
                                  poll_fd->fd,
                                  poll_fd->events, /* assume equivalent */
                                  NULL, /* prepare */
                                  on_cg_event_cb, /* dispatch */
                                  shell);
            c_array_append_val(shell->cg_poll_fds, poll_fd);
        }
    }

    shell->cg_poll_fds_age = age;

    if (cg_timeout >= 0) {
        cg_timeout /= 1000;
        uv_timer_start(&shell->cg_timer, dummy_timer_cb, cg_timeout, 0);
        uv_check_start(&shell->cg_timer_check, dummy_shell_timer_check_cb);
    }
}
#endif /* RIG_SIMULATOR_ONLY */

static rut_poll_source_t *
find_fd_source(rut_shell_t *shell, int fd)
{
    rut_poll_source_t *tmp;

    c_list_for_each(tmp, &shell->poll_sources, link)
    {
        if (tmp->fd == fd)
            return tmp;
    }

    return NULL;
}

static void
source_handle_close_cb(uv_handle_t *handle)
{
    rut_poll_source_t *source = handle->data;

    if (--source->n_uv_handles) {
        c_slice_free(rut_poll_source_t, source);
    }
}

static void
close_source(rut_poll_source_t *source)
{
    int n_uv_handles = 0;

    uv_timer_stop(&source->uv_timer);
    n_uv_handles++;

    if (source->prepare) {
        uv_prepare_stop(&source->uv_prepare);
        n_uv_handles++;
    }

    if (source->fd >= 0) {
        uv_poll_stop(&source->uv_poll);
        n_uv_handles++;
    }

    uv_check_stop(&source->uv_check);
    n_uv_handles++;

    c_warn_if_fail(n_uv_handles == source->n_uv_handles);
}
#endif /* USE_UV */

void
rut_poll_shell_remove_fd(rut_shell_t *shell, int fd)
{
#ifdef USE_UV
    rut_poll_source_t *source = find_fd_source(shell, fd);

    if (!source)
        return;

    shell->poll_sources_age++;

    c_list_remove(&source->link);
    close_source(source);
#else
    c_return_if_reached();
#endif
}

#ifdef USE_UV
static enum uv_poll_event
poll_fd_events_to_uv_events(rut_poll_fd_event_t events)
{
    enum uv_poll_event uv_events = 0;

    if (events & RUT_POLL_FD_EVENT_IN)
        uv_events |= UV_READABLE;

    if (events & RUT_POLL_FD_EVENT_OUT)
        uv_events |= UV_WRITABLE;

    return uv_events;
}

static rut_poll_fd_event_t
uv_events_to_poll_fd_events(enum uv_poll_event events)
{
    rut_poll_fd_event_t poll_fd_events = 0;
    if (events & UV_READABLE)
        poll_fd_events |= RUT_POLL_FD_EVENT_IN;
    if (events & UV_WRITABLE)
        poll_fd_events |= RUT_POLL_FD_EVENT_OUT;

    return poll_fd_events;
}

static void
source_poll_cb(uv_poll_t *poll, int status, int events)
{
    rut_poll_source_t *source = poll->data;
    rut_poll_fd_event_t poll_fd_events;

    rut_set_thread_current_shell(source->shell);

    poll_fd_events = uv_events_to_poll_fd_events(events);
    source->dispatch(source->user_data, source->fd, poll_fd_events);

    rut_set_thread_current_shell(NULL);
}
#endif

void
rut_poll_shell_modify_fd(rut_shell_t *shell, int fd, rut_poll_fd_event_t events)
{
#ifdef USE_UV
    rut_poll_source_t *source = find_fd_source(shell, fd);
    enum uv_poll_event uv_events;

    c_return_if_fail(source != NULL);

    uv_events = poll_fd_events_to_uv_events(events);
    uv_poll_start(&source->uv_poll, uv_events, source_poll_cb);

    shell->poll_sources_age++;
#else
    c_return_if_reached();
#endif
}

#ifdef USE_UV
static void
source_prepare_cb(uv_prepare_t *prepare)
{
    rut_poll_source_t *source = prepare->data;
    int64_t timeout = source->prepare(source->user_data);

    rut_set_thread_current_shell(source->shell);

    if (timeout == 0)
        source->dispatch(source->user_data, source->fd, 0);

    if (timeout >= 0) {
        timeout /= 1000;
        uv_timer_start(&source->uv_timer, dummy_timer_cb, timeout, 0 /* no repeat */);
        uv_check_start(&source->uv_check, dummy_source_timer_check_cb);
    }

    rut_set_thread_current_shell(NULL);
}
#endif

rut_poll_source_t *
rut_poll_shell_add_fd(rut_shell_t *shell,
                      int fd,
                      rut_poll_fd_event_t events,
                      int64_t (*prepare)(void *user_data),
                      void (*dispatch)(void *user_data, int fd, int revents),
                      void *user_data)
{
#ifdef USE_UV
    rut_poll_source_t *source;
    uv_loop_t *loop;

    if (fd >= 0)
        rut_poll_shell_remove_fd(shell, fd);

    source = c_slice_new0(rut_poll_source_t);
    source->shell = shell;
    source->fd = fd;
    source->prepare = prepare;
    source->dispatch = dispatch;
    source->user_data = user_data;

    loop = rut_uv_shell_get_loop(shell);

    uv_timer_init(loop, &source->uv_timer);
    source->uv_timer.data = source;
    source->n_uv_handles++;

    uv_check_init(loop, &source->uv_check);
    source->uv_check.data = source;
    source->n_uv_handles++;

    if (prepare) {
        uv_prepare_init(loop, &source->uv_prepare);
        source->uv_prepare.data = source;
        uv_prepare_start(&source->uv_prepare, source_prepare_cb);
        source->uv_prepare.data = source;
        source->n_uv_handles++;
    }

    if (fd >= 0) {
        enum uv_poll_event uv_events = poll_fd_events_to_uv_events(events);

        uv_poll_init(loop, &source->uv_poll, fd);
        source->uv_poll.data = source;
        uv_poll_start(&source->uv_poll, uv_events, source_poll_cb);
        source->uv_poll.data = source;
        source->n_uv_handles++;
    }

    c_list_insert(shell->poll_sources.prev, &source->link);

    shell->poll_sources_age++;

    return source;
#else
    c_return_val_if_reached(NULL);
#endif
}

#ifdef USE_UV
static void
libuv_dispatch_idles_cb(uv_idle_t *idle)
{
    rut_shell_t *shell = idle->data;

    rut_set_thread_current_shell(shell);
    rut_closure_list_invoke_no_args(&shell->idle_closures);
    rut_set_thread_current_shell(NULL);
}
#endif

#ifdef __EMSCRIPTEN__
static void
em_dispatch_idles_cb(void *user_data)
{
    rut_shell_t *shell = user_data;

    rut_set_thread_current_shell(shell);
    rut_closure_list_invoke_no_args(&shell->idle_closures);
    rut_set_thread_current_shell(NULL);
}
#endif

void
rut_poll_shell_add_idle(rut_shell_t *shell, rut_closure_t *idle)
{
#ifdef __EMSCRIPTEN__
    emscripten_async_call(em_dispatch_idles_cb, shell, 0);
#else
    uv_idle_start(&shell->uv_idle, libuv_dispatch_idles_cb);
#endif

    rut_closure_list_add(&shell->idle_closures, idle);
}

void
rut_poll_shell_remove_idle(rut_shell_t *shell, rut_closure_t *idle)
{
    rut_closure_remove(idle);

#ifdef USE_UV
    if (c_list_empty(&shell->idle_closures))
        uv_idle_stop(&shell->uv_idle);
#endif
}

struct _rut_poll_timer
{
#ifdef USE_UV
    uv_timer_t uv_timer;
#endif
    void (*callback)(rut_poll_timer_t *timer, void *user_data);
    void *user_data;
};

rut_poll_timer_t *
rut_poll_shell_create_timer(rut_shell_t *shell)
{
    rut_poll_timer_t *timer = c_slice_new0(rut_poll_timer_t);

#ifdef USE_UV
    uv_loop_t *loop = rut_uv_shell_get_loop(shell);

    uv_timer_init(loop, &timer->uv_timer);
    timer->uv_timer.data = timer;
#endif

    return timer;
}

static void
timer_cb(rut_poll_timer_t *timer)
{
    if (timer->callback) { /* could be deleted before timeout fired */
        timer->callback(timer, timer->user_data);
        timer->callback = NULL;
        timer->user_data = NULL;
    }
#ifdef __EMSCRIPTEN__
    else {
        /* If timer->callback is NULL that implies
         * rut_shell_delete_timer() was called. Currently Emscripten
         * doesn't provide a way to cancel the timeout via
         * clearTimeout() so we have to wait until the timeout
         * fires to before we can free the timer. */
        c_slice_free(rut_poll_timer_t, timer);
    }
#endif
}

#ifdef USE_UV
static void
uv_timer_fire_cb(uv_timer_t *uv_timer)
{
    rut_poll_timer_t *timer = uv_timer->data;

    timer_cb(timer);
}
#endif

void
rut_poll_shell_add_timeout(rut_shell_t *shell,
                           rut_poll_timer_t *timer,
                           void (*callback)(rut_poll_timer_t *timer,
                                            void *user_data),
                           void *user_data,
                           int timeout)
{
    c_return_if_fail(timer->callback == NULL);

    timer->callback = callback;
    timer->user_data = user_data;

#ifdef USE_UV
    uv_timer_start(&timer->uv_timer, uv_timer_fire_cb, timeout, 0 /* no repeat */);
#elif defined(__EMSCRIPTEN__)
    emscripten_async_call(timer_cb, shell, timeout);
#else
#error "missing rut_shell_add_timeout support"
#endif
}

static void
timer_closed_cb(uv_handle_t *timer)
{
    c_slice_free(rut_poll_timer_t, timer);
}

void
rut_poll_shell_delete_timer(rut_shell_t *shell,
                            rut_poll_timer_t *timer)
{
#ifdef __EMSCRIPTEN__
    /* Emscripten doesn't give us a way to cancel the timeout so only
     * free the timer structure if there's no pending timeout.
     *
     * If there's a pending timeout then the cleared callback and
     * user_data pointers will mark that it has been deleted when
     * the timeout fires.
     */
    if (!timer->callback)
        c_slice_free(rut_poll_timer_t, timer);
#endif

    timer->callback = NULL;
    timer->user_data = NULL;

#ifdef USE_UV
    uv_close((uv_handle_t *)&timer->uv_timer, timer_closed_cb);
#endif
}

rut_closure_t *
rut_poll_shell_add_idle_FIXME(rut_shell_t *shell,
                              void (*idle_cb)(void *user_data),
                              void *user_data,
                              void (*destroy_cb)(void *user_data))
{
    rut_closure_t *closure = c_slice_new(rut_closure_t);
    rut_closure_init(closure, idle_cb, user_data);
    if (destroy_cb)
        rut_closure_set_finalize(closure, destroy_cb);

    rut_poll_shell_add_idle(shell, closure);

    return closure;
}

void
rut_poll_shell_remove_idle_FIXME(rut_shell_t *shell, rut_closure_t *idle)
{
    rut_poll_shell_remove_idle(shell, idle);

    c_slice_free(rut_closure_t, idle);
}


#ifdef USE_GLIB
typedef struct _uv_glib_poll_t {
    rut_shell_t *shell;
    uv_poll_t poll_handle;
    int pollfd_index;
} uv_glib_poll_t;

static void
glib_uv_poll_cb(uv_poll_t *poll, int status, int events)
{
    uv_glib_poll_t *glib_poll = poll->data;
    rut_shell_t *shell = glib_poll->shell;
    GPollFD *pollfd =
        &g_array_index(shell->pollfds, GPollFD, glib_poll->pollfd_index);

    c_warn_if_fail((events & ~(UV_READABLE | UV_WRITABLE)) == 0);

    pollfd->revents = 0;
    if (events & UV_READABLE)
        pollfd->revents |= G_IO_IN;
    if (events & UV_WRITABLE)
        pollfd->revents |= G_IO_OUT;
}

static void
glib_uv_prepare_cb(uv_prepare_t *prepare)
{
    rut_shell_t *shell = prepare->data;
    GMainContext *ctx = shell->glib_main_ctx;
    uv_loop_t *loop;
    GPollFD *pollfds;
    int priority;
    int timeout;
    int i;

    loop = rut_uv_shell_get_loop(shell);

    g_main_context_prepare(ctx, &priority);

    pollfds = (GPollFD *)shell->pollfds->data;
    shell->n_pollfds = g_main_context_query(
        ctx, INT_MAX, &timeout, pollfds, shell->pollfds->len);

    if (shell->n_pollfds > shell->pollfds->len) {
        g_array_set_size(shell->pollfds, shell->n_pollfds);
        g_array_set_size(shell->glib_polls, shell->n_pollfds);
        g_main_context_query(
            ctx, INT_MAX, &timeout, pollfds, shell->pollfds->len);
    }

    for (i = 0; i < shell->n_pollfds; i++) {
        int events = 0;
        uv_glib_poll_t *glib_poll =
            &g_array_index(shell->glib_polls, uv_glib_poll_t, i);

        glib_poll->shell = shell;
        glib_poll->poll_handle.data = glib_poll;
        uv_poll_init(loop, &glib_poll->poll_handle, pollfds[i].fd);
        glib_poll->pollfd_index = i;

        c_warn_if_fail((pollfds[i].events & ~(G_IO_IN | G_IO_OUT)) == 0);

        if (pollfds[i].events & G_IO_IN)
            events |= UV_READABLE;
        if (pollfds[i].events & G_IO_OUT)
            events |= UV_READABLE;

        uv_poll_start(&glib_poll->poll_handle, events, glib_uv_poll_cb);
    }

    if (timeout >= 0) {
        uv_timer_start(&shell->glib_uv_timer, dummy_timer_cb, timeout, 0);
        uv_check_start(&shell->glib_uv_timer_check, dummy_shell_timer_check_cb);
    }
}

static void
glib_uv_check_cb(uv_check_t *check)
{
    rut_shell_t *shell = check->data;
    int i;

    g_main_context_check(shell->glib_main_ctx,
                         INT_MAX,
                         (GPollFD *)shell->pollfds->data,
                         shell->n_pollfds);

    for (i = 0; i < shell->n_pollfds; i++) {
        uv_glib_poll_t *glib_poll =
            &g_array_index(shell->glib_polls, uv_glib_poll_t, i);
        uv_poll_stop(&glib_poll->poll_handle);
    }
    shell->n_pollfds = 0;

    g_main_context_dispatch(shell->glib_main_ctx);
}
#endif /* USE_GLIB */

#ifdef USE_UV
static void
libuv_cg_prepare_callback(uv_prepare_t *prepare)
{
    rut_shell_t *shell = prepare->data;
    cg_renderer_t *renderer =
        cg_device_get_renderer(shell->cg_device);

    cg_loop_dispatch(renderer, NULL, 0);

    update_cg_sources(shell);
}

void
rut_poll_shell_integrate_cg_events_via_libuv(rut_shell_t *shell)
{
    uv_loop_t *loop = rut_uv_shell_get_loop(shell);

    uv_timer_init(loop, &shell->cg_timer);
    uv_check_init(loop, &shell->cg_timer_check);
    shell->cg_timer_check.data = &shell->cg_timer;

    uv_prepare_init(loop, &shell->cg_prepare);
    shell->cg_prepare.data = shell;
    uv_prepare_start(&shell->cg_prepare, libuv_cg_prepare_callback);
}
#endif /* USE_UV */

#ifdef USE_UV
static void
handle_sigchild(uv_signal_t *handle, int signo)
{
    rut_shell_t *shell = handle->data;
    rut_closure_list_invoke_no_args(&shell->sigchild_closures);
}
#endif

void
rut_poll_init(rut_shell_t *shell, rut_shell_t *main_shell)
{
#ifdef USE_UV
    uv_loop_t *loop;
#endif

    shell->main_shell = main_shell;

    c_list_init(&shell->poll_sources);
    c_list_init(&shell->idle_closures);
#ifndef __EMSCRIPTEN__
    c_list_init(&shell->sigchild_closures);
#endif

#ifdef USE_UV
    if (main_shell == NULL) {
        loop = uv_loop_new();

        uv_signal_init(loop, &shell->sigchild_handle);
        shell->sigchild_handle.data = shell;
        uv_signal_start(&shell->sigchild_handle, handle_sigchild, SIGCHLD);

#ifdef USE_GLIB
        /* XXX: Note: glib work will always be associated with
         * the main shell... */

        uv_prepare_init(loop, &shell->glib_uv_prepare);
        shell->glib_uv_prepare.data = shell;

        uv_check_init(loop, &shell->glib_uv_check);
        shell->glib_uv_check.data = shell;

        uv_timer_init(loop, &shell->glib_uv_timer);
        uv_check_init(loop, &shell->glib_uv_timer_check);
        shell->glib_uv_timer_check.data = &shell->glib_uv_timer;

        shell->n_pollfds = 0;
        shell->pollfds = g_array_sized_new(false, false, sizeof(GPollFD), 5);
        shell->glib_polls =
            g_array_sized_new(false, false, sizeof(uv_glib_poll_t), 5);
#endif /* USE_GLIB */

    } else
        loop = main_shell->uv_loop;

    shell->uv_loop = loop;

    uv_idle_init(loop, &shell->uv_idle);
    shell->uv_idle.data = shell;

#endif /* USE_UV */
}

rut_closure_t *
rut_poll_shell_add_sigchild(rut_shell_t *shell,
                            void (*sigchild_cb)(void *user_data),
                            void *user_data,
                            void (*destroy_cb)(void *user_data))
{
#ifdef USE_UV
    return rut_closure_list_add_FIXME(
        &shell->sigchild_closures, sigchild_cb, user_data, destroy_cb);
#else
    c_return_val_if_reached(NULL);
#endif
}

void
rut_poll_shell_remove_sigchild(rut_shell_t *shell, rut_closure_t *sigchild)
{
#ifdef USE_UV
    rut_closure_disconnect_FIXME(sigchild);
#else
    c_return_if_reached();
#endif
}

#ifdef USE_GLIB
static void
rut_glib_poll_run(rut_shell_t *shell)
{
    uv_loop_t *loop = rut_uv_shell_get_loop(shell);
    GMainContext *ctx = g_main_context_get_thread_default();

    if (!ctx)
        ctx = g_main_context_default();

    if (g_main_context_acquire(ctx)) {
        shell->glib_main_ctx = ctx;
        uv_prepare_start(&shell->glib_uv_prepare, glib_uv_prepare_cb);
        uv_check_start(&shell->glib_uv_check, glib_uv_check_cb);
    } else
        c_warning("Failed to acquire glib context");

    rut_set_thread_current_shell(shell);

    if (shell->on_run_cb)
        shell->on_run_cb(shell, shell->on_run_data);
    shell->running = true;

    rut_set_thread_current_shell(NULL);

    uv_run(loop, UV_RUN_DEFAULT);

    g_main_context_release(shell->glib_main_ctx);
}
#endif /* USE_GLIB */

#ifdef __ANDROID__
static int
looper_uv_event_cb(int fd, int events, void *data)
{
    rut_shell_t *shell = data;
    uv_loop_t *loop = rut_uv_shell_get_loop(shell);

    shell->uv_ready = uv_run(loop, UV_RUN_NOWAIT);

    return 1; /* don't unregister */
}

static void
rut_android_poll_run(rut_shell_t *shell)
{
    uv_loop_t *loop = rut_uv_shell_get_loop(shell);
    int backend_fd = uv_backend_fd(loop);
    ALooper *looper = shell->android_application->looper;

    ALooper_addFd(looper, backend_fd, 0, /* ident */
                  ALOOPER_EVENT_INPUT,
                  looper_uv_event_cb,
                  shell);

    shell->quit = false;
    shell->uv_ready = 1;

    while (!shell->quit) {
        int ident;
        int poll_events;
        void *user_data;
        bool ready = shell->uv_ready;

        shell->uv_ready = false;

        ident = ALooper_pollAll(ready ? 0 : -1, NULL,
                                &poll_events, &user_data);
        switch (ident)
        {
        case ALOOPER_POLL_WAKE:
            break;
        case ALOOPER_POLL_TIMEOUT:
            c_warning("Spurious timeout for ALooper_pollAll");
            break;
        case ALOOPER_POLL_ERROR:
            c_error("Spurious error for ALooper_pollAll");
            return;
        case LOOPER_ID_MAIN: {
            struct android_poll_source *source = user_data;
            source->process(shell->android_application, source);
            break;
        }
        case LOOPER_ID_INPUT: {
            struct android_app *app = shell->android_application;
            struct android_poll_source *source = user_data;
            AInputEvent* event = NULL;

            while (AInputQueue_getEvent(app->inputQueue, &event) >= 0) {
                int32_t handled = 0;

                if (AInputQueue_preDispatchEvent(app->inputQueue, event))
                    continue;

                rut_android_shell_handle_input(shell, event);
            }
            break;
        }
        default:
            c_warning("Unknown ALooper_pollAll event identity: %d", ident);
            break;
        }
    }
}
#endif /* __ANDROID__ */

#ifdef __EMSCRIPTEN__
static void
em_paint_loop(void *user_data)
{
    rut_shell_t *shell = user_data;

    emscripten_pause_main_loop();
    shell->paint_loop_running = false;

    rut_shell_paint(shell);
}
#endif

void
rut_poll_run(rut_shell_t *shell)
{
    if (shell->main_shell) {

        if (shell->on_run_cb)
            shell->on_run_cb(shell, shell->on_run_data);
        shell->running = true;

        return;
    }

#if defined(USE_GLIB)
    rut_glib_poll_run(shell);
#elif defined(__ANDROID__)
    rut_android_poll_run(shell);
#elif defined(__EMSCRIPTEN__)
    if (shell->on_run_cb)
        shell->on_run_cb(shell, shell->on_run_data);
    shell->running = true;

    emscripten_set_main_loop_arg(em_paint_loop, shell, -1, true);
#elif defined(USE_UV)
    {
        uv_loop_t *loop = rut_uv_shell_get_loop(shell);

        if (shell->on_run_cb)
            shell->on_run_cb(shell, shell->on_run_data);
        shell->running = true;

        uv_run(loop, UV_RUN_DEFAULT);
    }
#else
#error "No rut_poll_run() mainloop implementation"
#endif
}

void
rut_poll_quit(rut_shell_t *shell)
{
    if (shell->main_shell)
        return;

#if defined(__ANDROID__)
    shell->quit = true;
#elif defined(USE_UV)
    {
        uv_loop_t *loop = rut_uv_shell_get_loop(shell);
        uv_stop(loop);
    }
#endif
}
