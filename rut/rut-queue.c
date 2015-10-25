/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013  Intel Corporation
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

#include <rut-config.h>

#include "rut-magazine.h"

#include "rut-queue.h"

static rut_magazine_t *_rut_queue_magazine;

static rut_queue_item_t *
alloc_item(void)
{
    if (C_UNLIKELY(_rut_queue_magazine == NULL)) {
        _rut_queue_magazine = rut_magazine_new(sizeof(rut_queue_item_t), 1000);
    }

    return rut_magazine_chunk_alloc(_rut_queue_magazine);
}

static void
free_item(rut_queue_item_t *item)
{
    rut_magazine_chunk_free(_rut_queue_magazine, item);
}

rut_queue_t *
rut_queue_new(void)
{
    rut_queue_t *queue = c_new(rut_queue_t, 1);
    c_list_init(&queue->items);
    queue->len = 0;
    return queue;
}

void
rut_queue_push_tail(rut_queue_t *queue, void *data)
{
    rut_queue_item_t *item = alloc_item();

    item->data = data;

    c_list_insert(queue->items.prev, &item->list_node);
    queue->len++;
}

void *
rut_queue_peek_tail(rut_queue_t *queue)
{
    rut_queue_item_t *item;

    if (c_list_empty(&queue->items))
        return NULL;

    item = c_container_of(queue->items.prev, rut_queue_item_t, list_node);

    return item->data;
}

void *
rut_queue_pop_tail(rut_queue_t *queue)
{
    rut_queue_item_t *item;
    void *ret;

    if (c_list_empty(&queue->items))
        return NULL;

    item = c_container_of(queue->items.prev, rut_queue_item_t, list_node);
    c_list_remove(&item->list_node);

    ret = item->data;

    free_item(item);

    queue->len--;

    return ret;
}

void *
rut_queue_peek_head(rut_queue_t *queue)
{
    rut_queue_item_t *item;

    if (c_list_empty(&queue->items))
        return NULL;

    item = c_container_of(queue->items.next, rut_queue_item_t, list_node);

    return item->data;
}

void *
rut_queue_pop_head(rut_queue_t *queue)
{
    rut_queue_item_t *item;
    void *ret;

    if (c_list_empty(&queue->items))
        return NULL;

    item = c_container_of(queue->items.next, rut_queue_item_t, list_node);
    c_list_remove(&item->list_node);

    ret = item->data;

    free_item(item);

    queue->len--;

    return ret;
}

bool
rut_queue_remove(rut_queue_t *queue, void *data)
{
    rut_queue_item_t *item;

    c_list_for_each(item, &queue->items, list_node)
    {
        if (item->data == data) {
            c_list_remove(&item->list_node);
            free_item(item);
            return true;
        }
    }

    return false;
}

void *
rut_queue_peek_nth(rut_queue_t *queue, int n)
{
    rut_queue_item_t *item;
    int i = 0;

    c_list_for_each(item, &queue->items, list_node)
    {
        if (i++ >= n)
            return item->data;
    }

    return NULL;
}

void
rut_queue_clear(rut_queue_t *queue)
{
    rut_queue_item_t *item, *tmp;

    c_list_for_each_safe(item, tmp, &queue->items, list_node)
    free_item(item);
    rut_queue_init(queue);
}

void
rut_queue_free(rut_queue_t *queue)
{
    c_return_if_fail(queue->len == 0);

    c_free(queue);
}
