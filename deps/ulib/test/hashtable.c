#include <stdio.h>
#include <string.h>
#include <ulib.h>
#include "test.h"

int foreach_count = 0;
int foreach_fail = 0;

void foreach (void * key, void * value, void * user_data)
{
	foreach_count++;
	if (U_POINTER_TO_INT (user_data) != 'a')
		foreach_fail = 1;
}

RESULT hash_t1 (void)
{
	UHashTable *t = u_hash_table_new (u_str_hash, u_str_equal);

	foreach_count = 0;
	foreach_fail = 0;
	u_hash_table_insert (t, "hello", "world");
	u_hash_table_insert (t, "my", "god");

	u_hash_table_foreach (t, foreach, U_INT_TO_POINTER('a'));
	if (foreach_count != 2)
		return FAILED ("did not find all keys, got %d expected 2", foreach_count);
	if (foreach_fail)
		return FAILED("failed to pass the user-data to foreach");

	if (!u_hash_table_remove (t, "my"))
		return FAILED ("did not find known key");
	if (u_hash_table_size (t) != 1)
		return FAILED ("unexpected size");
	u_hash_table_insert(t, "hello", "moon");
	if (strcmp (u_hash_table_lookup (t, "hello"), "moon") != 0)
		return FAILED ("did not replace world with moon");

	if (!u_hash_table_remove (t, "hello"))
		return FAILED ("did not find known key");
	if (u_hash_table_size (t) != 0)
		return FAILED ("unexpected size");
	u_hash_table_destroy (t);

	return OK;
}

RESULT hash_t2 (void)
{
	return OK;
}

RESULT hash_default (void)
{
	UHashTable *hash = u_hash_table_new (NULL, NULL);

	if (hash == NULL)
		return FAILED ("u_hash_table_new should return a valid hash");

	u_hash_table_destroy (hash);
	return NULL;
}

RESULT
hash_null_lookup (void)
{
	UHashTable *hash = u_hash_table_new (NULL, NULL);
	void *ok, *ov;

	u_hash_table_insert (hash, NULL, U_INT_TO_POINTER (1));
	u_hash_table_insert (hash, U_INT_TO_POINTER(1), U_INT_TO_POINTER(2));

	if (!u_hash_table_lookup_extended (hash, NULL, &ok, &ov))
		return FAILED ("Did not find the NULL");
	if (ok != NULL)
		return FAILED ("Incorrect key found");
	if (ov != U_INT_TO_POINTER (1))
		return FAILED ("Got wrong value %p\n", ov);

	if (!u_hash_table_lookup_extended (hash, U_INT_TO_POINTER(1), &ok, &ov))
		return FAILED ("Did not find the 1");
	if (ok != U_INT_TO_POINTER(1))
		return FAILED ("Incorrect key found");
	if (ov != U_INT_TO_POINTER (2))
		return FAILED ("Got wrong value %p\n", ov);

	u_hash_table_destroy (hash);

	return NULL;
}

static void
counter (void * key, void * value, void * user_data)
{
	int *counter = (int *) user_data;

	(*counter)++;
}

RESULT hash_grow (void)
{
	UHashTable *hash = u_hash_table_new_full (u_str_hash, u_str_equal, u_free, u_free);
	int i, count = 0;

	for (i = 0; i < 1000; i++)
		u_hash_table_insert (hash, u_strdup_printf ("%d", i), u_strdup_printf ("x-%d", i));

	for (i = 0; i < 1000; i++){
		char buffer [30];
		void * value;

		sprintf (buffer, "%d", i);

		value = u_hash_table_lookup (hash, buffer);
		sprintf (buffer, "x-%d", i);
		if (strcmp (value, buffer) != 0){
			return FAILED ("Failed to lookup the key %d, the value was %s\n", i, value);
		}
	}

	if (u_hash_table_size (hash) != 1000)
		return FAILED ("Did not find 1000 elements on the hash, found %d\n", u_hash_table_size (hash));

	/* Now do the manual count, lets not trust the internals */
	u_hash_table_foreach (hash, counter, &count);
	if (count != 1000){
		return FAILED ("Foreach count is not 1000");
	}

	u_hash_table_destroy (hash);
	return NULL;
}

RESULT hash_iter (void)
{
#if !defined(ULIB_MAJOR_VERSION) || ULIB_CHECK_VERSION(2, 16, 0)
	UHashTable *hash = u_hash_table_new_full (u_direct_hash, u_direct_equal, NULL, NULL);
	UHashTableIter iter;
	int i, sum, keys_sum, values_sum;
	void *key, *value;

	sum = 0;
	for (i = 0; i < 1000; i++) {
		sum += i;
		u_hash_table_insert (hash, U_UINT_TO_POINTER (i), U_UINT_TO_POINTER (i));
	}

	keys_sum = values_sum = 0;
	u_hash_table_iter_init (&iter, hash);
	while (u_hash_table_iter_next (&iter, &key, &value)) {
		if (key != value)
			return FAILED ("key != value");
		keys_sum += U_POINTER_TO_UINT (key);
		values_sum += U_POINTER_TO_UINT (value);
	}
	if (keys_sum != sum || values_sum != sum)
		return FAILED ("Did not find all key-value pairs");
	u_hash_table_destroy (hash);
	return NULL;
#else
	/* UHashTableIter was added in glib 2.16 */
	return NULL;
#endif
}

static Test hashtable_tests [] = {
	{"t1", hash_t1},
	{"t2", hash_t2},
	{"grow", hash_grow},
	{"default", hash_default},
	{"null_lookup", hash_null_lookup},
	{"iter", hash_iter},
	{NULL, NULL}
};

DEFINE_TEST_GROUP_INIT(hashtable_tests_init, hashtable_tests)

