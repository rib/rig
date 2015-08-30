/*
 * Queue
 *
 * Author:
 *   Duncan Mak (duncan@novell.com)
 *   Gonzalo Paniagua Javier (gonzalo@novell.com)
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
 * Copyright (c) 2006-2009 Novell, Inc.
 *
 */

#include <clib-config.h>

#include <stdio.h>
#include <clib.h>

void
c_queue_init(c_queue_t *queue)
{
    queue->head = NULL;
    queue->tail = NULL;
    queue->length = 0;
}

void *
c_queue_peek_head(c_queue_t *queue)
{
    c_return_val_if_fail(queue, NULL);
    return queue->head ? queue->head->data : NULL;
}

void *
c_queue_pop_head(c_queue_t *queue)
{
    void *result;
    c_llist_t *old_head;

    c_return_val_if_fail(queue, NULL);

    if (!queue->head)
        return NULL;

    result = queue->head->data;
    old_head = queue->head;
    queue->head = old_head->next;
    c_llist_free_1(old_head);

    if (--queue->length)
        queue->head->prev = NULL;
    else
        queue->tail = NULL;

    return result;
}

void *
c_queue_peek_tail(c_queue_t *queue)
{
    c_return_val_if_fail(queue, NULL);
    return queue->tail ? queue->tail->data : NULL;
}

void *
c_queue_pop_tail(c_queue_t *queue)
{
    void *result;
    c_llist_t *old_tail;

    c_return_val_if_fail(queue, NULL);

    if (!queue->tail)
        return NULL;

    result = queue->tail->data;
    old_tail = queue->tail;
    queue->tail = old_tail->prev;

    if (old_tail->prev)
        old_tail->prev->next = NULL;
    else
        queue->head = NULL;

    queue->length--;
    c_llist_free_1(old_tail);

    return result;
}

bool
c_queue_is_empty(c_queue_t *queue)
{
    c_return_val_if_fail(queue, true);
    return queue->length == 0;
}

void
c_queue_push_head(c_queue_t *queue, void *head)
{
    c_return_if_fail(queue);

    queue->head = c_llist_prepend(queue->head, head);

    if (!queue->tail)
        queue->tail = queue->head;

    queue->length++;
}

void
c_queue_push_tail(c_queue_t *queue, void *data)
{
    c_return_if_fail(queue);

    queue->tail = c_llist_append(queue->tail, data);
    if (queue->head == NULL)
        queue->head = queue->tail;
    else
        queue->tail = queue->tail->next;
    queue->length++;
}

c_queue_t *
c_queue_new(void)
{
    return c_new0(c_queue_t, 1);
}

void
c_queue_free(c_queue_t *queue)
{
    c_return_if_fail(queue);

    c_llist_free(queue->head);
    c_free(queue);
}

void
c_queue_foreach(c_queue_t *queue, c_iter_func_t func, void *user_data)
{
    c_return_if_fail(queue);
    c_return_if_fail(func);

    c_llist_foreach(queue->head, func, user_data);
}

c_llist_t *
c_queue_find(c_queue_t *queue, const void *data)
{
    c_llist_t *l;

    for (l = queue->head; l; l = l->next)
        if (l->data == data)
            return l;

    return NULL;
}

void
c_queue_clear(c_queue_t *queue)
{
    c_return_if_fail(queue);

    c_llist_free(queue->head);
    queue->length = 0;
    queue->head = NULL;
    queue->tail = NULL;
}
