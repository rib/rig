/*
 * gqueue.c: Queue
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


#include <config.h>

#include <stdio.h>
#include <ulib.h>

void
u_queue_init (UQueue *queue)
{
	queue->head = NULL;
	queue->tail = NULL;
	queue->length = 0;
}

void *
u_queue_peek_head (UQueue *queue)
{
        u_return_val_if_fail (queue, NULL);
        return queue->head ? queue->head->data : NULL;
}

void *
u_queue_pop_head (UQueue *queue)
{
	void * result;
	UList *old_head;

        u_return_val_if_fail (queue, NULL);

	if (!queue->head)
		return NULL;

	result = queue->head->data;
	old_head = queue->head;
	queue->head = old_head->next;
	u_list_free_1 (old_head);

	if (--queue->length)
		queue->head->prev = NULL;
	else
		queue->tail = NULL;

	return result;
}

void *
u_queue_peek_tail (UQueue *queue)
{
        u_return_val_if_fail (queue, NULL);
        return queue->tail ? queue->tail->data : NULL;
}

void *
u_queue_pop_tail (UQueue *queue)
{
	void * result;
	UList *old_tail;

        u_return_val_if_fail (queue, NULL);

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
	u_list_free_1 (old_tail);

        return result;
}

uboolean
u_queue_is_empty (UQueue *queue)
{
        u_return_val_if_fail (queue, TRUE);
	return queue->length == 0;
}

void
u_queue_push_head (UQueue *queue, void * head)
{
        u_return_if_fail (queue);

	queue->head = u_list_prepend (queue->head, head);

	if (!queue->tail)
		queue->tail = queue->head;

	queue->length ++;
}

void
u_queue_push_tail (UQueue *queue, void * data)
{
        u_return_if_fail (queue);

	queue->tail = u_list_append (queue->tail, data);
	if (queue->head == NULL)
		queue->head = queue->tail;
	else
		queue->tail = queue->tail->next;
	queue->length++;
}

UQueue *
u_queue_new (void)
{
	return u_new0 (UQueue, 1);
}

void
u_queue_free (UQueue *queue)
{
        u_return_if_fail (queue);

	u_list_free (queue->head);
	u_free (queue);
}

void
u_queue_foreach (UQueue *queue, UFunc func, void * user_data)
{
        u_return_if_fail (queue);
        u_return_if_fail (func);

	u_list_foreach (queue->head, func, user_data);
}

UList *
u_queue_find (UQueue *queue, const void * data)
{
        UList *l;

        for (l = queue->head; l; l = l->next)
                if (l->data == data)
                        return l;

        return NULL;
}

void
u_queue_clear (UQueue *queue)
{
        u_return_if_fail (queue);

        u_list_free (queue->head);
        queue->length = 0;
        queue->head = NULL;
        queue->tail = NULL;
}
