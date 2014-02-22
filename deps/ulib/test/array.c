#include <stdio.h>
#include <string.h>
#include <ulib.h>
#include "test.h"

/* example from glib documentation */
RESULT
test_array_big ()
{
	UArray *garray;
	int i;

	/* We create a new array to store int values.
	   We don't want it zero-terminated or cleared to 0's. */
	garray = u_array_new (FALSE, FALSE, sizeof (int));
	for (i = 0; i < 10000; i++)
		u_array_append_val (garray, i);

	for (i = 0; i < 10000; i++)
		if (u_array_index (garray, int, i) != i)
			return FAILED ("array value didn't match");

	u_array_free (garray, TRUE);

	return NULL;
}

RESULT
test_array_index ()
{
	UArray *array = u_array_new (FALSE, FALSE, sizeof (int));
	int v;

	v = 27;
	u_array_append_val (array, v);

	if (27 != u_array_index (array, int, 0))
		return FAILED ("");

	u_array_free (array, TRUE);

	return NULL;
}

RESULT
test_array_append_zero_terminated ()
{
	UArray *array = u_array_new (TRUE, FALSE, sizeof (int));
	int v;

	v = 27;
	u_array_append_val (array, v);

	if (27 != u_array_index (array, int, 0))
		return FAILED ("u_array_append_val failed");

	if (0 != u_array_index (array, int, 1))
		return FAILED ("zero_terminated didn't append a zero element");

	u_array_free (array, TRUE);

	return NULL;
}

RESULT
test_array_append ()
{
	UArray *array = u_array_new (FALSE, FALSE, sizeof (int));
	int v;

	if (0 != array->len)
		return FAILED ("initial array length not zero");

	v = 27;

	u_array_append_val (array, v);

	if (1 != array->len)
		return FAILED ("array append failed");

	u_array_free (array, TRUE);

	return NULL;
}

RESULT
test_array_insert_val ()
{
	UArray *array = u_array_new (FALSE, FALSE, sizeof (void *));
	void *ptr0, *ptr1, *ptr2, *ptr3;

	u_array_insert_val (array, 0, array);

	if (array != u_array_index (array, void *, 0))
		return FAILED ("1 The value in the array is incorrect");

	u_array_insert_val (array, 1, array);
	if (array != u_array_index (array, void *, 1))
		return FAILED ("2 The value in the array is incorrect");

	u_array_insert_val (array, 2, array);
	if (array != u_array_index (array, void *, 2))
		return FAILED ("3 The value in the array is incorrect");

	u_array_free (array, TRUE);
	array = u_array_new (FALSE, FALSE, sizeof (void *));
	ptr0 = array;
	ptr1 = array + 1;
	ptr2 = array + 2;
	ptr3 = array + 3;

	u_array_insert_val (array, 0, ptr0);
	u_array_insert_val (array, 1, ptr1);
	u_array_insert_val (array, 2, ptr2);
	u_array_insert_val (array, 1, ptr3);
	if (ptr0 != u_array_index (array, void *, 0))
		return FAILED ("4 The value in the array is incorrect");
	if (ptr3 != u_array_index (array, void *, 1))
		return FAILED ("5 The value in the array is incorrect");
	if (ptr1 != u_array_index (array, void *, 2))
		return FAILED ("6 The value in the array is incorrect");
	if (ptr2 != u_array_index (array, void *, 3))
		return FAILED ("7 The value in the array is incorrect");

	u_array_free (array, TRUE);
	return NULL;
}

RESULT
test_array_remove ()
{
	UArray *array = u_array_new (FALSE, FALSE, sizeof (int));
	int v[] = {30, 29, 28, 27, 26, 25};

	u_array_append_vals (array, v, 6);

	if (6 != array->len)
		return FAILED ("append_vals fail");

	u_array_remove_index (array, 3);

	if (5 != array->len)
		return FAILED ("remove_index failed to update length");

	if (26 != u_array_index (array, int, 3))
		return FAILED ("remove_index failed to update the array");

	u_array_free (array, TRUE);

	return NULL;
}

static Test array_tests [] = {
	{"big", test_array_big},
	{"append", test_array_append},
	{"insert_val", test_array_insert_val},
	{"index", test_array_index},
	{"remove", test_array_remove},
	{"append_zero_term", test_array_append_zero_terminated},
	{NULL, NULL}
};

DEFINE_TEST_GROUP_INIT(array_tests_init, array_tests)
