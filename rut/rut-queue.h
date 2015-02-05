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

#ifndef _RUT_QUEUE_H_
#define _RUT_QUEUE_H_

#include <stdbool.h>

typedef struct _rut_queue_item_t {
    c_list_t list_node;
    void *data;
} rut_queue_item_t;

typedef struct _rut_queue_t {
    c_list_t items;
    int len;
} rut_queue_t;

rut_queue_t *rut_queue_new(void);

void rut_queue_push_tail(rut_queue_t *queue, void *data);

void *rut_queue_peek_tail(rut_queue_t *queue);

void *rut_queue_pop_tail(rut_queue_t *queue);

void *rut_queue_peek_head(rut_queue_t *queue);

void *rut_queue_pop_head(rut_queue_t *queue);

bool rut_queue_remove(rut_queue_t *queue, void *data);

void *rut_queue_peek_nth(rut_queue_t *queue, int n);

void rut_queue_clear(rut_queue_t *queue);

void rut_queue_free(rut_queue_t *queue);

#define rut_queue_is_empty(queue) (!!(queue->len))

/* C++ will gets upset with how c_container_of is implemented */
#ifndef __cplusplus

static inline void
rut_queue_init(rut_queue_t *queue)
{
    c_list_init(&queue->items);
    queue->len = 0;
}

/* Note: we can assume the both lists have at least 1 item in them */
static inline void
_rut_queue_merge_sort_items(c_list_t *sorted,
                            c_list_t *sub0,
                            c_list_t *sub1,
                            int (*compare)(void *data0,
                                           void *data1))
{
    rut_queue_item_t *pos0;
    rut_queue_item_t *pos1;

    pos0 = c_container_of(sub0->next, rut_queue_item_t, list_node);
    pos1 = c_container_of(sub1->next, rut_queue_item_t, list_node);

    do {
        if (compare(pos0->data, pos1->data) <= 0) {
            /* remove */
            pos0->list_node.prev->next = pos0->list_node.next;
            pos0->list_node.next->prev = pos0->list_node.prev;

            /* insert */
            pos0->list_node.prev = sorted->prev;
            pos0->list_node.next = sorted;
            sorted->prev->next = &pos0->list_node;
            sorted->prev = &pos0->list_node;

            pos0 = c_container_of(pos0->list_node.next,
                                  rut_queue_item_t, list_node);
        } else {
            /* remove */
            pos1->list_node.prev->next = pos1->list_node.next;
            pos1->list_node.next->prev = pos1->list_node.prev;

            /* insert */
            pos1->list_node.prev = sorted->prev;
            pos1->list_node.next = sorted;
            sorted->prev->next = &pos1->list_node;
            sorted->prev = &pos1->list_node;

            pos1 = c_container_of(pos1->list_node.next,
                                  rut_queue_item_t, list_node);
        }
    } while (&pos0->list_node != sub0 && &pos1->list_node != sub1);
}

static inline void
_rut_queue_sort_items(
    c_list_t *list, int len, int (*compare)(void *data0, void *data1))
{
    c_list_t sub0, sub1;
    int len0;
    c_list_t *l;
    int i;

    if (len < 2)
        return;

    len0 = len / 2;
    for (i = 0, l = list->next; i < len0; i++, l = l->next)
        ;

    sub1.next = l;
    sub1.prev = list->prev;
    sub0.next = list->next;
    sub0.prev = l->prev;
    l->prev->next = &sub0;
    l->prev = &sub1;

    _rut_queue_sort_items(&sub0, len0, compare);
    _rut_queue_sort_items(&sub1, len - len0, compare);

    _rut_queue_merge_sort_items(list, &sub0, &sub1, compare);
}

static inline void
rut_queue_sort(rut_queue_t *queue,
               int (*compare)(void *data0, void *data1))
{
    _rut_queue_sort_items(&queue->items, queue->len, compare);
}

#endif /* __cplusplus */

#endif /* _RUT_QUEUE_H_ */
