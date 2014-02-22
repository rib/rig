#include <stdio.h>
#include <ulib.h>
#include "test.h"

/* Redefine the private structure only to verify proper allocations */
typedef struct _UPtrArrayPriv {
	void * *pdata;
	unsigned int len;
	unsigned int size;
} UPtrArrayPriv;

/* Don't add more than 32 items to this please */
static const char *items [] = {
	"Apples", "Oranges", "Plumbs", "Goats", "Snorps", "Grapes", 
	"Tickle", "Place", "Coffee", "Cookies", "Cake", "Cheese",
	"Tseng", "Holiday", "Avenue", "Smashing", "Water", "Toilet",
	NULL
};

static UPtrArray *ptrarray_alloc_and_fill(unsigned int *item_count)
{
	UPtrArray *array = u_ptr_array_new();
	int i;
	
	for(i = 0; items[i] != NULL; i++) {
		u_ptr_array_add(array, (void *)items[i]);
	}

	if(item_count != NULL) {
		*item_count = i;
	}
	
	return array;
}

static unsigned int guess_size(unsigned int length)
{
	unsigned int size = 1;

	while(size < length) {
		size <<= 1;
	}

	return size;
}

RESULT ptrarray_alloc()
{
	UPtrArrayPriv *array;
	unsigned int i;
	
	array = (UPtrArrayPriv *)ptrarray_alloc_and_fill(&i);
	
	if(array->size != guess_size(array->len)) {
		return FAILED("Size should be %d, but it is %d", 
			guess_size(array->len), array->size);
	}
	
	if(array->len != i) {
		return FAILED("Expected %d node(s) in the array", i);
	}
	
	u_ptr_array_free((UPtrArray *)array, TRUE);

	return OK;
}

RESULT ptrarray_for_iterate()
{
	UPtrArray *array = ptrarray_alloc_and_fill(NULL);
	unsigned int i;

	for(i = 0; i < array->len; i++) {
		char *item = (char *)u_ptr_array_index(array, i);
		if(item != items[i]) {
			return FAILED(
				"Expected item at %d to be %s, but it was %s", 
				i, items[i], item);
		}
	}

	u_ptr_array_free(array, TRUE);

	return OK;
}

static int foreach_iterate_index = 0;
static char *foreach_iterate_error = NULL;

void foreach_callback(void * data, void * user_data)
{
	char *item = (char *)data;
	const char *item_cmp = items[foreach_iterate_index++];

	if(foreach_iterate_error != NULL) {
		return;
	}

	if(item != item_cmp) {
		foreach_iterate_error = FAILED(
			"Expected item at %d to be %s, but it was %s", 
				foreach_iterate_index - 1, item_cmp, item);
	}
}

RESULT ptrarray_foreach_iterate()
{
	UPtrArray *array = ptrarray_alloc_and_fill(NULL);
	
	foreach_iterate_index = 0;
	foreach_iterate_error = NULL;
	
	u_ptr_array_foreach(array, foreach_callback, array);
	
	u_ptr_array_free(array, TRUE);

	return foreach_iterate_error;
}

RESULT ptrarray_set_size()
{
	UPtrArray *array = u_ptr_array_new();
	unsigned int i, grow_length = 50;
	
	u_ptr_array_add(array, (void *)items[0]);
	u_ptr_array_add(array, (void *)items[1]);
	u_ptr_array_set_size(array, grow_length);

	if(array->len != grow_length) {
		return FAILED("Array length should be 50, it is %d", array->len);
	} else if(array->pdata[0] != items[0]) {
		return FAILED("Item 0 was overwritten, should be %s", items[0]);
	} else if(array->pdata[1] != items[1]) {
		return FAILED("Item 1 was overwritten, should be %s", items[1]);
	}

	for(i = 2; i < array->len; i++) {
		if(array->pdata[i] != NULL) {
			return FAILED("Item %d is not NULL, it is %p", i, array->pdata[i]);
		}
	}

	u_ptr_array_free(array, TRUE);

	return OK;
}

RESULT ptrarray_remove_index()
{
	UPtrArray *array;
	unsigned int i;
	
	array = ptrarray_alloc_and_fill(&i);
	
	u_ptr_array_remove_index(array, 0);
	if(array->pdata[0] != items[1]) {
		return FAILED("First item is not %s, it is %s", items[1],
			array->pdata[0]);
	}

	u_ptr_array_remove_index(array, array->len - 1);
	
	if(array->pdata[array->len - 1] != items[array->len]) {
		return FAILED("Last item is not %s, it is %s", 
			items[array->len - 2], array->pdata[array->len - 1]);
	}

	u_ptr_array_free(array, TRUE);

	return OK;
}

RESULT ptrarray_remove_index_fast()
{
	UPtrArray *array;
	unsigned int i;

	array = ptrarray_alloc_and_fill(&i);

	u_ptr_array_remove_index_fast(array, 0);
	if(array->pdata[0] != items[array->len]) {
		return FAILED("First item is not %s, it is %s", items[array->len],
			array->pdata[0]);
	}

	u_ptr_array_remove_index_fast(array, array->len - 1);
	if(array->pdata[array->len - 1] != items[array->len - 1]) {
		return FAILED("Last item is not %s, it is %s",
			items[array->len - 1], array->pdata[array->len - 1]);
	}

	u_ptr_array_free(array, TRUE);

	return OK;
}

RESULT ptrarray_remove()
{
	UPtrArray *array;
	unsigned int i;
	
	array = ptrarray_alloc_and_fill(&i);

	u_ptr_array_remove(array, (void *)items[7]);

	if(!u_ptr_array_remove(array, (void *)items[4])) {
		return FAILED("Item %s not removed", items[4]);
	}

	if(u_ptr_array_remove(array, (void *)items[4])) {
		return FAILED("Item %s still in array after removal", items[4]);
	}

	if(array->pdata[array->len - 1] != items[array->len + 1]) {
		return FAILED("Last item in UPtrArray not correct");
	}

	u_ptr_array_free(array, TRUE);

	return OK;
}

static int ptrarray_sort_compare(const void * a, const void * b)
{
	char *stra = *(char **) a;
	char *strb = *(char **) b;
	return strcmp(stra, strb);
}

RESULT ptrarray_sort()
{
	UPtrArray *array = u_ptr_array_new();
	unsigned int i;
	char *letters [] = { "A", "B", "C", "D", "E" };
	
	u_ptr_array_add(array, letters[0]);
	u_ptr_array_add(array, letters[1]);
	u_ptr_array_add(array, letters[2]);
	u_ptr_array_add(array, letters[3]);
	u_ptr_array_add(array, letters[4]);
	
	u_ptr_array_sort(array, ptrarray_sort_compare);

	for(i = 0; i < array->len; i++) {
		if(array->pdata[i] != letters[i]) {
			return FAILED("Array out of order, expected %s got %s at position %d",
				letters [i], (char *) array->pdata [i], i);
		}
	}

	u_ptr_array_free(array, TRUE);
	
	return OK;
}

static int ptrarray_sort_compare_with_data (const void * a, const void * b, void * user_data)
{
	char *stra = *(char **) a;
	char *strb = *(char **) b;

	if (strcmp (user_data, "this is the data for qsort") != 0)
		fprintf (stderr, "oops at compare with_data\n");

	return strcmp(stra, strb);
}

RESULT ptrarray_sort_with_data ()
{
	UPtrArray *array = u_ptr_array_new();
	unsigned int i;
	char *letters [] = { "A", "B", "C", "D", "E" };

	u_ptr_array_add(array, letters[4]);
	u_ptr_array_add(array, letters[1]);
	u_ptr_array_add(array, letters[2]);
	u_ptr_array_add(array, letters[0]);
	u_ptr_array_add(array, letters[3]);

	u_ptr_array_sort_with_data(array, ptrarray_sort_compare_with_data, "this is the data for qsort");

	for(i = 0; i < array->len; i++) {
		if(array->pdata[i] != letters[i]) {
			return FAILED("Array out of order, expected %s got %s at position %d",
				letters [i], (char *) array->pdata [i], i);
		}
	}

	u_ptr_array_free(array, TRUE);

	return OK;
}

RESULT ptrarray_remove_fast()
{
	UPtrArray *array = u_ptr_array_new();
	char *letters [] = { "A", "B", "C", "D", "E" };
	
	if (u_ptr_array_remove_fast (array, NULL))
		return FAILED ("Removing NULL succeeded");

	u_ptr_array_add(array, letters[0]);
	if (!u_ptr_array_remove_fast (array, letters[0]) || array->len != 0)
		return FAILED ("Removing last element failed");

	u_ptr_array_add(array, letters[0]);
	u_ptr_array_add(array, letters[1]);
	u_ptr_array_add(array, letters[2]);
	u_ptr_array_add(array, letters[3]);
	u_ptr_array_add(array, letters[4]);

	if (!u_ptr_array_remove_fast (array, letters[0]) || array->len != 4)
		return FAILED ("Removing first element failed");

	if (array->pdata [0] != letters [4])
		return FAILED ("First element wasn't replaced with last upon removal");

	if (u_ptr_array_remove_fast (array, letters[0]))
		return FAILED ("Succedeed removing a non-existing element");

	if (!u_ptr_array_remove_fast (array, letters[3]) || array->len != 3)
		return FAILED ("Failed removing \"D\"");

	if (!u_ptr_array_remove_fast (array, letters[1]) || array->len != 2)
		return FAILED ("Failed removing \"B\"");

	if (array->pdata [0] != letters [4] || array->pdata [1] != letters [2])
		return FAILED ("Last two elements are wrong");
	u_ptr_array_free(array, TRUE);
	
	return OK;
}

static Test ptrarray_tests [] = {
	{"alloc", ptrarray_alloc},
	{"for_iterate", ptrarray_for_iterate},
	{"foreach_iterate", ptrarray_foreach_iterate},
	{"set_size", ptrarray_set_size},
	{"remove_index", ptrarray_remove_index},
	{"remove_index_fast", ptrarray_remove_index_fast},
	{"remove", ptrarray_remove},
	{"sort", ptrarray_sort},
	{"remove_fast", ptrarray_remove_fast},
	{"sort_with_data", ptrarray_sort_with_data},
	{NULL, NULL}
};

DEFINE_TEST_GROUP_INIT(ptrarray_tests_init, ptrarray_tests)


