#include <stdio.h>
#include <string.h>
#include <ulib.h>
#include "test.h"


RESULT
test_slist_nth ()
{
	char *foo = "foo";
	char *bar = "bar";
	char *baz = "baz";
	USList *nth, *list;
	list = u_slist_prepend (NULL, baz);
	list = u_slist_prepend (list, bar);
	list = u_slist_prepend (list, foo);

	nth = u_slist_nth (list, 0);
	if (nth->data != foo)
		return FAILED ("nth failed. #0");

	nth = u_slist_nth (list, 1);
	if (nth->data != bar)
		return FAILED ("nth failed. #1");
	
	nth = u_slist_nth (list, 2);
	if (nth->data != baz)
		return FAILED ("nth failed. #2");

	nth = u_slist_nth (list, 3);
	if (nth)
		return FAILED ("nth failed. #3: %s", nth->data);

	u_slist_free (list);
	return OK;
}

RESULT
test_slist_index ()
{
	int i;
	char *foo = "foo";
	char *bar = "bar";
	char *baz = "baz";
	USList *list;
	list = u_slist_prepend (NULL, baz);
	list = u_slist_prepend (list, bar);
	list = u_slist_prepend (list, foo);

	i = u_slist_index (list, foo);
	if (i != 0)
		return FAILED ("index failed. #0: %d", i);

	i = u_slist_index (list, bar);
	if (i != 1)
		return FAILED ("index failed. #1: %d", i);
	
	i = u_slist_index (list, baz);
	if (i != 2)
		return FAILED ("index failed. #2: %d", i);

	u_slist_free (list);
	return OK;
}

RESULT
test_slist_append ()
{
	USList *foo;
	USList *list = u_slist_append (NULL, "first");
	if (u_slist_length (list) != 1)
		return FAILED ("append(null,...) failed");

	foo = u_slist_append (list, "second");
	if (foo != list)
		return FAILED ("changed list head on non-empty");

	if (u_slist_length (list) != 2)
		return FAILED ("Append failed");

	u_slist_free (list);
	return OK;
}

RESULT
test_slist_concat ()
{
	USList *foo = u_slist_prepend (NULL, "foo");
	USList *bar = u_slist_prepend (NULL, "bar");

	USList *list = u_slist_concat (foo, bar);

	if (u_slist_length (list) != 2)
		return FAILED ("Concat failed.");

	u_slist_free (list);
	return OK;
}

RESULT
test_slist_find ()
{
	USList *list = u_slist_prepend (NULL, "three");
	USList *found;
	char *data;
		
	list = u_slist_prepend (list, "two");
	list = u_slist_prepend (list, "one");

	data = "four";
	list = u_slist_append (list, data);

	found = u_slist_find (list, data);

	if (found->data != data)
		return FAILED ("Find failed");

	u_slist_free (list);
	return OK;
}

static int
find_custom (const void * a, const void * b)
{
	return(strcmp (a, b));
}

RESULT
test_slist_find_custom ()
{
	USList *list = NULL, *found;
	char *foo = "foo";
	char *bar = "bar";
	char *baz = "baz";
	
	list = u_slist_prepend (list, baz);
	list = u_slist_prepend (list, bar);
	list = u_slist_prepend (list, foo);
	
	found = u_slist_find_custom (list, baz, find_custom);
	
	if (found == NULL)
		return FAILED ("Find failed");
	
	u_slist_free (list);
	
	return OK;
}

RESULT
test_slist_remove ()
{
	USList *list = u_slist_prepend (NULL, "three");
	char *one = "one";
	list = u_slist_prepend (list, "two");
	list = u_slist_prepend (list, one);

	list = u_slist_remove (list, one);

	if (u_slist_length (list) != 2)
		return FAILED ("Remove failed");

	if (strcmp ("two", list->data) != 0)
		return FAILED ("Remove failed");

	u_slist_free (list);
	return OK;
}

RESULT
test_slist_remove_link ()
{
	USList *foo = u_slist_prepend (NULL, "a");
	USList *bar = u_slist_prepend (NULL, "b");
	USList *baz = u_slist_prepend (NULL, "c");
	USList *list = foo;

	foo = u_slist_concat (foo, bar);
	foo = u_slist_concat (foo, baz);	

	list = u_slist_remove_link (list, bar);

	if (u_slist_length (list) != 2)
		return FAILED ("remove_link failed #1");

	if (bar->next != NULL)
		return FAILED ("remove_link failed #2");

	u_slist_free (list);	
	u_slist_free (bar);

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
test_slist_insert_sorted ()
{
	USList *list = u_slist_prepend (NULL, "a");
	list = u_slist_append (list, "aaa");

	/* insert at the middle */
	list = u_slist_insert_sorted (list, "aa", compare);
	if (strcmp ("aa", list->next->data))
		return FAILED("insert_sorted failed #1");

	/* insert at the beginning */
	list = u_slist_insert_sorted (list, "", compare);
	if (strcmp ("", list->data))
		return FAILED ("insert_sorted failed #2");

	/* insert at the end */
	list = u_slist_insert_sorted (list, "aaaa", compare);
	if (strcmp ("aaaa", u_slist_last (list)->data))
		return FAILED ("insert_sorted failed #3");

	u_slist_free (list);	
	return OK;
}

RESULT
test_slist_insert_before ()
{
	USList *foo, *bar, *baz;

	foo = u_slist_prepend (NULL, "foo");
	foo = u_slist_insert_before (foo, NULL, "bar");
	bar = u_slist_last (foo);

	if (strcmp (bar->data, "bar"))
		return FAILED ("1");

	baz = u_slist_insert_before (foo, bar, "baz");
	if (foo != baz)
		return FAILED ("2");

	if (strcmp (foo->next->data, "baz"))
		return FAILED ("3: %s", foo->next->data);

	u_slist_free (foo);
	return OK;
}

#define N_ELEMS 100

static int intcompare (const void * p1, const void * p2)
{
	return U_POINTER_TO_INT (p1) - U_POINTER_TO_INT (p2);
}

static uboolean verify_sort (USList *list, int len)
{
	int prev = U_POINTER_TO_INT (list->data);
	len--;
	for (list = list->next; list; list = list->next) {
		int curr = U_POINTER_TO_INT (list->data);
		if (prev > curr)
			return FALSE;
		prev = curr;

		if (len == 0)
			return FALSE;
		len--;
	}
	return len == 0;
}

RESULT
test_slist_sort ()
{
	int i, j, mul;
	USList *list = NULL;

	for (i = 0; i < N_ELEMS; ++i)
		list = u_slist_prepend (list, U_INT_TO_POINTER (i));
	list = u_slist_sort (list, intcompare);
	if (!verify_sort (list, N_ELEMS))
		return FAILED ("decreasing list");

	u_slist_free (list);

	list = NULL;
	for (i = 0; i < N_ELEMS; ++i)
		list = u_slist_prepend (list, U_INT_TO_POINTER (-i));
	list = u_slist_sort (list, intcompare);
	if (!verify_sort (list, N_ELEMS))
		return FAILED ("increasing list");

	u_slist_free (list);

	list = u_slist_prepend (NULL, U_INT_TO_POINTER (0));
	for (i = 1; i < N_ELEMS; ++i) {
		list = u_slist_prepend (list, U_INT_TO_POINTER (-i));
		list = u_slist_prepend (list, U_INT_TO_POINTER (i));
	}
	list = u_slist_sort (list, intcompare);
	if (!verify_sort (list, 2*N_ELEMS-1))
		return FAILED ("alternating list");

	u_slist_free (list);

	list = NULL;
	mul = 1;
	for (i = 1; i < N_ELEMS; ++i) {
		mul = -mul;
		for (j = 0; j < i; ++j)
			list = u_slist_prepend (list, U_INT_TO_POINTER (mul * j));
	}
	list = u_slist_sort (list, intcompare);
	if (!verify_sort (list, (N_ELEMS*N_ELEMS - N_ELEMS)/2))
		return FAILED ("wavering list");

	u_slist_free (list);

	return OK;
}

static Test slist_tests [] = {
	{"nth", test_slist_nth},
	{"index", test_slist_index},
	{"append", test_slist_append},
	{"concat", test_slist_concat},
	{"find", test_slist_find},
	{"find_custom", test_slist_find_custom},
	{"remove", test_slist_remove},
	{"remove_link", test_slist_remove_link},
	{"insert_sorted", test_slist_insert_sorted},
	{"insert_before", test_slist_insert_before},
	{"sort", test_slist_sort},
	{NULL, NULL}
};

DEFINE_TEST_GROUP_INIT(slist_tests_init, slist_tests)

