/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2014  Intel Corporation
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

#include <config.h>

#include <clib.h>

#include "rut-glib-source.h"
#include "rut-poll.h"

typedef struct _RutGLibSource
{
  GSource source;

  RutShell *shell;

  CArray *poll_fds;
  int poll_fds_age;

  int64_t expiration_time;
} RutGLibSource;

static gboolean
rut_glib_source_prepare (GSource *source, int *timeout)
{
  RutGLibSource *rut_source = (RutGLibSource *) source;
  RutPollFD *poll_fds;
  int n_poll_fds;
  int64_t rut_timeout;
  int age;
  int i;

  age = rut_poll_shell_get_info (rut_source->shell,
                                 &poll_fds,
                                 &n_poll_fds,
                                 &rut_timeout);

  /* We have to be careful not to call g_source_add/remove_poll unless
   * the FDs have changed because it will cause the main loop to
   * immediately wake up. If we call it every time the source is
   * prepared it will effectively never go idle. */
  if (age != rut_source->poll_fds_age)
    {
      /* Remove any existing polls before adding the new ones */
      for (i = 0; i < rut_source->poll_fds->len; i++)
        {
          GPollFD *poll_fd = &c_array_index (rut_source->poll_fds, GPollFD, i);
          g_source_remove_poll (source, poll_fd);
        }

      c_array_set_size (rut_source->poll_fds, n_poll_fds);

      for (i = 0; i < n_poll_fds; i++)
        {
          GPollFD *poll_fd = &c_array_index (rut_source->poll_fds, GPollFD, i);
          poll_fd->fd = poll_fds[i].fd;
          g_source_add_poll (source, poll_fd);
        }
    }

  rut_source->poll_fds_age = age;

  /* Update the events */
  for (i = 0; i < n_poll_fds; i++)
    {
      GPollFD *poll_fd = &c_array_index (rut_source->poll_fds, GPollFD, i);
      poll_fd->events = poll_fds[i].events;
      poll_fd->revents = 0;
    }

  *timeout = rut_timeout;

  return *timeout == 0;
}

static gboolean
rut_glib_source_check (GSource *source)
{
  RutGLibSource *rut_source = (RutGLibSource *) source;
  int i;

  for (i = 0; i < rut_source->poll_fds->len; i++)
    {
      GPollFD *poll_fd = &c_array_index (rut_source->poll_fds, GPollFD, i);
      if (poll_fd->revents != 0)
        return TRUE;
    }

  return FALSE;
}

static gboolean
rut_glib_source_dispatch (GSource *source,
                          GSourceFunc callback,
                          void *user_data)
{
  RutGLibSource *rut_source = (RutGLibSource *) source;
  RutPollFD *poll_fds =
    (RutPollFD *) &c_array_index (rut_source->poll_fds, GPollFD, 0);

  rut_poll_shell_dispatch (rut_source->shell,
                           poll_fds,
                           rut_source->poll_fds->len);

  return TRUE;
}

static void
rut_glib_source_finalize (GSource *source)
{
  RutGLibSource *rut_source = (RutGLibSource *) source;

  c_array_free (rut_source->poll_fds, TRUE);
}

static GSourceFuncs
rut_glib_source_funcs =
  {
    rut_glib_source_prepare,
    rut_glib_source_check,
    rut_glib_source_dispatch,
    rut_glib_source_finalize
  };

GSource *
rut_glib_shell_source_new (RutShell *shell,
                           int priority)
{
  GSource *source;
  RutGLibSource *rut_source;

  source = g_source_new (&rut_glib_source_funcs,
                         sizeof (RutGLibSource));
  rut_source = (RutGLibSource *) source;

  rut_source->shell = shell;
  rut_source->poll_fds = c_array_new (FALSE, FALSE, sizeof (GPollFD));

  if (priority != G_PRIORITY_DEFAULT)
    g_source_set_priority (source, priority);

  return source;
}
