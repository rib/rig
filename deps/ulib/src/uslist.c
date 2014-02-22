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
#include <ulib.h>

USList*
u_slist_alloc (void)
{
	return u_new0 (USList, 1);
}

void
u_slist_free_1 (USList *list)
{
	u_free (list);
}

USList*
u_slist_append (USList *list, void * data)
{
	return u_slist_concat (list, u_slist_prepend (NULL, data));
}

/* This is also a list node constructor. */
USList*
u_slist_prepend (USList *list, void * data)
{
	USList *head = u_slist_alloc ();
	head->data = data;
	head->next = list;

	return head;
}

/*
 * Insert the given data in a new node after the current node. 
 * Return new node.
 */
static inline USList *
insert_after (USList *list, void * data)
{
	list->next = u_slist_prepend (list->next, data);
	return list->next;
}

/*
 * Return the node prior to the node containing 'data'.
 * If the list is empty, or the first node contains 'data', return NULL.
 * If no node contains 'data', return the last node.
 */
static inline USList*
find_prev (USList *list, const void * data)
{
	USList *prev = NULL;
	while (list) {
		if (list->data == data)
			break;
		prev = list;
		list = list->next;
	}
	return prev;
}

/* like 'find_prev', but searches for node 'link' */
static inline USList*
find_prev_link (USList *list, USList *link)
{
	USList *prev = NULL;
	while (list) {
		if (list == link)
			break;
		prev = list;
		list = list->next;
	}
	return prev;
}

USList*
u_slist_insert_before (USList *list, USList *sibling, void * data)
{
	USList *prev = find_prev_link (list, sibling);

	if (!prev)
		return u_slist_prepend (list, data);

	insert_after (prev, data);
	return list;
}

void
u_slist_free (USList *list)
{
	while (list) {
		USList *next = list->next;
		u_slist_free_1 (list);
		list = next;
	}
}

USList*
u_slist_copy (USList *list)
{
	USList *copy, *tmp;

	if (!list)
		return NULL;

	copy = u_slist_prepend (NULL, list->data);
	tmp = copy;

	for (list = list->next; list; list = list->next)
		tmp = insert_after (tmp, list->data);

	return copy;
}

USList*
u_slist_concat (USList *list1, USList *list2)
{
	if (!list1)
		return list2;

	u_slist_last (list1)->next = list2;
	return list1;
}

void
u_slist_foreach (USList *list, UFunc func, void * user_data)
{
	while (list) {
		(*func) (list->data, user_data);
		list = list->next;
	}
}

USList*
u_slist_last (USList *list)
{
	if (!list)
		return NULL;

	while (list->next)
		list = list->next;

	return list;
}

USList*
u_slist_find (USList *list, const void * data)
{
	for (; list; list = list->next)
		if (list->data == data)
			return list;
	return NULL;
}

USList *
u_slist_find_custom (USList *list, const void * data, UCompareFunc func)
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
u_slist_length (USList *list)
{
	unsigned int length = 0;

	while (list) {
		length ++;
		list = list->next;
	}

	return length;
}

USList*
u_slist_remove (USList *list, const void * data)
{
	USList *prev = find_prev (list, data);
	USList *current = prev ? prev->next : list;

	if (current) {
		if (prev)
			prev->next = current->next;
		else
			list = current->next;
		u_slist_free_1 (current);
	}

	return list;
}

USList*
u_slist_remove_all (USList *list, const void * data)
{
	USList *next = list;
	USList *prev = NULL;
	USList *current;

	while (next) {
		USList *tmp_prev = find_prev (next, data);
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
		u_slist_free_1 (current);
	}

	return list;
}

USList*
u_slist_remove_link (USList *list, USList *link)
{
	USList *prev = find_prev_link (list, link);
	USList *current = prev ? prev->next : list;

	if (current) {
		if (prev)
			prev->next = current->next;
		else
			list = current->next;
		current->next = NULL;
	}

	return list;
}

USList*
u_slist_delete_link (USList *list, USList *link)
{
	list = u_slist_remove_link (list, link);
	u_slist_free_1 (link);

	return list;
}

USList*
u_slist_reverse (USList *list)
{
	USList *prev = NULL;
	while (list){
		USList *next = list->next;
		list->next = prev;
		prev = list;
		list = next;
	}

	return prev;
}

USList*
u_slist_insert_sorted (USList *list, void * data, UCompareFunc func)
{
	USList *prev = NULL;
	
	if (!func)
		return list;

	if (!list || func (list->data, data) > 0)
		return u_slist_prepend (list, data);

	/* Invariant: func (prev->data, data) <= 0) */
	for (prev = list; prev->next; prev = prev->next)
		if (func (prev->next->data, data) > 0)
			break;

	/* ... && (prev->next == 0 || func (prev->next->data, data) > 0)) */
	insert_after (prev, data);
	return list;
}

int
u_slist_index (USList *list, const void * data)
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

USList*
u_slist_nth (USList *list, unsigned int n)
{
	for (; list; list = list->next) {
		if (n == 0)
			break;
		n--;
	}
	return list;
}

void *
u_slist_nth_data (USList *list, unsigned int n)
{
	USList *node = u_slist_nth (list, n);
	return node ? node->data : NULL;
}

typedef USList list_node;
#include "sort.frag.h"

USList*
u_slist_sort (USList *list, UCompareFunc func)
{
	if (!list || !list->next)
		return list;
	return do_sort (list, func);
}
