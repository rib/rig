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

#include <cogl/cogl.h>

#include "rut-poll.h"
#include "rut-shell.h"
#include "rut-context.h"
#include "rut-os.h"

struct _RutPollSource
{
  int fd;
  int64_t (*prepare) (void *user_data);
  void (*dispatch) (void *user_data,
                    int revents);
  void *user_data;
};

static void
on_cogl_event_cb (void *user_data,
                  int revents)
{
  RutShell *shell = user_data;
  CoglRenderer *renderer =
    cogl_context_get_renderer (shell->rut_ctx->cogl_context);

  cogl_poll_renderer_dispatch (renderer,
                               (CoglPollFD *)shell->poll_fds->data,
                               shell->poll_fds->len);
}

int64_t
get_cogl_info (RutShell *shell)
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
          RutPollFD *poll_fd = &g_array_index (shell->poll_fds, RutPollFD, i);
          rut_poll_shell_remove_fd (shell, poll_fd->fd);
        }

      for (i = 0; i < n_poll_fds; i++)
        {
          rut_poll_shell_add_fd (shell,
                                 poll_fds[i].fd,
                                 poll_fds[i].events, /* assume equivalent */
                                 NULL, /* prepare */
                                 on_cogl_event_cb, /* dispatch */
                                 shell);
        }
    }

  shell->cogl_poll_fds_age = age;

  return cogl_timeout;
}

int
rut_poll_shell_get_info (RutShell *shell,
                         RutPollFD **poll_fds,
                         int *n_poll_fds,
                         int64_t *timeout)
{
  GList *l, *next;

  g_return_val_if_fail (poll_fds != NULL, 0);
  g_return_val_if_fail (n_poll_fds != NULL, 0);
  g_return_val_if_fail (timeout != NULL, 0);

  *timeout = -1;

  if (shell->rut_ctx->cogl_context)
    *timeout = get_cogl_info (shell);

  if (!rut_list_empty (&shell->idle_closures))
    *timeout = 0;

  /* This loop needs to cope with the prepare callback removing its
   * own fd */
  for (l = shell->poll_sources; l; l = next)
    {
      RutPollSource *source = l->data;

      next = l->next;

      if (source->prepare)
        {
          int64_t source_timeout = source->prepare (source->user_data);
          if (source_timeout > 0 &&
              (*timeout == -1 || *timeout > source_timeout))
            *timeout = source_timeout;
        }
    }

  /* This is deliberately set after calling the prepare callbacks in
   * case one of them removes its fd */
  *poll_fds = (void *)shell->poll_fds->data;
  *n_poll_fds = shell->poll_fds->len;

  return shell->poll_sources_age;
}

void
rut_poll_shell_dispatch (RutShell *shell,
                         const RutPollFD *poll_fds,
                         int n_poll_fds)
{
  GList *l, *next;

  rut_closure_list_invoke_no_args (&shell->idle_closures);

  /* This loop needs to cope with the dispatch callback removing its
   * own fd */
  for (l = shell->poll_sources; l; l = next)
    {
      RutPollSource *source = l->data;
      int i;

      next = l->next;

      if (source->fd == -1)
        {
          source->dispatch (source->user_data, 0);
          continue;
        }

      for (i = 0; i < n_poll_fds; i++)
        {
          const RutPollFD *pollfd = &poll_fds[i];

          if (pollfd->fd == source->fd && pollfd->revents & pollfd->events)
            {
              source->dispatch (source->user_data, pollfd->revents);
              break;
            }
        }
    }
}

static int
find_pollfd (RutShell *shell, int fd)
{
  int i;

  for (i = 0; i < shell->poll_fds->len; i++)
    {
      RutPollFD *pollfd = &g_array_index (shell->poll_fds, RutPollFD, i);

      if (pollfd->fd == fd)
        return i;
    }

  return -1;
}

void
rut_poll_shell_remove_fd (RutShell *shell, int fd)
{
  int i = find_pollfd (shell, fd);
  GList *l;

  if (i < 0)
    return;

  g_array_remove_index_fast (shell->poll_fds, i);
  shell->poll_sources_age++;

  for (l = shell->poll_sources; l; l = l->next)
    {
      RutPollSource *source = l->data;
      if (source->fd == fd)
        {
          shell->poll_sources =
            g_list_delete_link (shell->poll_sources, l);
          g_slice_free (RutPollSource, source);
          break;
        }
    }
}

void
rut_poll_shell_modify_fd (RutShell *shell,
                          int fd,
                          RutPollFDEvent events)
{
  int fd_index = find_pollfd (shell, fd);

  if (fd_index == -1)
    g_warn_if_reached ();
  else
    {
      RutPollFD *pollfd =
        &g_array_index (shell->poll_sources, RutPollFD, fd_index);

      pollfd->events = events;
      shell->poll_sources_age++;
    }
}

void
rut_poll_shell_add_fd (RutShell *shell,
                       int fd,
                       RutPollFDEvent events,
                       int64_t (*prepare) (void *user_data),
                       void (*dispatch) (void *user_data,
                                         int revents),
                       void *user_data)
{
  RutPollFD pollfd = {
    fd,
    events
  };
  RutPollSource *source;

  rut_poll_shell_remove_fd (shell, fd);

  source = g_slice_new0 (RutPollSource);
  source->fd = fd;
  source->prepare = prepare;
  source->dispatch = dispatch;
  source->user_data = user_data;

  shell->poll_sources = g_list_prepend (shell->poll_sources, source);

  g_array_append_val (shell->poll_fds, pollfd);
  shell->poll_sources_age++;
}

RutPollSource *
rut_poll_shell_add_source (RutShell *shell,
                           int64_t (*prepare) (void *user_data),
                           void (*dispatch) (void *user_data,
                                             int revents),
                           void *user_data)
{
  RutPollSource *source;

  source = g_slice_new0 (RutPollSource);
  source->fd = -1;
  source->prepare = prepare;
  source->dispatch = dispatch;
  source->user_data = user_data;

  shell->poll_sources = g_list_prepend (shell->poll_sources, source);

  return source;
}

void
rut_poll_shell_remove_source (RutShell *shell,
                              RutPollSource *source)
{
  GList *l;

  for (l = shell->poll_sources; l; l = l->next)
    {
      if (l->data == source)
        {
          shell->poll_sources =
            g_list_delete_link (shell->poll_sources, l);
          g_slice_free (RutPollSource, source);
          break;
        }
    }
}

RutClosure *
rut_poll_shell_add_idle (RutShell *shell,
                         void (*idle_cb) (void *user_data),
                         void *user_data,
                         void (*destroy_cb) (void *user_data))
{
  return rut_closure_list_add (&shell->idle_closures,
                               idle_cb,
                               user_data,
                               destroy_cb);
}

#ifdef __ANDROID__
int
sdl_android_event_filter_cb (void *user_data,
                             SDL_Event *event)
{
  RutShell *shell = user_data;

  if (!SDL_LockMutex (shell->event_pipe_mutex))
    {
      g_warning ("Spurious error locking event pipe mutex: %s", SDL_GetError ());
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

int64_t
prepare_sdl_events (void *user_data)
{
  return SDL_PollEvent (NULL) ? 0 : -1;
}

void
dispatch_sdl_events (RutPollFDEvent events,
                     void *user_data)
{
  RutShell *shell = user_data;
  SDL_Event event;

  while (SDL_PollEvent (&event))
    {
      cogl_sdl_handle_event (shell->rut_ctx->cogl_context,
                             &event);

      rut_shell_handle_sdl_event (shell, &event);
    }
}
#endif

void
rut_poll_init (RutShell *shell)
{
  rut_list_init (&shell->idle_closures);

  shell->poll_fds = g_array_new (FALSE, FALSE, sizeof (RutPollFD));

  /* XXX: On Android we know that SDL events are queued up from a
   * separate thread so we can use an event watch as a means to wake
   * up the mainloop... */
#ifdef __ANDROID__
  int fds[2];

  shell->event_pipe_mutex = SDL_CreateMutex ();
  if (!shell->event_pipe_mutex)
    {
      g_warning ("Failed to create event pipe mutex");
      return;
    }

  if (pipe (fds) < 0)
    {
      g_warning ("Failed to create event pipe");
      SDL_DestroyMutex (shell->event_pipe_mutex);
      return;
    }

  shell->event_pipe_read = fds[0];
  shell->event_pipe_write = fds[1];

  rut_poll_shell_add_fd (shell,
                         shell->event_pipe_read,
                         RUT_POLL_FD_EVENT_IN,
                         prepare_sdl_events,
                         dispatch_sdl_events,
                         shell);

  SDL_AddEventWatch (sdl_android_event_filter_cb, shell);
#endif /* __ANDROID__ */
}
