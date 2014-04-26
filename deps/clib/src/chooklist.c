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

#include <clib.h>

void
c_hook_list_init (CHookList *hook_list,
                  unsigned int hook_size)
{
  hook_list->hooks = NULL;
}

void
c_hook_list_invoke (CHookList *hook_list,
                    cboolean may_recurse)
{
  CHook *h;

  if (may_recurse)
    {
      for (h = hook_list->hooks; h; h = h->next)
        {
          CHookFunc func = h->func;
          func (h->data);
        }
    }
  else
    {
      for (h = hook_list->hooks; h && !h->in_call; h = h->next)
        {
          CHookFunc func = h->func;
          h->in_call = TRUE;
          func (h->data);
          h->in_call = FALSE;
        }
    }
}

void
c_hook_list_clear (CHookList *hook_list)
{
  while (hook_list->hooks)
    c_hook_destroy_link (hook_list, hook_list->hooks);
}

CHook *
c_hook_alloc (CHookList *hook_list)
{
  return c_new (CHook, 1);
}

CHook *
c_hook_find_func_data (CHookList *hook_list,
                       cboolean need_valids,
                       void * func,
                       void * data)
{
  CHook *h;

  for (h = hook_list->hooks; h; h = h->next)
    {
      if (h->func == func && h->data == data)
        return h;
    }

  return NULL;
}

void
c_hook_destroy_link (CHookList *hook_list,
                     CHook *hook)
{
  if (hook_list->hooks == hook)
    hook_list->hooks = hook->next;

  if (hook->next)
    hook->next->prev = hook->prev;
  if (hook->prev)
    hook->prev->next = hook->next;

  c_free (hook);
}

void
c_hook_prepend (CHookList *hook_list,
                CHook *hook)
{
  CHook *prev = hook_list->hooks ? hook_list->hooks->prev : NULL;
  CHook *next = hook_list->hooks;

  hook->prev = prev;
  hook->next = next;
  if (prev)
    prev->next = hook;
  if (next)
    next->prev = hook;
}
