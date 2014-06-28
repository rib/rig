/*
 * glist.c: Doubly-linked list implementation
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

#include <config.h>

#include <stdio.h>
#include <clib.h>

c_list_t *
c_list_alloc()
{
    return c_new0(c_list_t, 1);
}

static inline c_list_t *
new_node(c_list_t *prev, void *data, c_list_t *next)
{
    c_list_t *node = c_list_alloc();
    node->data = data;
    node->prev = prev;
    node->next = next;
    if (prev)
        prev->next = node;
    if (next)
        next->prev = node;
    return node;
}

static inline c_list_t *
disconnect_node(c_list_t *node)
{
    if (node->next)
        node->next->prev = node->prev;
    if (node->prev)
        node->prev->next = node->next;
    return node;
}

c_list_t *
c_list_prepend(c_list_t *list, void *data)
{
    return new_node(list ? list->prev : NULL, data, list);
}

void
c_list_free_1(c_list_t *list)
{
    c_free(list);
}

void
c_list_free(c_list_t *list)
{
    while (list) {
        c_list_t *next = list->next;
        c_list_free_1(list);
        list = next;
    }
}

void
c_list_free_full(c_list_t *list, c_destroy_func_t free_func)
{
    while (list) {
        c_list_t *next = list->next;
        free_func(list->data);
        c_list_free_1(list);
        list = next;
    }
}

c_list_t *
c_list_append(c_list_t *list, void *data)
{
    c_list_t *node = new_node(c_list_last(list), data, NULL);
    return list ? list : node;
}

c_list_t *
c_list_concat(c_list_t *list1, c_list_t *list2)
{
    if (list1 && list2) {
        list2->prev = c_list_last(list1);
        list2->prev->next = list2;
    }
    return list1 ? list1 : list2;
}

unsigned int
c_list_length(c_list_t *list)
{
    unsigned int length = 0;

    while (list) {
        length++;
        list = list->next;
    }

    return length;
}

c_list_t *
c_list_remove(c_list_t *list, const void *data)
{
    c_list_t *current = c_list_find(list, data);
    if (!current)
        return list;

    if (current == list)
        list = list->next;
    c_list_free_1(disconnect_node(current));

    return list;
}

c_list_t *
c_list_remove_all(c_list_t *list, const void *data)
{
    c_list_t *current = c_list_find(list, data);

    if (!current)
        return list;

    while (current) {
        if (current == list)
            list = list->next;
        c_list_free_1(disconnect_node(current));

        current = c_list_find(list, data);
    }

    return list;
}

c_list_t *
c_list_remove_link(c_list_t *list, c_list_t *link)
{
    if (list == link)
        list = list->next;

    disconnect_node(link);
    link->next = NULL;
    link->prev = NULL;

    return list;
}

c_list_t *
c_list_delete_link(c_list_t *list, c_list_t *link)
{
    list = c_list_remove_link(list, link);
    c_list_free_1(link);

    return list;
}

c_list_t *
c_list_find(c_list_t *list, const void *data)
{
    while (list) {
        if (list->data == data)
            return list;

        list = list->next;
    }

    return NULL;
}

c_list_t *
c_list_find_custom(c_list_t *list, const void *data, c_compare_func_t func)
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

c_list_t *
c_list_reverse(c_list_t *list)
{
    c_list_t *reverse = NULL;

    while (list) {
        reverse = list;
        list = reverse->next;

        reverse->next = reverse->prev;
        reverse->prev = list;
    }

    return reverse;
}

c_list_t *
c_list_first(c_list_t *list)
{
    if (!list)
        return NULL;

    while (list->prev)
        list = list->prev;

    return list;
}

c_list_t *
c_list_last(c_list_t *list)
{
    if (!list)
        return NULL;

    while (list->next)
        list = list->next;

    return list;
}

c_list_t *
c_list_insert_sorted(c_list_t *list, void *data, c_compare_func_t func)
{
    c_list_t *prev = NULL;
    c_list_t *current;
    c_list_t *node;

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

c_list_t *
c_list_insert_before(c_list_t *list, c_list_t *sibling, void *data)
{
    if (sibling) {
        c_list_t *node = new_node(sibling->prev, data, sibling);
        return list == sibling ? node : list;
    }
    return c_list_append(list, data);
}

void
c_list_foreach(c_list_t *list, c_iter_func_t func, void *user_data)
{
    while (list) {
        (*func)(list->data, user_data);
        list = list->next;
    }
}

int
c_list_index(c_list_t *list, const void *data)
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

c_list_t *
c_list_nth(c_list_t *list, unsigned int n)
{
    for (; list; list = list->next) {
        if (n == 0)
            break;
        n--;
    }
    return list;
}

void *
c_list_nth_data(c_list_t *list, unsigned int n)
{
    c_list_t *node = c_list_nth(list, n);
    return node ? node->data : NULL;
}

c_list_t *
c_list_copy(c_list_t *list)
{
    c_list_t *copy = NULL;

    if (list) {
        c_list_t *tmp = new_node(NULL, list->data, NULL);
        copy = tmp;

        for (list = list->next; list; list = list->next)
            tmp = new_node(tmp, list->data, NULL);
    }

    return copy;
}

typedef c_list_t list_node;
#include "sort.frag.h"

c_list_t *
c_list_sort(c_list_t *list, c_compare_func_t func)
{
    c_list_t *current;
    if (!list || !list->next)
        return list;
    list = do_sort(list, func);

    /* Fixup: do_sort doesn't update 'prev' pointers */
    list->prev = NULL;
    for (current = list; current->next; current = current->next)
        current->next->prev = current;

    return list;
}
