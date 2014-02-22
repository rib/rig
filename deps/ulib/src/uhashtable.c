/*
 * ghashtable.c: Hashtable implementation
 *
 * Author:
 *   Miguel de Icaza (miguel@novell.com)
 *
 * (C) 2006 Novell, Inc.
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
 */

#include <config.h>

#include <stdio.h>
#include <math.h>
#include <ulib.h>

typedef struct _Slot Slot;

struct _Slot {
	void * key;
	void * value;
	Slot    *next;
};

static void * KEYMARKER_REMOVED = &KEYMARKER_REMOVED;

struct _UHashTable {
	UHashFunc      hash_func;
	UEqualFunc     key_equal_func;

	Slot **table;
	int   table_size;
	int   in_use;
	int   threshold;
	int   last_rehash;
	UDestroyNotify value_destroy_func, key_destroy_func;
};

typedef struct {
	UHashTable *ht;
	int slot_index;
	Slot *slot;
} Iter;

static const unsigned int prime_tbl[] = {
	11, 19, 37, 73, 109, 163, 251, 367, 557, 823, 1237,
	1861, 2777, 4177, 6247, 9371, 14057, 21089, 31627,
	47431, 71143, 106721, 160073, 240101, 360163,
	540217, 810343, 1215497, 1823231, 2734867, 4102283,
	6153409, 9230113, 13845163
};

static uboolean
test_prime (int x)
{
	if ((x & 1) != 0) {
		int n;
		for (n = 3; n< (int)sqrt (x); n += 2) {
			if ((x % n) == 0)
				return FALSE;
		}
		return TRUE;
	}
	// There is only one even prime - 2.
	return (x == 2);
}

static int
calc_prime (int x)
{
	int i;

	for (i = (x & (~1))-1; i< U_MAXINT32; i += 2) {
		if (test_prime (i))
			return i;
	}
	return x;
}

unsigned int
u_spaced_primes_closest (unsigned int x)
{
	int i;

	for (i = 0; i < U_N_ELEMENTS (prime_tbl); i++) {
		if (x <= prime_tbl [i])
			return prime_tbl [i];
	}
	return calc_prime (x);
}

UHashTable *
u_hash_table_new (UHashFunc hash_func, UEqualFunc key_equal_func)
{
	UHashTable *hash;

	if (hash_func == NULL)
		hash_func = u_direct_hash;
	if (key_equal_func == NULL)
		key_equal_func = u_direct_equal;
	hash = u_new0 (UHashTable, 1);

	hash->hash_func = hash_func;
	hash->key_equal_func = key_equal_func;

	hash->table_size = u_spaced_primes_closest (1);
	hash->table = u_new0 (Slot *, hash->table_size);
	hash->last_rehash = hash->table_size;

	return hash;
}

UHashTable *
u_hash_table_new_full (UHashFunc hash_func, UEqualFunc key_equal_func,
		       UDestroyNotify key_destroy_func, UDestroyNotify value_destroy_func)
{
	UHashTable *hash = u_hash_table_new (hash_func, key_equal_func);
	if (hash == NULL)
		return NULL;

	hash->key_destroy_func = key_destroy_func;
	hash->value_destroy_func = value_destroy_func;

	return hash;
}

#if 0
static void
dump_hash_table (UHashTable *hash)
{
	int i;

	for (i = 0; i < hash->table_size; i++) {
		Slot *s;

		for (s = hash->table [i]; s != NULL; s = s->next){
			unsigned int hashcode = (*hash->hash_func) (s->key);
			unsigned int slot = (hashcode) % hash->table_size;
			printf ("key %p hash %x on slot %d correct slot %d tb size %d\n", s->key, hashcode, i, slot, hash->table_size);
		}
	}
}
#endif

#ifdef SANITY_CHECK
static void
sanity_check (UHashTable *hash)
{
	int i;

	for (i = 0; i < hash->table_size; i++) {
		Slot *s;

		for (s = hash->table [i]; s != NULL; s = s->next){
			unsigned int hashcode = (*hash->hash_func) (s->key);
			unsigned int slot = (hashcode) % hash->table_size;
			if (slot != i) {
				dump_hashcode_func = 1;
				hashcode = (*hash->hash_func) (s->key);
				dump_hashcode_func = 0;
				u_error ("Key %p (bucket %d) on invalid bucket %d (hashcode %x) (tb size %d)", s->key, slot, i, hashcode, hash->table_size);
			}
		}
	}
}
#else

#define sanity_check(HASH) do {}while(0)

#endif

static void
do_rehash (UHashTable *hash)
{
	int current_size, i;
	Slot **table;

	/* printf ("Resizing diff=%d slots=%d\n", hash->in_use - hash->last_rehash, hash->table_size); */
	hash->last_rehash = hash->table_size;
	current_size = hash->table_size;
	hash->table_size = u_spaced_primes_closest (hash->in_use);
	/* printf ("New size: %d\n", hash->table_size); */
	table = hash->table;
	hash->table = u_new0 (Slot *, hash->table_size);

	for (i = 0; i < current_size; i++){
		Slot *s, *next;

		for (s = table [i]; s != NULL; s = next){
			unsigned int hashcode = ((*hash->hash_func) (s->key)) % hash->table_size;
			next = s->next;

			s->next = hash->table [hashcode];
			hash->table [hashcode] = s;
		}
	}
	u_free (table);
}

static void
rehash (UHashTable *hash)
{
	int diff = ABS (hash->last_rehash - hash->in_use);

	/* These are the factors to play with to change the rehashing strategy */
	/* I played with them with a large range, and could not really get */
	/* something that was too good, maybe the tests are not that great */
	if (!(diff * 0.75 > hash->table_size * 2))
		return;
	do_rehash (hash);
	sanity_check (hash);
}

void
u_hash_table_insert_replace (UHashTable *hash, void * key, void * value, uboolean replace)
{
	unsigned int hashcode;
	Slot *s;
	UEqualFunc equal;

	u_return_if_fail (hash != NULL);
	sanity_check (hash);

	equal = hash->key_equal_func;
	if (hash->in_use >= hash->threshold)
		rehash (hash);

	hashcode = ((*hash->hash_func) (key)) % hash->table_size;
	for (s = hash->table [hashcode]; s != NULL; s = s->next){
		if ((*equal) (s->key, key)){
			if (replace){
				if (hash->key_destroy_func != NULL)
					(*hash->key_destroy_func)(s->key);
				s->key = key;
			}
			if (hash->value_destroy_func != NULL)
				(*hash->value_destroy_func) (s->value);
			s->value = value;
			sanity_check (hash);
			return;
		}
	}
	s = u_new (Slot, 1);
	s->key = key;
	s->value = value;
	s->next = hash->table [hashcode];
	hash->table [hashcode] = s;
	hash->in_use++;
	sanity_check (hash);
}

UList*
u_hash_table_get_keys (UHashTable *hash)
{
	UHashTableIter iter;
	UList *rv = NULL;
	void * key;

	u_hash_table_iter_init (&iter, hash);

	while (u_hash_table_iter_next (&iter, &key, NULL))
		rv = u_list_prepend (rv, key);

	return u_list_reverse (rv);
}

UList*
u_hash_table_get_values (UHashTable *hash)
{
	UHashTableIter iter;
	UList *rv = NULL;
	void * value;

	u_hash_table_iter_init (&iter, hash);

	while (u_hash_table_iter_next (&iter, NULL, &value))
		rv = u_list_prepend (rv, value);

	return u_list_reverse (rv);
}


unsigned int
u_hash_table_size (UHashTable *hash)
{
	u_return_val_if_fail (hash != NULL, 0);

	return hash->in_use;
}

void *
u_hash_table_lookup (UHashTable *hash, const void * key)
{
	void *orig_key, *value;

	if (u_hash_table_lookup_extended (hash, key, &orig_key, &value))
		return value;
	else
		return NULL;
}

uboolean
u_hash_table_lookup_extended (UHashTable *hash, const void * key, void * *orig_key, void * *value)
{
	UEqualFunc equal;
	Slot *s;
	unsigned int hashcode;

	u_return_val_if_fail (hash != NULL, FALSE);
	sanity_check (hash);
	equal = hash->key_equal_func;

	hashcode = ((*hash->hash_func) (key)) % hash->table_size;

	for (s = hash->table [hashcode]; s != NULL; s = s->next){
		if ((*equal)(s->key, key)){
			if (orig_key)
				*orig_key = s->key;
			if (value)
				*value = s->value;
			return TRUE;
		}
	}
	return FALSE;
}

void
u_hash_table_foreach (UHashTable *hash, UHFunc func, void * user_data)
{
	int i;

	u_return_if_fail (hash != NULL);
	u_return_if_fail (func != NULL);

	for (i = 0; i < hash->table_size; i++){
		Slot *s;

		for (s = hash->table [i]; s != NULL; s = s->next)
			(*func)(s->key, s->value, user_data);
	}
}

void *
u_hash_table_find (UHashTable *hash, UHRFunc predicate, void * user_data)
{
	int i;

	u_return_val_if_fail (hash != NULL, NULL);
	u_return_val_if_fail (predicate != NULL, NULL);

	for (i = 0; i < hash->table_size; i++){
		Slot *s;

		for (s = hash->table [i]; s != NULL; s = s->next)
			if ((*predicate)(s->key, s->value, user_data))
				return s->value;
	}
	return NULL;
}

void
u_hash_table_remove_all (UHashTable *hash)
{
	int i;

	u_return_if_fail (hash != NULL);

	for (i = 0; i < hash->table_size; i++){
		Slot *s;

		while (hash->table [i]) {
			s = hash->table [i];
			u_hash_table_remove (hash, s->key);
		}
	}
}

uboolean
u_hash_table_remove (UHashTable *hash, const void * key)
{
	UEqualFunc equal;
	Slot *s, *last;
	unsigned int hashcode;

	u_return_val_if_fail (hash != NULL, FALSE);
	sanity_check (hash);
	equal = hash->key_equal_func;

	hashcode = ((*hash->hash_func)(key)) % hash->table_size;
	last = NULL;
	for (s = hash->table [hashcode]; s != NULL; s = s->next){
		if ((*equal)(s->key, key)){
			if (hash->key_destroy_func != NULL)
				(*hash->key_destroy_func)(s->key);
			if (hash->value_destroy_func != NULL)
				(*hash->value_destroy_func)(s->value);
			if (last == NULL)
				hash->table [hashcode] = s->next;
			else
				last->next = s->next;
			u_free (s);
			hash->in_use--;
			sanity_check (hash);
			return TRUE;
		}
		last = s;
	}
	sanity_check (hash);
	return FALSE;
}

unsigned int
u_hash_table_foreach_remove (UHashTable *hash, UHRFunc func, void * user_data)
{
	int i;
	int count = 0;

	u_return_val_if_fail (hash != NULL, 0);
	u_return_val_if_fail (func != NULL, 0);

	sanity_check (hash);
	for (i = 0; i < hash->table_size; i++){
		Slot *s, *last;

		last = NULL;
		for (s = hash->table [i]; s != NULL; ){
			if ((*func)(s->key, s->value, user_data)){
				Slot *n;

				if (hash->key_destroy_func != NULL)
					(*hash->key_destroy_func)(s->key);
				if (hash->value_destroy_func != NULL)
					(*hash->value_destroy_func)(s->value);
				if (last == NULL){
					hash->table [i] = s->next;
					n = s->next;
				} else  {
					last->next = s->next;
					n = last->next;
				}
				u_free (s);
				hash->in_use--;
				count++;
				s = n;
			} else {
				last = s;
				s = s->next;
			}
		}
	}
	sanity_check (hash);
	if (count > 0)
		rehash (hash);
	return count;
}

uboolean
u_hash_table_steal (UHashTable *hash, const void * key)
{
	UEqualFunc equal;
	Slot *s, *last;
	unsigned int hashcode;

	u_return_val_if_fail (hash != NULL, FALSE);
	sanity_check (hash);
	equal = hash->key_equal_func;

	hashcode = ((*hash->hash_func)(key)) % hash->table_size;
	last = NULL;
	for (s = hash->table [hashcode]; s != NULL; s = s->next){
		if ((*equal)(s->key, key)) {
			if (last == NULL)
				hash->table [hashcode] = s->next;
			else
				last->next = s->next;
			u_free (s);
			hash->in_use--;
			sanity_check (hash);
			return TRUE;
		}
		last = s;
	}
	sanity_check (hash);
	return FALSE;

}

unsigned int
u_hash_table_foreach_steal (UHashTable *hash, UHRFunc func, void * user_data)
{
	int i;
	int count = 0;

	u_return_val_if_fail (hash != NULL, 0);
	u_return_val_if_fail (func != NULL, 0);

	sanity_check (hash);
	for (i = 0; i < hash->table_size; i++){
		Slot *s, *last;

		last = NULL;
		for (s = hash->table [i]; s != NULL; ){
			if ((*func)(s->key, s->value, user_data)){
				Slot *n;

				if (last == NULL){
					hash->table [i] = s->next;
					n = s->next;
				} else  {
					last->next = s->next;
					n = last->next;
				}
				u_free (s);
				hash->in_use--;
				count++;
				s = n;
			} else {
				last = s;
				s = s->next;
			}
		}
	}
	sanity_check (hash);
	if (count > 0)
		rehash (hash);
	return count;
}

void
u_hash_table_destroy (UHashTable *hash)
{
	int i;

	u_return_if_fail (hash != NULL);

	for (i = 0; i < hash->table_size; i++){
		Slot *s, *next;

		for (s = hash->table [i]; s != NULL; s = next){
			next = s->next;

			if (hash->key_destroy_func != NULL)
				(*hash->key_destroy_func)(s->key);
			if (hash->value_destroy_func != NULL)
				(*hash->value_destroy_func)(s->value);
			u_free (s);
		}
	}
	u_free (hash->table);

	u_free (hash);
}

void
u_hash_table_print_stats (UHashTable *table)
{
	int i, max_chain_index, chain_size, max_chain_size;
	Slot *node;

	max_chain_size = 0;
	max_chain_index = -1;
	for (i = 0; i < table->table_size; i++) {
		chain_size = 0;
		for (node = table->table [i]; node; node = node->next)
			chain_size ++;
		if (chain_size > max_chain_size) {
			max_chain_size = chain_size;
			max_chain_index = i;
		}
	}

	printf ("Size: %d Table Size: %d Max Chain Length: %d at %d\n", table->in_use, table->table_size, max_chain_size, max_chain_index);
}

void
u_hash_table_iter_init (UHashTableIter *it, UHashTable *hash_table)
{
	Iter *iter = (Iter*)it;

	memset (iter, 0, sizeof (Iter));
	iter->ht = hash_table;
	iter->slot_index = -1;
}

uboolean u_hash_table_iter_next (UHashTableIter *it, void * *key, void * *value)
{
	Iter *iter = (Iter*)it;

	UHashTable *hash = iter->ht;

	u_assert (iter->slot_index != -2);
	u_assert (sizeof (Iter) <= sizeof (UHashTableIter));

	if (!iter->slot) {
		while (TRUE) {
			iter->slot_index ++;
			if (iter->slot_index >= hash->table_size) {
				iter->slot_index = -2;
				return FALSE;
			}
			if (hash->table [iter->slot_index])
				break;
		}
		iter->slot = hash->table [iter->slot_index];
	}

	if (key)
		*key = iter->slot->key;
	if (value)
		*value = iter->slot->value;
	iter->slot = iter->slot->next;

	return TRUE;
}

uboolean
u_direct_equal (const void * v1, const void * v2)
{
	return v1 == v2;
}

unsigned int
u_direct_hash (const void * v1)
{
	return U_POINTER_TO_UINT (v1);
}

uboolean
u_int_equal (const void * v1, const void * v2)
{
	return *(int *)v1 == *(int *)v2;
}

unsigned int
u_int_hash (const void * v1)
{
	return *(unsigned int *)v1;
}

uboolean
u_str_equal (const void * v1, const void * v2)
{
	return strcmp (v1, v2) == 0;
}

unsigned int
u_str_hash (const void * v1)
{
	unsigned int hash = 0;
	char *p = (char *) v1;

	while (*p++)
		hash = (hash << 5) - (hash + *p);

	return hash;
}
