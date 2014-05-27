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

#include <config.h>

#ifdef USE_UV
#include <uv.h>
#endif

#include <cogl/cogl.h>
#include <cogl/cogl-sdl.h>

#include "rut-poll.h"
#include "rut-shell.h"
#include "rut-context.h"
#include "rut-os.h"

struct _RutPollSource
{
  RutList link;

  int fd;
  int64_t (*prepare) (void *user_data);
  void (*dispatch) (void *user_data,
                    int fd,
                    int revents);
  void *user_data;

#ifdef USE_UV
  uv_timer_t uv_timer;
  uv_poll_t uv_poll;
  uv_prepare_t uv_prepare;
  uv_check_t uv_check;
#endif
};

/* We use dummy timers as a way to affect the timeout value used
 * while polling for events, but rely on the other callbacks
 * to dispatch work.
 */
static void
dummy_timer_cb (uv_timer_t *timer)
{
  /* NOP */
}

static void
dummy_timer_check_cb (uv_check_t *check)
{
  uv_timer_t *timer = check->data;
  uv_timer_stop (timer);
  uv_check_stop (check);
}

#ifndef RIG_SIMULATOR_ONLY
static void
on_cogl_event_cb (void *user_data,
                  int fd,
                  int revents)
{
  RutShell *shell = user_data;
  CoglRenderer *renderer =
    cogl_context_get_renderer (shell->rut_ctx->cogl_context);

  cogl_poll_renderer_dispatch_fd (renderer,
                                  fd,
                                  revents);
}

static void
update_cogl_sources (RutShell *shell)
{
  CoglRenderer *renderer =
    cogl_context_get_renderer (shell->rut_ctx->cogl_context);
  CoglPollFD *poll_fds;
  int n_poll_fds;
  int64_t cogl_timeout;
  int age;

  age = cogl_poll_renderer_get_info (renderer,
                                     &poll_fds,
                                     &n_poll_fds,
                                     &cogl_timeout);

  if (age != shell->cogl_poll_fds_age)
    {
      int i;

      /* Remove any existing Cogl fds before adding the new ones */
      for (i = 0; i < shell->cogl_poll_fds->len; i++)
        {
          CoglPollFD *poll_fd = &c_array_index (shell->cogl_poll_fds, CoglPollFD, i);
          rut_poll_shell_remove_fd (shell, poll_fd->fd);
        }

      for (i = 0; i < n_poll_fds; i++)
        {
          CoglPollFD *poll_fd = &poll_fds[i];
          rut_poll_shell_add_fd (shell,
                                 poll_fd->fd,
                                 poll_fd->events, /* assume equivalent */
                                 NULL, /* prepare */
                                 on_cogl_event_cb, /* dispatch */
                                 shell);
          c_array_append_val (shell->cogl_poll_fds, poll_fd);
        }
    }

  shell->cogl_poll_fds_age = age;

  if (cogl_timeout)
    {
      cogl_timeout /= 1000;
      uv_timer_start (&shell->cogl_timer, dummy_timer_cb, cogl_timeout, 0);
      shell->cogl_check.data = &shell->cogl_timer;
      uv_check_start (&shell->cogl_check, dummy_timer_check_cb);
    }
}
#endif /* RIG_SIMULATOR_ONLY */

static RutPollSource *
find_fd_source (RutShell *shell, int fd)
{
  RutPollSource *tmp;

  rut_list_for_each (tmp, &shell->poll_sources, link)
    {
      if (tmp->fd == fd)
        return tmp;
    }

  return NULL;
}

static void
free_source (RutPollSource *source)
{
  uv_timer_stop (&source->uv_timer);
  uv_prepare_stop (&source->uv_prepare);

  if (source->fd > 0)
    uv_poll_stop (&source->uv_poll);

  c_slice_free (RutPollSource, source);
}

void
rut_poll_shell_remove_fd (RutShell *shell, int fd)
{
  RutPollSource *source = find_fd_source (shell, fd);

  if (!source)
    return;

  shell->poll_sources_age++;

  rut_list_remove (&source->link);
  free_source (source);
}

static enum uv_poll_event
poll_fd_events_to_uv_events (RutPollFDEvent events)
{
  enum uv_poll_event uv_events = 0;

  if (events & RUT_POLL_FD_EVENT_IN)
    uv_events |= UV_READABLE;

  if (events & RUT_POLL_FD_EVENT_OUT)
    uv_events |= UV_WRITABLE;

  return uv_events;
}

static RutPollFDEvent
uv_events_to_poll_fd_events (enum uv_poll_event events)
{
  RutPollFDEvent poll_fd_events = 0;
  if (events & UV_READABLE)
    poll_fd_events |= RUT_POLL_FD_EVENT_IN;
  if (events & UV_WRITABLE)
    poll_fd_events |= RUT_POLL_FD_EVENT_OUT;

  return poll_fd_events;
}

static void
source_poll_cb (uv_poll_t *poll, int status, int events)
{
  RutPollSource *source = poll->data;

  RutPollFDEvent poll_fd_events = uv_events_to_poll_fd_events (events);
  source->dispatch (source->user_data, source->fd, poll_fd_events);
}

void
rut_poll_shell_modify_fd (RutShell *shell,
                          int fd,
                          RutPollFDEvent events)
{
  RutPollSource *source = find_fd_source (shell, fd);
  enum uv_poll_event uv_events;

  c_return_if_fail (source != NULL);

  uv_events = poll_fd_events_to_uv_events (events);
  uv_poll_start (&source->uv_poll, uv_events, source_poll_cb);

  shell->poll_sources_age++;
}

static void
source_prepare_cb (uv_prepare_t *prepare)
{
  RutPollSource *source = prepare->data;
  int64_t timeout = source->prepare (source->user_data);

  if (timeout == 0)
    source->dispatch (source->user_data, source->fd, 0);
  else if (timeout > 0)
    {
      timeout /= 1000;
      uv_timer_start (&source->uv_timer, dummy_timer_cb,
                      timeout, 0 /* no repeat */);
      source->uv_check.data = &source->uv_timer;
      uv_check_start (&source->uv_check, dummy_timer_check_cb);
    }
}

RutPollSource *
rut_poll_shell_add_fd (RutShell *shell,
                       int fd,
                       RutPollFDEvent events,
                       int64_t (*prepare) (void *user_data),
                       void (*dispatch) (void *user_data,
                                         int fd,
                                         int revents),
                       void *user_data)
{
  RutPollSource *source;

  if (fd > 0)
    rut_poll_shell_remove_fd (shell, fd);

  source = c_slice_new0 (RutPollSource);
  source->fd = fd;
  source->prepare = prepare;
  source->dispatch = dispatch;
  source->user_data = user_data;

  uv_timer_init (shell->uv_loop, &source->uv_timer);
  uv_check_init (shell->uv_loop, &source->uv_check);

  if (prepare)
    {
      uv_prepare_init (shell->uv_loop, &source->uv_prepare);
      source->uv_prepare.data = source;
      uv_prepare_start (&source->uv_prepare, source_prepare_cb);
    }

  if (fd > 0)
    {
      enum uv_poll_event uv_events = poll_fd_events_to_uv_events (events);

      uv_poll_init (shell->uv_loop, &source->uv_poll, fd);
      source->uv_poll.data = source;
      uv_poll_start (&source->uv_poll, uv_events, source_poll_cb);
    }

  rut_list_insert (shell->poll_sources.prev, &source->link);

  shell->poll_sources_age++;

  return source;
}

RutPollSource *
rut_poll_shell_add_source (RutShell *shell,
                           int64_t (*prepare) (void *user_data),
                           void (*dispatch) (void *user_data,
                                             int fd,
                                             int revents),
                           void *user_data)
{
  return rut_poll_shell_add_fd (shell,
                                -1, /* fd */
                                0, /* events */
                                prepare,
                                dispatch,
                                user_data);
}

void
rut_poll_shell_remove_source (RutShell *shell,
                              RutPollSource *source)
{
  rut_list_remove (&source->link);
  free_source (source);
}

static void
dispatch_idles_cb (uv_idle_t *idle)
{
  RutShell *shell = idle->data;

  rut_closure_list_invoke_no_args (&shell->idle_closures);
}

RutClosure *
rut_poll_shell_add_idle (RutShell *shell,
                         void (*idle_cb) (void *user_data),
                         void *user_data,
                         void (*destroy_cb) (void *user_data))
{
  uv_idle_start (&shell->uv_idle, dispatch_idles_cb);
  return rut_closure_list_add (&shell->idle_closures,
                               idle_cb,
                               user_data,
                               destroy_cb);
}

void
rut_poll_shell_remove_idle (RutShell *shell, RutClosure *idle)
{
  rut_closure_disconnect (idle);

  if (rut_list_empty (&shell->idle_closures))
    uv_idle_stop (&shell->uv_idle);
}

#ifdef __ANDROID__
int
sdl_android_event_filter_cb (void *user_data,
                             SDL_Event *event)
{
  RutShell *shell = user_data;

  if (SDL_LockMutex (shell->event_pipe_mutex) != 0)
    {
      c_warning ("Spurious error locking event pipe mutex: %s", SDL_GetError ());
      return 1; /* ignored */
    }

  if (!shell->wake_queued)
    {
      rut_os_write (shell->event_pipe_write, "E", 1, NULL);
      shell->wake_queued = true;
    }

  SDL_UnlockMutex (shell->event_pipe_mutex);

  return 1; /* ignored */
}

static void
dispatch_sdl_pipe_events (void *user_data,
                          int revents)
{
  RutShell *shell = user_data;
  SDL_Event event;
  char buf;

  if (SDL_LockMutex (shell->event_pipe_mutex) != 0)
    {
      c_warning ("Spurious error locking event pipe mutex: %s", SDL_GetError ());
      return;
    }

  rut_os_read_len (shell->event_pipe_read, &buf, 1, NULL);
  shell->wake_queued = false;

  SDL_UnlockMutex (shell->event_pipe_mutex);

  while (SDL_PollEvent (&event))
    {
      cogl_sdl_handle_event (shell->rut_ctx->cogl_context,
                             &event);

      rut_shell_handle_sdl_event (shell, &event);
    }
}

static void
integrate_sdl_events_via_pipe (RutShell *shell)
{
  int fds[2];

  shell->event_pipe_mutex = SDL_CreateMutex ();
  if (!shell->event_pipe_mutex)
    {
      c_warning ("Failed to create event pipe mutex: %s",
                 SDL_GetError ());
      return;
    }

  if (pipe (fds) < 0)
    {
      c_warning ("Failed to create event pipe");
      SDL_DestroyMutex (shell->event_pipe_mutex);
      return;
    }

  shell->event_pipe_read = fds[0];
  shell->event_pipe_write = fds[1];

  rut_poll_shell_add_fd (shell,
                         shell->event_pipe_read,
                         RUT_POLL_FD_EVENT_IN,
                         NULL, /* prepare */
                         dispatch_sdl_pipe_events,
                         shell);

  SDL_AddEventWatch (sdl_android_event_filter_cb, shell);
}
#endif /* __ANDROID__ */

static int64_t
prepare_sdl_busy_wait (void *user_data)
{
  return SDL_PollEvent (NULL) ? 0 : 8;
}

static void
dispatch_sdl_busy_wait (void *user_data,
                        int fd,
                        int revents)
{
  RutShell *shell = user_data;
  SDL_Event event;

  while (SDL_PollEvent (&event))
    {
      cogl_sdl_handle_event (shell->rut_ctx->cogl_context, &event);

      rut_shell_handle_sdl_event (shell, &event);
    }
}

static void
integrate_sdl_events_via_busy_wait (RutShell *shell)
{
  rut_poll_shell_add_source (shell,
                             prepare_sdl_busy_wait,
                             dispatch_sdl_busy_wait,
                             shell);
}

#ifdef USE_GLIB
static gboolean
prepare_uv_events (GSource *source, gint *timeout)
{
  UVSource *uv_source (UVSource *)source;
  RutShell *shell = uv_source->shell;

  uv_update_time (shell->uv_loop);

  *timeout = uv_backend_timeout (shell->uv_loop);
}

static gboolean
check_uv_events (GSource *source)
{
  UVSource *uv_source (UVSource *)source;
  RutShell *shell = uv_source->shell;
  GIOCondition events;

  if (!uv_backend_timeout (shell->uv_loop))
    return TRUE;

  events = g_source_query_unix_fd (source, ((UVSource *)source)->tag);

  return events & G_IO_IN;
}

static gboolean
dispatch_uv_events (GSource *source, GSourceFunc callback, gpointer user_data)
{
  UVSource *uv_source (UVSource *)source;
  RutShell *shell = uv_source->shell;

  uv_run (shell->uv_loop, UV_RUN_NOWAIT);

  return TRUE;
}

static UVSource *
add_libuv_gsource (RutShell *shell)
{
  GSourceFuncs source_funcs = {
    .prepare = prepare_uv_events,
    .check = check_uv_events,
    .dispatch = dispatch_uv_events,
  };
  GSource *source = g_source_new (&source_funcs, sizeof (UVSource));
  int fd = uv_backend_fd (shell->uv_loop);

  ((UVSource *)source)->shell = shell;
  ((UVSource *)source)->fd = fd;
  ((UVSource *)source)->tag = g_source_attach_unix_fd (source, fd, G_IO_IN);

  g_source_attach (source, g_main_context_get_thread_default ());
}
#endif /* USE_GLIB */

static void
cogl_prepare_cb (uv_prepare_t *prepare)
{
  RutShell *shell = prepare->data;
  CoglRenderer *renderer =
    cogl_context_get_renderer (shell->rut_ctx->cogl_context);

  cogl_poll_renderer_dispatch (renderer, NULL, 0);

  update_cogl_sources (shell);
}

static void
integrate_cogl_events (RutShell *shell)
{
  uv_timer_init (shell->uv_loop, &shell->cogl_timer);

  uv_prepare_init (shell->uv_loop, &shell->cogl_prepare);
  shell->cogl_prepare.data = shell;
  uv_prepare_start (&shell->cogl_prepare, cogl_prepare_cb);

  uv_check_init (shell->uv_loop, &shell->cogl_check);
}

void
rut_poll_init (RutShell *shell)
{
  rut_list_init (&shell->poll_sources);
  rut_list_init (&shell->idle_closures);

#ifdef USE_UV
  shell->uv_loop = uv_loop_new ();

  uv_idle_init (shell->uv_loop, &shell->uv_idle);
  shell->uv_idle.data = shell;

  /* XXX: On Android we know that SDL events are queued up from a
   * separate thread so we can use an event watch as a means to wake
   * up the mainloop... */
  if (!shell->headless)
    {
      /* XXX: SDL doesn't give us a portable way of blocking for
       * events that is compatible with us polling for other file
       * descriptor events outside of SDL which means we normally
       * resort to busily polling SDL for events.
       *
       * Luckily on Android though we know that events are delivered
       * to SDL in a separate thread which we can monitor and using a
       * pipe we are able to wake up our own polling mainloop. This
       * means we can allow the Glib mainloop to block on Android...
       *
       * TODO: On X11 use XConnectionNumber(sdl_info.info.x11.display)
       * so we can also poll for events on X. One caveat would
       * probably be that we'd subvert SDL being able to specify a
       * timeout for polling.
       */
#ifndef RIG_SIMULATOR_ONLY

#ifdef __ANDROID__
      integrate_sdl_events_via_pipe (shell);
#else
      integrate_sdl_events_via_busy_wait (shell);
#endif

      integrate_cogl_events (shell);
#endif /* RIG_SIMULATOR_ONLY */
    }
#endif /* USE_UV */

#ifdef USE_GLIB
  add_libuv_gsource (shell);
#endif
}

void
rut_poll_run (RutShell *shell)
{
#ifdef USE_GLIB
  GApplication *application = NULL;

  if (!shell->headless)
    application = g_application_get_default ();

  if (application)
    g_application_run (application, 0, NULL);
  else
    {
      shell->main_loop =
        g_main_loop_new (g_main_context_get_thread_default(), TRUE);

      g_main_loop_run (shell->main_loop);
      g_main_loop_unref (shell->main_loop);
    }
#else
  uv_run (shell->uv_loop, UV_RUN_DEFAULT);
#endif
}

void
rut_poll_quit (RutShell *shell)
{
#ifdef USE_GLIB
  GApplication *application = g_application_get_default ();

  if (application)
    g_application_quit (application);
  else
    g_main_loop_quit (shell->main_loop);
#else
  uv_stop (shell->uv_loop);
#endif
}
