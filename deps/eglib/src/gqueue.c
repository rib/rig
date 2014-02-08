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
#include <glib.h>

void
g_queue_init (GQueue *queue)
{
	queue->head = NULL;
	queue->tail = NULL;
	queue->length = 0;
}

gpointer
g_queue_peek_head (GQueue *queue)
{
        g_return_val_if_fail (queue, NULL);
        return queue->head ? queue->head->data : NULL;
}

gpointer
g_queue_pop_head (GQueue *queue)
{
	gpointer result;
	GList *old_head;

        g_return_val_if_fail (queue, NULL);

	if (!queue->head)
		return NULL;

	result = queue->head->data;
	old_head = queue->head;
	queue->head = old_head->next;
	g_list_free_1 (old_head);

	if (--queue->length)
		queue->head->prev = NULL;
	else
		queue->tail = NULL;

	return result;
}

gpointer
g_queue_peek_tail (GQueue *queue)
{
        g_return_val_if_fail (queue, NULL);
        return queue->tail ? queue->tail->data : NULL;
}

gpointer
g_queue_pop_tail (GQueue *queue)
{
	gpointer result;
	GList *old_tail;

        g_return_val_if_fail (queue, NULL);

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
	g_list_free_1 (old_tail);

        return result;
}

gboolean
g_queue_is_empty (GQueue *queue)
{
        g_return_val_if_fail (queue, TRUE);
	return queue->length == 0;
}

void
g_queue_push_head (GQueue *queue, gpointer head)
{
        g_return_if_fail (queue);

	queue->head = g_list_prepend (queue->head, head);

	if (!queue->tail)
		queue->tail = queue->head;

	queue->length ++;
}

void
g_queue_push_tail (GQueue *queue, gpointer data)
{
        g_return_if_fail (queue);

	queue->tail = g_list_append (queue->tail, data);
	if (queue->head == NULL)
		queue->head = queue->tail;
	else
		queue->tail = queue->tail->next;
	queue->length++;
}

GQueue *
g_queue_new (void)
{
	return g_new0 (GQueue, 1);
}

void
g_queue_free (GQueue *queue)
{
        g_return_if_fail (queue);

	g_list_free (queue->head);
	g_free (queue);
}

void
g_queue_foreach (GQueue *queue, GFunc func, gpointer user_data)
{
        g_return_if_fail (queue);
        g_return_if_fail (func);

	g_list_foreach (queue->head, func, user_data);
}

GList *
g_queue_find (GQueue *queue, gconstpointer data)
{
        GList *l;

        for (l = queue->head; l; l = l->next)
                if (l->data == data)
                        return l;

        return NULL;
}

void
g_queue_clear (GQueue *queue)
{
        g_return_if_fail (queue);

        g_list_free (queue->head);
        queue->length = 0;
        queue->head = NULL;
        queue->tail = NULL;
}
