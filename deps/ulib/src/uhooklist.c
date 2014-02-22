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

#include <ulib.h>

void
u_hook_list_init (UHookList *hook_list,
                  unsigned int hook_size)
{
  hook_list->hooks = NULL;
}

void
u_hook_list_invoke (UHookList *hook_list,
                    uboolean may_recurse)
{
  UHook *h;

  if (may_recurse)
    {
      for (h = hook_list->hooks; h; h = h->next)
        {
          UHookFunc func = h->func;
          func (h->data);
        }
    }
  else
    {
      for (h = hook_list->hooks; h && !h->in_call; h = h->next)
        {
          UHookFunc func = h->func;
          h->in_call = TRUE;
          func (h->data);
          h->in_call = FALSE;
        }
    }
}

void
u_hook_list_clear (UHookList *hook_list)
{
  while (hook_list->hooks)
    u_hook_destroy_link (hook_list, hook_list->hooks);
}

UHook *
u_hook_alloc (UHookList *hook_list)
{
  return u_new (UHook, 1);
}

UHook *
u_hook_find_func_data (UHookList *hook_list,
                       uboolean need_valids,
                       void * func,
                       void * data)
{
  UHook *h;

  for (h = hook_list->hooks; h; h = h->next)
    {
      if (h->func == func && h->data == data)
        return h;
    }

  return NULL;
}

void
u_hook_destroy_link (UHookList *hook_list,
                     UHook *hook)
{
  if (hook_list->hooks == hook)
    hook_list->hooks = hook->next;

  if (hook->next)
    hook->next->prev = hook->prev;
  if (hook->prev)
    hook->prev->next = hook->next;

  u_free (hook);
}

void
u_hook_prepend (UHookList *hook_list,
                UHook *hook)
{
  UHook *prev = hook_list->hooks ? hook_list->hooks->prev : NULL;
  UHook *next = hook_list->hooks;

  hook->prev = prev;
  hook->next = next;
  if (prev)
    prev->next = hook;
  if (next)
    next->prev = hook;
}
