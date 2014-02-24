/*
 * Rut
 *
 * Copyright (C) 2013  Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "rut-magazine.h"

#include "rut-queue.h"

static RutMagazine *_rut_queue_magazine;

static RutQueueItem *
alloc_item (void)
{
  if (G_UNLIKELY (_rut_queue_magazine == NULL))
    {
      _rut_queue_magazine =
        rut_magazine_new (sizeof (RutQueueItem), 1000);
    }

  return rut_magazine_chunk_alloc (_rut_queue_magazine);
}

static void
free_item (RutQueueItem *item)
{
  rut_magazine_chunk_free (_rut_queue_magazine, item);
}

RutQueue *
rut_queue_new (void)
{
  RutQueue *queue = g_new (RutQueue, 1);
  rut_list_init (&queue->items);
  queue->len = 0;
  return queue;
}

void
rut_queue_push_tail (RutQueue *queue, void *data)
{
  RutQueueItem *item = alloc_item ();

  item->data = data;

  rut_list_insert (queue->items.prev, &item->list_node);
  queue->len++;
}

void *
rut_queue_peek_tail (RutQueue *queue)
{
  RutQueueItem *item;

  if (rut_list_empty (&queue->items))
    return NULL;

  item = rut_container_of (queue->items.prev, item, list_node);

  return item->data;
}

void *
rut_queue_pop_tail (RutQueue *queue)
{
  RutQueueItem *item;
  void *ret;

  if (rut_list_empty (&queue->items))
    return NULL;

  item = rut_container_of (queue->items.prev, item, list_node);
  rut_list_remove (&item->list_node);

  ret = item->data;

  free_item (item);

  queue->len--;

  return ret;
}

void *
rut_queue_peek_head (RutQueue *queue)
{
  RutQueueItem *item;

  if (rut_list_empty (&queue->items))
    return NULL;

  item = rut_container_of (queue->items.next, item, list_node);

  return item->data;
}

void *
rut_queue_pop_head (RutQueue *queue)
{
  RutQueueItem *item;
  void *ret;

  if (rut_list_empty (&queue->items))
    return NULL;

  item = rut_container_of (queue->items.next, item, list_node);
  rut_list_remove (&item->list_node);

  ret = item->data;

  free_item (item);

  queue->len--;

  return ret;
}

bool
rut_queue_remove (RutQueue *queue, void *data)
{
  RutQueueItem *item;

  rut_list_for_each (item, &queue->items, list_node)
    {
      if (item->data == data)
        {
          rut_list_remove (&item->list_node);
          free_item (item);
          return true;
        }
    }

  return false;
}

void *
rut_queue_peek_nth (RutQueue *queue, int n)
{
  RutQueueItem *item;
  int i = 0;

  rut_list_for_each (item, &queue->items, list_node)
    {
      if (i++ >= n)
        return item->data;
    }

  return NULL;
}

void
rut_queue_clear (RutQueue *queue)
{
  RutQueueItem *item, *tmp;

  rut_list_for_each_safe (item, tmp, &queue->items, list_node)
    free_item (item);
  rut_queue_init (queue);
}

void
rut_queue_free (RutQueue *queue)
{
  g_return_if_fail (queue->len == 0);

  g_free (queue);
}
