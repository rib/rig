#include <stdio.h>
#include <string.h>
#include <ulib.h>
#include "test.h"

RESULT
test_list_length ()
{
	UList *list = u_list_prepend (NULL, "foo");

	if (u_list_length (list) != 1)
		return FAILED ("length failed. #1");

	list = u_list_prepend (list, "bar");
	if (u_list_length (list) != 2)
		return FAILED ("length failed. #2");

	list = u_list_append (list, "bar");
	if (u_list_length (list) != 3)
		return FAILED ("length failed. #3");

	u_list_free (list);
	return NULL;
}

RESULT
test_list_nth ()
{
	char *foo = "foo";
	char *bar = "bar";
	char *baz = "baz";
	UList *nth, *list;
	list = u_list_prepend (NULL, baz);
	list = u_list_prepend (list, bar);
	list = u_list_prepend (list, foo);

	nth = u_list_nth (list, 0);
	if (nth->data != foo)
		return FAILED ("nth failed. #0");

	nth = u_list_nth (list, 1);
	if (nth->data != bar)
		return FAILED ("nth failed. #1");
	
	nth = u_list_nth (list, 2);
	if (nth->data != baz)
		return FAILED ("nth failed. #2");

	nth = u_list_nth (list, 3);
	if (nth)
		return FAILED ("nth failed. #3: %s", nth->data);

	u_list_free (list);
	return OK;
}

RESULT
test_list_index ()
{
	int i;
	char *foo = "foo";
	char *bar = "bar";
	char *baz = "baz";
	UList *list;
	list = u_list_prepend (NULL, baz);
	list = u_list_prepend (list, bar);
	list = u_list_prepend (list, foo);

	i = u_list_index (list, foo);
	if (i != 0)
		return FAILED ("index failed. #0: %d", i);

	i = u_list_index (list, bar);
	if (i != 1)
		return FAILED ("index failed. #1: %d", i);
	
	i = u_list_index (list, baz);
	if (i != 2)
		return FAILED ("index failed. #2: %d", i);

	u_list_free (list);
	return OK;
}

RESULT
test_list_append ()
{
	UList *list = u_list_prepend (NULL, "first");
	if (u_list_length (list) != 1)
		return FAILED ("Prepend failed");

	list = u_list_append (list, "second");

	if (u_list_length (list) != 2)
		return FAILED ("Append failed");

	u_list_free (list);
	return OK;
}

RESULT
test_list_last ()
{
	UList *foo = u_list_prepend (NULL, "foo");
	UList *bar = u_list_prepend (NULL, "bar");
	UList *last;
	
	foo = u_list_concat (foo, bar);
	last = u_list_last (foo);

	if (last != bar)
		return FAILED ("last failed. #1");

	foo = u_list_concat (foo, u_list_prepend (NULL, "baz"));
	foo = u_list_concat (foo, u_list_prepend (NULL, "quux"));

	last = u_list_last (foo);	
	if (strcmp ("quux", last->data))
		return FAILED ("last failed. #2");

	u_list_free (foo);

	return OK;
}

RESULT
test_list_concat ()
{
	UList *foo = u_list_prepend (NULL, "foo");
	UList *bar = u_list_prepend (NULL, "bar");
	UList *list = u_list_concat (foo, bar);

	if (u_list_length (list) != 2)
		return FAILED ("Concat failed. #1");

	if (strcmp (list->data, "foo"))
		return FAILED ("Concat failed. #2");

	if (strcmp (list->next->data, "bar"))
		return FAILED ("Concat failed. #3");

	if (u_list_first (list) != foo)
		return FAILED ("Concat failed. #4");
	
	if (u_list_last (list) != bar)
		return FAILED ("Concat failed. #5");

	u_list_free (list);

	return OK;
}


static int
compare (const void * a, const void * b)
{
	char *foo = (char *) a;
	char *bar = (char *) b;

	if (strlen (foo) < strlen (bar))
		return -1;

	return 1;
}

RESULT
test_list_insert_sorted ()
{
	UList *list = u_list_prepend (NULL, "a");
	list = u_list_append (list, "aaa");

	/* insert at the middle */
	list = u_list_insert_sorted (list, "aa", compare);
	if (strcmp ("aa", list->next->data))
		return FAILED ("insert_sorted failed. #1");

	/* insert at the beginning */
	list = u_list_insert_sorted (list, "", compare);
	if (strcmp ("", list->data))
		return FAILED ("insert_sorted failed. #2");		

	/* insert at the end */
	list = u_list_insert_sorted (list, "aaaa", compare);
	if (strcmp ("aaaa", u_list_last (list)->data))
		return FAILED ("insert_sorted failed. #3");

	u_list_free (list);
	return OK;
}

RESULT
test_list_copy ()
{
	int i, length;
	UList *list, *copy;
	list = u_list_prepend (NULL, "a");
	list = u_list_append  (list, "aa");
	list = u_list_append  (list, "aaa");
	list = u_list_append  (list, "aaaa");

	length = u_list_length (list);
	copy = u_list_copy (list);

	for (i = 0; i < length; i++)
		if (strcmp (u_list_nth (list, i)->data,
			    u_list_nth (copy, i)->data))
			return FAILED ("copy failed.");

	u_list_free (list);
	u_list_free (copy);	
	return OK;
}

RESULT
test_list_reverse ()
{
	unsigned int i, length;
	UList *list, *reverse;
	list = u_list_prepend (NULL, "a");
	list = u_list_append  (list, "aa");
	list = u_list_append  (list, "aaa");
	list = u_list_append  (list, "aaaa");

	length  = u_list_length (list);
	reverse = u_list_reverse (u_list_copy (list));

	if (u_list_length (reverse) != length)
		return FAILED ("reverse failed #1");

	for (i = 0; i < length; i++){
		unsigned int j = length - i - 1;
		if (strcmp (u_list_nth (list, i)->data,
			    u_list_nth (reverse, j)->data))
			return FAILED ("reverse failed. #2");
	}

	u_list_free (list);
	u_list_free (reverse);	
	return OK;
}

RESULT
test_list_remove ()
{
	UList *list = u_list_prepend (NULL, "three");
	char *one = "one";
	list = u_list_prepend (list, "two");
	list = u_list_prepend (list, one);

	list = u_list_remove (list, one);

	if (u_list_length (list) != 2)
		return FAILED ("Remove failed");

	if (strcmp ("two", list->data) != 0)
		return FAILED ("Remove failed");

	u_list_free (list);
	return OK;
}

RESULT
test_list_remove_link ()
{
	UList *foo = u_list_prepend (NULL, "a");
	UList *bar = u_list_prepend (NULL, "b");
	UList *baz = u_list_prepend (NULL, "c");
	UList *list = foo;

	foo = u_list_concat (foo, bar);
	foo = u_list_concat (foo, baz);	

	list = u_list_remove_link (list, bar);

	if (u_list_length (list) != 2)
		return FAILED ("remove_link failed #1");

	if (bar->next != NULL)
		return FAILED ("remove_link failed #2");

	u_list_free (list);	
	u_list_free (bar);
	return OK;
}

RESULT
test_list_insert_before ()
{
	UList *foo, *bar, *baz;

	foo = u_list_prepend (NULL, "foo");
	foo = u_list_insert_before (foo, NULL, "bar");
	bar = u_list_last (foo);

	if (strcmp (bar->data, "bar"))
		return FAILED ("1");

	baz = u_list_insert_before (foo, bar, "baz");
	if (foo != baz)
		return FAILED ("2");

	if (strcmp (u_list_nth_data (foo, 1), "baz"))
		return FAILED ("3: %s", u_list_nth_data (foo, 1));	

	u_list_free (foo);
	return OK;
}

#define N_ELEMS 101

static int intcompare (const void * p1, const void * p2)
{
	return U_POINTER_TO_INT (p1) - U_POINTER_TO_INT (p2);
}

static uboolean verify_sort (UList *list, int len)
{
	int prev;

	if (list->prev)
		return FALSE;

	prev = U_POINTER_TO_INT (list->data);
	len--;
	for (list = list->next; list; list = list->next) {
		int curr = U_POINTER_TO_INT (list->data);
		if (prev > curr)
			return FALSE;
		prev = curr;

		if (!list->prev || list->prev->next != list)
			return FALSE;

		if (len == 0)
			return FALSE;
		len--;
	}
	return len == 0;
}

RESULT
test_list_sort ()
{
	int i, j, mul;
	UList *list = NULL;

	for (i = 0; i < N_ELEMS; ++i)
		list = u_list_prepend (list, U_INT_TO_POINTER (i));
	list = u_list_sort (list, intcompare);
	if (!verify_sort (list, N_ELEMS))
		return FAILED ("decreasing list");

	u_list_free (list);

	list = NULL;
	for (i = 0; i < N_ELEMS; ++i)
		list = u_list_prepend (list, U_INT_TO_POINTER (-i));
	list = u_list_sort (list, intcompare);
	if (!verify_sort (list, N_ELEMS))
		return FAILED ("increasing list");

	u_list_free (list);

	list = u_list_prepend (NULL, U_INT_TO_POINTER (0));
	for (i = 1; i < N_ELEMS; ++i) {
		list = u_list_prepend (list, U_INT_TO_POINTER (i));
		list = u_list_prepend (list, U_INT_TO_POINTER (-i));
	}
	list = u_list_sort (list, intcompare);
	if (!verify_sort (list, 2*N_ELEMS-1))
		return FAILED ("alternating list");

	u_list_free (list);

	list = NULL;
	mul = 1;
	for (i = 1; i < N_ELEMS; ++i) {
		mul = -mul;
		for (j = 0; j < i; ++j)
			list = u_list_prepend (list, U_INT_TO_POINTER (mul * j));
	}
	list = u_list_sort (list, intcompare);
	if (!verify_sort (list, (N_ELEMS*N_ELEMS - N_ELEMS)/2))
		return FAILED ("wavering list");

	u_list_free (list);

	return OK;
}

static int
find_custom (const void * a, const void * b)
{
	return(strcmp (a, b));
}

RESULT
test_list_find_custom ()
{
	UList *list = NULL, *found;
	char *foo = "foo";
	char *bar = "bar";
	char *baz = "baz";
	
	list = u_list_prepend (list, baz);
	list = u_list_prepend (list, bar);
	list = u_list_prepend (list, foo);
	
	found = u_list_find_custom (list, baz, find_custom);
	
	if (found == NULL)
		return FAILED ("Find failed");
	
	u_list_free (list);
	
	return OK;
}

static Test list_tests [] = {
	{       "length", test_list_length},
	{          "nth", test_list_nth},
	{        "index", test_list_index},	
	{         "last", test_list_last},	
	{       "append", test_list_append},
	{       "concat", test_list_concat},
	{"insert_sorted", test_list_insert_sorted},
	{"insert_before", test_list_insert_before},
	{         "copy", test_list_copy},
	{      "reverse", test_list_reverse},
	{       "remove", test_list_remove},
	{  "remove_link", test_list_remove_link},
	{  "remove_link", test_list_remove_link},
	{         "sort", test_list_sort},
	{  "find_custom", test_list_find_custom},
	{NULL, NULL}
};

DEFINE_TEST_GROUP_INIT(list_tests_init, list_tests)
