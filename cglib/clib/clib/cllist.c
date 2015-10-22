/*
 * Doubly-linked list implementation
 *
 * Authors:
 *   Duncan Mak (duncan@novell.com)
 *   Raja R Harinath (rharinath@novell.com)
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
 * (C) 2006 Novell, Inc.
 */

#include <clib-config.h>

#include <stdio.h>
#include <clib.h>

c_llist_t *
c_llist_alloc()
{
    return c_new0(c_llist_t, 1);
}

static inline c_llist_t *
new_node(c_llist_t *prev, void *data, c_llist_t *next)
{
    c_llist_t *node = c_llist_alloc();
    node->data = data;
    node->prev = prev;
    node->next = next;
    if (prev)
        prev->next = node;
    if (next)
        next->prev = node;
    return node;
}

static inline c_llist_t *
disconnect_node(c_llist_t *node)
{
    if (node->next)
        node->next->prev = node->prev;
    if (node->prev)
        node->prev->next = node->next;
    return node;
}

c_llist_t *
c_llist_prepend(c_llist_t *list, void *data)
{
    return new_node(list ? list->prev : NULL, data, list);
}

void
c_llist_free_1(c_llist_t *list)
{
    c_free(list);
}

void
c_llist_free(c_llist_t *list)
{
    while (list) {
        c_llist_t *next = list->next;
        c_llist_free_1(list);
        list = next;
    }
}

void
c_llist_free_full(c_llist_t *list, c_destroy_func_t free_func)
{
    while (list) {
        c_llist_t *next = list->next;
        free_func(list->data);
        c_llist_free_1(list);
        list = next;
    }
}

c_llist_t *
c_llist_append(c_llist_t *list, void *data)
{
    c_llist_t *node = new_node(c_llist_last(list), data, NULL);
    return list ? list : node;
}

c_llist_t *
c_llist_concat(c_llist_t *list1, c_llist_t *list2)
{
    if (list1 && list2) {
        list2->prev = c_llist_last(list1);
        list2->prev->next = list2;
    }
    return list1 ? list1 : list2;
}

unsigned int
c_llist_length(c_llist_t *list)
{
    unsigned int length = 0;

    while (list) {
        length++;
        list = list->next;
    }

    return length;
}

c_llist_t *
c_llist_remove(c_llist_t *list, const void *data)
{
    c_llist_t *current = c_llist_find(list, data);
    if (!current)
        return list;

    if (current == list)
        list = list->next;
    c_llist_free_1(disconnect_node(current));

    return list;
}

c_llist_t *
c_llist_remove_all(c_llist_t *list, const void *data)
{
    c_llist_t *current = c_llist_find(list, data);

    if (!current)
        return list;

    while (current) {
        if (current == list)
            list = list->next;
        c_llist_free_1(disconnect_node(current));

        current = c_llist_find(list, data);
    }

    return list;
}

c_llist_t *
c_llist_remove_link(c_llist_t *list, c_llist_t *link)
{
    if (list == link)
        list = list->next;

    disconnect_node(link);
    link->next = NULL;
    link->prev = NULL;

    return list;
}

c_llist_t *
c_llist_delete_link(c_llist_t *list, c_llist_t *link)
{
    list = c_llist_remove_link(list, link);
    c_llist_free_1(link);

    return list;
}

c_llist_t *
c_llist_find(c_llist_t *list, const void *data)
{
    while (list) {
        if (list->data == data)
            return list;

        list = list->next;
    }

    return NULL;
}

c_llist_t *
c_llist_find_custom(c_llist_t *list, const void *data, c_compare_func_t func)
{
    if (!func)
        return NULL;

    while (list) {
        if (func(list->data, data) == 0)
            return list;

        list = list->next;
    }

    return NULL;
}

c_llist_t *
c_llist_reverse(c_llist_t *list)
{
    c_llist_t *reverse = NULL;

    while (list) {
        reverse = list;
        list = reverse->next;

        reverse->next = reverse->prev;
        reverse->prev = list;
    }

    return reverse;
}

c_llist_t *
c_llist_first(c_llist_t *list)
{
    if (!list)
        return NULL;

    while (list->prev)
        list = list->prev;

    return list;
}

c_llist_t *
c_llist_last(c_llist_t *list)
{
    if (!list)
        return NULL;

    while (list->next)
        list = list->next;

    return list;
}

c_llist_t *
c_llist_insert_sorted(c_llist_t *list, void *data, c_compare_func_t func)
{
    c_llist_t *prev = NULL;
    c_llist_t *current;
    c_llist_t *node;

    if (!func)
        return list;

    /* Invariant: !prev || func (prev->data, data) <= 0) */
    for (current = list; current; current = current->next) {
        if (func(current->data, data) > 0)
            break;
        prev = current;
    }

    node = new_node(prev, data, current);
    return list == current ? node : list;
}

c_llist_t *
c_llist_insert_before(c_llist_t *list, c_llist_t *sibling, void *data)
{
    if (sibling) {
        c_llist_t *node = new_node(sibling->prev, data, sibling);
        return list == sibling ? node : list;
    }
    return c_llist_append(list, data);
}

void
c_llist_foreach(c_llist_t *list, c_iter_func_t func, void *user_data)
{
    while (list) {
        (*func)(list->data, user_data);
        list = list->next;
    }
}

int
c_llist_index(c_llist_t *list, const void *data)
{
    int index = 0;

    while (list) {
        if (list->data == data)
            return index;

        index++;
        list = list->next;
    }

    return -1;
}

c_llist_t *
c_llist_nth(c_llist_t *list, unsigned int n)
{
    for (; list; list = list->next) {
        if (n == 0)
            break;
        n--;
    }
    return list;
}

void *
c_llist_nth_data(c_llist_t *list, unsigned int n)
{
    c_llist_t *node = c_llist_nth(list, n);
    return node ? node->data : NULL;
}

c_llist_t *
c_llist_copy(c_llist_t *list)
{
    c_llist_t *copy = NULL;

    if (list) {
        c_llist_t *tmp = new_node(NULL, list->data, NULL);
        copy = tmp;

        for (list = list->next; list; list = list->next)
            tmp = new_node(tmp, list->data, NULL);
    }

    return copy;
}

typedef c_llist_t list_node;
#include "sort.frag.h"

c_llist_t *
c_llist_sort(c_llist_t *list, c_compare_func_t func)
{
    c_llist_t *current;
    if (!list || !list->next)
        return list;
    list = do_sort(list, func);

    /* Fixup: do_sort doesn't update 'prev' pointers */
    list->prev = NULL;
    for (current = list; current->next; current = current->next)
        current->next->prev = current;

    return list;
}
