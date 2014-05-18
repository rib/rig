/*
 * gslist.c: Singly-linked list implementation
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

CSList*
c_slist_alloc (void)
{
	return c_new0 (CSList, 1);
}

void
c_slist_free_1 (CSList *list)
{
	c_free (list);
}

CSList*
c_slist_append (CSList *list, void * data)
{
	return c_slist_concat (list, c_slist_prepend (NULL, data));
}

/* This is also a list node constructor. */
CSList*
c_slist_prepend (CSList *list, void * data)
{
	CSList *head = c_slist_alloc ();
	head->data = data;
	head->next = list;

	return head;
}

/*
 * Insert the given data in a new node after the current node. 
 * Return new node.
 */
static inline CSList *
insert_after (CSList *list, void * data)
{
	list->next = c_slist_prepend (list->next, data);
	return list->next;
}

/*
 * Return the node prior to the node containing 'data'.
 * If the list is empty, or the first node contains 'data', return NULL.
 * If no node contains 'data', return the last node.
 */
static inline CSList*
find_prev (CSList *list, const void * data)
{
	CSList *prev = NULL;
	while (list) {
		if (list->data == data)
			break;
		prev = list;
		list = list->next;
	}
	return prev;
}

/* like 'find_prev', but searches for node 'link' */
static inline CSList*
find_prev_link (CSList *list, CSList *link)
{
	CSList *prev = NULL;
	while (list) {
		if (list == link)
			break;
		prev = list;
		list = list->next;
	}
	return prev;
}

CSList*
c_slist_insert_before (CSList *list, CSList *sibling, void * data)
{
	CSList *prev = find_prev_link (list, sibling);

	if (!prev)
		return c_slist_prepend (list, data);

	insert_after (prev, data);
	return list;
}

void
c_slist_free (CSList *list)
{
	while (list) {
		CSList *next = list->next;
		c_slist_free_1 (list);
		list = next;
	}
}

CSList*
c_slist_copy (CSList *list)
{
	CSList *copy, *tmp;

	if (!list)
		return NULL;

	copy = c_slist_prepend (NULL, list->data);
	tmp = copy;

	for (list = list->next; list; list = list->next)
		tmp = insert_after (tmp, list->data);

	return copy;
}

CSList*
c_slist_concat (CSList *list1, CSList *list2)
{
	if (!list1)
		return list2;

	c_slist_last (list1)->next = list2;
	return list1;
}

void
c_slist_foreach (CSList *list, CFunc func, void * user_data)
{
	while (list) {
		(*func) (list->data, user_data);
		list = list->next;
	}
}

CSList*
c_slist_last (CSList *list)
{
	if (!list)
		return NULL;

	while (list->next)
		list = list->next;

	return list;
}

CSList*
c_slist_find (CSList *list, const void * data)
{
	for (; list; list = list->next)
		if (list->data == data)
			return list;
	return NULL;
}

CSList *
c_slist_find_custom (CSList *list, const void * data, CCompareFunc func)
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

unsigned int
c_slist_length (CSList *list)
{
	unsigned int length = 0;

	while (list) {
		length ++;
		list = list->next;
	}

	return length;
}

CSList*
c_slist_remove (CSList *list, const void * data)
{
	CSList *prev = find_prev (list, data);
	CSList *current = prev ? prev->next : list;

	if (current) {
		if (prev)
			prev->next = current->next;
		else
			list = current->next;
		c_slist_free_1 (current);
	}

	return list;
}

CSList*
c_slist_remove_all (CSList *list, const void * data)
{
	CSList *next = list;
	CSList *prev = NULL;
	CSList *current;

	while (next) {
		CSList *tmp_prev = find_prev (next, data);
		if (tmp_prev)
			prev = tmp_prev;
		current = prev ? prev->next : list;

		if (!current)
			break;

		next = current->next;

		if (prev)
			prev->next = next;
		else
			list = next;
		c_slist_free_1 (current);
	}

	return list;
}

CSList*
c_slist_remove_link (CSList *list, CSList *link)
{
	CSList *prev = find_prev_link (list, link);
	CSList *current = prev ? prev->next : list;

	if (current) {
		if (prev)
			prev->next = current->next;
		else
			list = current->next;
		current->next = NULL;
	}

	return list;
}

CSList*
c_slist_delete_link (CSList *list, CSList *link)
{
	list = c_slist_remove_link (list, link);
	c_slist_free_1 (link);

	return list;
}

CSList*
c_slist_reverse (CSList *list)
{
	CSList *prev = NULL;
	while (list){
		CSList *next = list->next;
		list->next = prev;
		prev = list;
		list = next;
	}

	return prev;
}

CSList*
c_slist_insert_sorted (CSList *list, void * data, CCompareFunc func)
{
	CSList *prev = NULL;
	
	if (!func)
		return list;

	if (!list || func (list->data, data) > 0)
		return c_slist_prepend (list, data);

	/* Invariant: func (prev->data, data) <= 0) */
	for (prev = list; prev->next; prev = prev->next)
		if (func (prev->next->data, data) > 0)
			break;

	/* ... && (prev->next == 0 || func (prev->next->data, data) > 0)) */
	insert_after (prev, data);
	return list;
}

int
c_slist_index (CSList *list, const void * data)
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

CSList*
c_slist_nth (CSList *list, unsigned int n)
{
	for (; list; list = list->next) {
		if (n == 0)
			break;
		n--;
	}
	return list;
}

void *
c_slist_nth_data (CSList *list, unsigned int n)
{
	CSList *node = c_slist_nth (list, n);
	return node ? node->data : NULL;
}

typedef CSList list_node;
#include "sort.frag.h"

CSList*
c_slist_sort (CSList *list, CCompareFunc func)
{
	if (!list || !list->next)
		return list;
	return do_sort (list, func);
}
