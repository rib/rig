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
#include <ulib.h>

UList*
u_list_alloc ()
{
	return u_new0 (UList, 1);
}

static inline UList*
new_node (UList *prev, void * data, UList *next)
{
	UList *node = u_list_alloc ();
	node->data = data;
	node->prev = prev;
	node->next = next;
	if (prev)
		prev->next = node;
	if (next)
		next->prev = node;
	return node;
}

static inline UList*
disconnect_node (UList *node)
{
	if (node->next)
		node->next->prev = node->prev;
	if (node->prev)
		node->prev->next = node->next;
	return node;
}

UList *
u_list_prepend (UList *list, void * data)
{
	return new_node (list ? list->prev : NULL, data, list);
}

void
u_list_free_1 (UList *list)
{
	u_free (list);
}

void
u_list_free (UList *list)
{
	while (list){
		UList *next = list->next;
		u_list_free_1 (list);
		list = next;
	}
}

void
u_list_free_full (UList *list, UDestroyNotify free_func)
{
	while (list){
		UList *next = list->next;
                free_func (list->data);
		u_list_free_1 (list);
		list = next;
	}
}

UList*
u_list_append (UList *list, void * data)
{
	UList *node = new_node (u_list_last (list), data, NULL);
	return list ? list : node;
}

UList *
u_list_concat (UList *list1, UList *list2)
{
	if (list1 && list2) {
		list2->prev = u_list_last (list1);
		list2->prev->next = list2;
	}
	return list1 ? list1 : list2;
}

unsigned int
u_list_length (UList *list)
{
	unsigned int length = 0;

	while (list) {
		length ++;
		list = list->next;
	}

	return length;
}

UList*
u_list_remove (UList *list, const void * data)
{
	UList *current = u_list_find (list, data);
	if (!current)
		return list;

	if (current == list)
		list = list->next;
	u_list_free_1 (disconnect_node (current));

	return list;
}

UList*
u_list_remove_all (UList *list, const void * data)
{
	UList *current = u_list_find (list, data);

	if (!current)
		return list;

	while (current) {
		if (current == list)
			list = list->next;
		u_list_free_1 (disconnect_node (current));

		current = u_list_find (list, data);
	}

	return list;
}

UList*
u_list_remove_link (UList *list, UList *link)
{
	if (list == link)
		list = list->next;

	disconnect_node (link);
	link->next = NULL;
	link->prev = NULL;

	return list;
}

UList*
u_list_delete_link (UList *list, UList *link)
{
	list = u_list_remove_link (list, link);
	u_list_free_1 (link);

	return list;
}

UList*
u_list_find (UList *list, const void * data)
{
	while (list){
		if (list->data == data)
			return list;

		list = list->next;
	}

	return NULL;
}

UList*
u_list_find_custom (UList *list, const void * data, UCompareFunc func)
{
	if (!func)
		return NULL;
	
	while (list) {
		if (func (list->data, data) == 0)
			return list;
		
		list = list->next;
	}
	
	return NULL;
}

UList*
u_list_reverse (UList *list)
{
	UList *reverse = NULL;

	while (list) {
		reverse = list;
		list = reverse->next;

		reverse->next = reverse->prev;
		reverse->prev = list;
	}

	return reverse;
}

UList*
u_list_first (UList *list)
{
	if (!list)
		return NULL;

	while (list->prev)
		list = list->prev;

	return list;
}

UList*
u_list_last (UList *list)
{
	if (!list)
		return NULL;

	while (list->next)
		list = list->next;

	return list;
}

UList*
u_list_insert_sorted (UList *list, void * data, UCompareFunc func)
{
	UList *prev = NULL;
	UList *current;
	UList *node;

	if (!func)
		return list;

	/* Invariant: !prev || func (prev->data, data) <= 0) */
	for (current = list; current; current = current->next) {
		if (func (current->data, data) > 0)
			break;
		prev = current;
	}

	node = new_node (prev, data, current);
	return list == current ? node : list;
}

UList*
u_list_insert_before (UList *list, UList *sibling, void * data)
{
	if (sibling) {
		UList *node = new_node (sibling->prev, data, sibling);
		return list == sibling ? node : list;
	}
	return u_list_append (list, data);
}

void
u_list_foreach (UList *list, UFunc func, void * user_data)
{
	while (list){
		(*func) (list->data, user_data);
		list = list->next;
	}
}

int
u_list_index (UList *list, const void * data)
{
	int index = 0;

	while (list){
		if (list->data == data)
			return index;

		index ++;
		list = list->next;
	}

	return -1;
}

UList*
u_list_nth (UList *list, unsigned int n)
{
	for (; list; list = list->next) {
		if (n == 0)
			break;
		n--;
	}
	return list;
}

void *
u_list_nth_data (UList *list, unsigned int n)
{
	UList *node = u_list_nth (list, n);
	return node ? node->data : NULL;
}

UList*
u_list_copy (UList *list)
{
	UList *copy = NULL;

	if (list) {
		UList *tmp = new_node (NULL, list->data, NULL);
		copy = tmp;

		for (list = list->next; list; list = list->next)
			tmp = new_node (tmp, list->data, NULL);
	}

	return copy;
}

typedef UList list_node;
#include "sort.frag.h"

UList*
u_list_sort (UList *list, UCompareFunc func)
{
	UList *current;
	if (!list || !list->next)
		return list;
	list = do_sort (list, func);

	/* Fixup: do_sort doesn't update 'prev' pointers */
	list->prev = NULL;
	for (current = list; current->next; current = current->next)
		current->next->prev = current;

	return list;
}
