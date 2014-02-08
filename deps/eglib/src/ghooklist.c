/*
 * ghooklist.c: API for manipulating a list of hook functions
 *
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

#include <config.h>

#include <glib.h>

void
g_hook_list_init (GHookList *hook_list,
                  guint hook_size)
{
  hook_list->hooks = NULL;
}

void
g_hook_list_invoke (GHookList *hook_list,
                    gboolean may_recurse)
{
  GHook *h;

  if (may_recurse)
    {
      for (h = hook_list->hooks; h; h = h->next)
        {
          GHookFunc func = h->func;
          func (h->data);
        }
    }
  else
    {
      for (h = hook_list->hooks; h && !h->in_call; h = h->next)
        {
          GHookFunc func = h->func;
          h->in_call = TRUE;
          func (h->data);
          h->in_call = FALSE;
        }
    }
}

void
g_hook_list_clear (GHookList *hook_list)
{
  while (hook_list->hooks)
    g_hook_destroy_link (hook_list, hook_list->hooks);
}

GHook *
g_hook_alloc (GHookList *hook_list)
{
  return g_new (GHook, 1);
}

GHook *
g_hook_find_func_data (GHookList *hook_list,
                       gboolean need_valids,
                       gpointer func,
                       gpointer data)
{
  GHook *h;

  for (h = hook_list->hooks; h; h = h->next)
    {
      if (h->func == func && h->data == data)
        return h;
    }

  return NULL;
}

void
g_hook_destroy_link (GHookList *hook_list,
                     GHook *hook)
{
  if (hook_list->hooks == hook)
    hook_list->hooks = hook->next;

  if (hook->next)
    hook->next->prev = hook->prev;
  if (hook->prev)
    hook->prev->next = hook->next;

  g_free (hook);
}

void
g_hook_prepend (GHookList *hook_list,
                GHook *hook)
{
  GHook *prev = hook_list->hooks ? hook_list->hooks->prev : NULL;
  GHook *next = hook_list->hooks;

  hook->prev = prev;
  hook->next = next;
  if (prev)
    prev->next = hook;
  if (next)
    next->prev = hook;
}
