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

CList*
c_list_alloc ()
{
	return c_new0 (CList, 1);
}

static inline CList*
new_node (CList *prev, void * data, CList *next)
{
	CList *node = c_list_alloc ();
	node->data = data;
	node->prev = prev;
	node->next = next;
	if (prev)
		prev->next = node;
	if (next)
		next->prev = node;
	return node;
}

static inline CList*
disconnect_node (CList *node)
{
	if (node->next)
		node->next->prev = node->prev;
	if (node->prev)
		node->prev->next = node->next;
	return node;
}

CList *
c_list_prepend (CList *list, void * data)
{
	return new_node (list ? list->prev : NULL, data, list);
}

void
c_list_free_1 (CList *list)
{
	c_free (list);
}

void
c_list_free (CList *list)
{
	while (list){
		CList *next = list->next;
		c_list_free_1 (list);
		list = next;
	}
}

void
c_list_free_full (CList *list, CDestroyNotify free_func)
{
	while (list){
		CList *next = list->next;
                free_func (list->data);
		c_list_free_1 (list);
		list = next;
	}
}

CList*
c_list_append (CList *list, void * data)
{
	CList *node = new_node (c_list_last (list), data, NULL);
	return list ? list : node;
}

CList *
c_list_concat (CList *list1, CList *list2)
{
	if (list1 && list2) {
		list2->prev = c_list_last (list1);
		list2->prev->next = list2;
	}
	return list1 ? list1 : list2;
}

unsigned int
c_list_length (CList *list)
{
	unsigned int length = 0;

	while (list) {
		length ++;
		list = list->next;
	}

	return length;
}

CList*
c_list_remove (CList *list, const void * data)
{
	CList *current = c_list_find (list, data);
	if (!current)
		return list;

	if (current == list)
		list = list->next;
	c_list_free_1 (disconnect_node (current));

	return list;
}

CList*
c_list_remove_all (CList *list, const void * data)
{
	CList *current = c_list_find (list, data);

	if (!current)
		return list;

	while (current) {
		if (current == list)
			list = list->next;
		c_list_free_1 (disconnect_node (current));

		current = c_list_find (list, data);
	}

	return list;
}

CList*
c_list_remove_link (CList *list, CList *link)
{
	if (list == link)
		list = list->next;

	disconnect_node (link);
	link->next = NULL;
	link->prev = NULL;

	return list;
}

CList*
c_list_delete_link (CList *list, CList *link)
{
	list = c_list_remove_link (list, link);
	c_list_free_1 (link);

	return list;
}

CList*
c_list_find (CList *list, const void * data)
{
	while (list){
		if (list->data == data)
			return list;

		list = list->next;
	}

	return NULL;
}

CList*
c_list_find_custom (CList *list, const void * data, CCompareFunc func)
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

CList*
c_list_reverse (CList *list)
{
	CList *reverse = NULL;

	while (list) {
		reverse = list;
		list = reverse->next;

		reverse->next = reverse->prev;
		reverse->prev = list;
	}

	return reverse;
}

CList*
c_list_first (CList *list)
{
	if (!list)
		return NULL;

	while (list->prev)
		list = list->prev;

	return list;
}

CList*
c_list_last (CList *list)
{
	if (!list)
		return NULL;

	while (list->next)
		list = list->next;

	return list;
}

CList*
c_list_insert_sorted (CList *list, void * data, CCompareFunc func)
{
	CList *prev = NULL;
	CList *current;
	CList *node;

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

CList*
c_list_insert_before (CList *list, CList *sibling, void * data)
{
	if (sibling) {
		CList *node = new_node (sibling->prev, data, sibling);
		return list == sibling ? node : list;
	}
	return c_list_append (list, data);
}

void
c_list_foreach (CList *list, CFunc func, void * user_data)
{
	while (list){
		(*func) (list->data, user_data);
		list = list->next;
	}
}

int
c_list_index (CList *list, const void * data)
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

CList*
c_list_nth (CList *list, unsigned int n)
{
	for (; list; list = list->next) {
		if (n == 0)
			break;
		n--;
	}
	return list;
}

void *
c_list_nth_data (CList *list, unsigned int n)
{
	CList *node = c_list_nth (list, n);
	return node ? node->data : NULL;
}

CList*
c_list_copy (CList *list)
{
	CList *copy = NULL;

	if (list) {
		CList *tmp = new_node (NULL, list->data, NULL);
		copy = tmp;

		for (list = list->next; list; list = list->next)
			tmp = new_node (tmp, list->data, NULL);
	}

	return copy;
}

typedef CList list_node;
#include "sort.frag.h"

CList*
c_list_sort (CList *list, CCompareFunc func)
{
	CList *current;
	if (!list || !list->next)
		return list;
	list = do_sort (list, func);

	/* Fixup: do_sort doesn't update 'prev' pointers */
	list->prev = NULL;
	for (current = list; current->next; current = current->next)
		current->next->prev = current;

	return list;
}
