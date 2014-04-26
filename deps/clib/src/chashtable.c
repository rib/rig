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
#include <clib.h>

typedef struct _Slot Slot;

struct _Slot {
	void * key;
	void * value;
	Slot    *next;
};

static void * KEYMARKER_REMOVED = &KEYMARKER_REMOVED;

struct _CHashTable {
	CHashFunc      hash_func;
	CEqualFunc     key_equal_func;

	Slot **table;
	int   table_size;
	int   in_use;
	int   threshold;
	int   last_rehash;
	CDestroyNotify value_destroy_func, key_destroy_func;
};

typedef struct {
	CHashTable *ht;
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

static cboolean
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

	for (i = (x & (~1))-1; i< C_MAXINT32; i += 2) {
		if (test_prime (i))
			return i;
	}
	return x;
}

unsigned int
c_spaced_primes_closest (unsigned int x)
{
	int i;

	for (i = 0; i < C_N_ELEMENTS (prime_tbl); i++) {
		if (x <= prime_tbl [i])
			return prime_tbl [i];
	}
	return calc_prime (x);
}

CHashTable *
c_hash_table_new (CHashFunc hash_func, CEqualFunc key_equal_func)
{
	CHashTable *hash;

	if (hash_func == NULL)
		hash_func = c_direct_hash;
	if (key_equal_func == NULL)
		key_equal_func = c_direct_equal;
	hash = c_new0 (CHashTable, 1);

	hash->hash_func = hash_func;
	hash->key_equal_func = key_equal_func;

	hash->table_size = c_spaced_primes_closest (1);
	hash->table = c_new0 (Slot *, hash->table_size);
	hash->last_rehash = hash->table_size;

	return hash;
}

CHashTable *
c_hash_table_new_full (CHashFunc hash_func, CEqualFunc key_equal_func,
		       CDestroyNotify key_destroy_func, CDestroyNotify value_destroy_func)
{
	CHashTable *hash = c_hash_table_new (hash_func, key_equal_func);
	if (hash == NULL)
		return NULL;

	hash->key_destroy_func = key_destroy_func;
	hash->value_destroy_func = value_destroy_func;

	return hash;
}

#if 0
static void
dump_hash_table (CHashTable *hash)
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
sanity_check (CHashTable *hash)
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
				c_error ("Key %p (bucket %d) on invalid bucket %d (hashcode %x) (tb size %d)", s->key, slot, i, hashcode, hash->table_size);
			}
		}
	}
}
#else

#define sanity_check(HASH) do {}while(0)

#endif

static void
do_rehash (CHashTable *hash)
{
	int current_size, i;
	Slot **table;

	/* printf ("Resizing diff=%d slots=%d\n", hash->in_use - hash->last_rehash, hash->table_size); */
	hash->last_rehash = hash->table_size;
	current_size = hash->table_size;
	hash->table_size = c_spaced_primes_closest (hash->in_use);
	/* printf ("New size: %d\n", hash->table_size); */
	table = hash->table;
	hash->table = c_new0 (Slot *, hash->table_size);

	for (i = 0; i < current_size; i++){
		Slot *s, *next;

		for (s = table [i]; s != NULL; s = next){
			unsigned int hashcode = ((*hash->hash_func) (s->key)) % hash->table_size;
			next = s->next;

			s->next = hash->table [hashcode];
			hash->table [hashcode] = s;
		}
	}
	c_free (table);
}

static void
rehash (CHashTable *hash)
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
c_hash_table_insert_replace (CHashTable *hash, void * key, void * value, cboolean replace)
{
	unsigned int hashcode;
	Slot *s;
	CEqualFunc equal;

	c_return_if_fail (hash != NULL);
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
	s = c_new (Slot, 1);
	s->key = key;
	s->value = value;
	s->next = hash->table [hashcode];
	hash->table [hashcode] = s;
	hash->in_use++;
	sanity_check (hash);
}

CList*
c_hash_table_get_keys (CHashTable *hash)
{
	CHashTableIter iter;
	CList *rv = NULL;
	void * key;

	c_hash_table_iter_init (&iter, hash);

	while (c_hash_table_iter_next (&iter, &key, NULL))
		rv = c_list_prepend (rv, key);

	return c_list_reverse (rv);
}

CList*
c_hash_table_get_values (CHashTable *hash)
{
	CHashTableIter iter;
	CList *rv = NULL;
	void * value;

	c_hash_table_iter_init (&iter, hash);

	while (c_hash_table_iter_next (&iter, NULL, &value))
		rv = c_list_prepend (rv, value);

	return c_list_reverse (rv);
}


unsigned int
c_hash_table_size (CHashTable *hash)
{
	c_return_val_if_fail (hash != NULL, 0);

	return hash->in_use;
}

void *
c_hash_table_lookup (CHashTable *hash, const void * key)
{
	void *orig_key, *value;

	if (c_hash_table_lookup_extended (hash, key, &orig_key, &value))
		return value;
	else
		return NULL;
}

cboolean
c_hash_table_lookup_extended (CHashTable *hash, const void * key, void * *orig_key, void * *value)
{
	CEqualFunc equal;
	Slot *s;
	unsigned int hashcode;

	c_return_val_if_fail (hash != NULL, FALSE);
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
c_hash_table_foreach (CHashTable *hash, CHFunc func, void * user_data)
{
	int i;

	c_return_if_fail (hash != NULL);
	c_return_if_fail (func != NULL);

	for (i = 0; i < hash->table_size; i++){
		Slot *s;

		for (s = hash->table [i]; s != NULL; s = s->next)
			(*func)(s->key, s->value, user_data);
	}
}

void *
c_hash_table_find (CHashTable *hash, CHRFunc predicate, void * user_data)
{
	int i;

	c_return_val_if_fail (hash != NULL, NULL);
	c_return_val_if_fail (predicate != NULL, NULL);

	for (i = 0; i < hash->table_size; i++){
		Slot *s;

		for (s = hash->table [i]; s != NULL; s = s->next)
			if ((*predicate)(s->key, s->value, user_data))
				return s->value;
	}
	return NULL;
}

void
c_hash_table_remove_all (CHashTable *hash)
{
	int i;

	c_return_if_fail (hash != NULL);

	for (i = 0; i < hash->table_size; i++){
		Slot *s;

		while (hash->table [i]) {
			s = hash->table [i];
			c_hash_table_remove (hash, s->key);
		}
	}
}

cboolean
c_hash_table_remove (CHashTable *hash, const void * key)
{
	CEqualFunc equal;
	Slot *s, *last;
	unsigned int hashcode;

	c_return_val_if_fail (hash != NULL, FALSE);
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
			c_free (s);
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
c_hash_table_foreach_remove (CHashTable *hash, CHRFunc func, void * user_data)
{
	int i;
	int count = 0;

	c_return_val_if_fail (hash != NULL, 0);
	c_return_val_if_fail (func != NULL, 0);

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
				c_free (s);
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

cboolean
c_hash_table_steal (CHashTable *hash, const void * key)
{
	CEqualFunc equal;
	Slot *s, *last;
	unsigned int hashcode;

	c_return_val_if_fail (hash != NULL, FALSE);
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
			c_free (s);
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
c_hash_table_foreach_steal (CHashTable *hash, CHRFunc func, void * user_data)
{
	int i;
	int count = 0;

	c_return_val_if_fail (hash != NULL, 0);
	c_return_val_if_fail (func != NULL, 0);

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
				c_free (s);
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
c_hash_table_destroy (CHashTable *hash)
{
	int i;

	c_return_if_fail (hash != NULL);

	for (i = 0; i < hash->table_size; i++){
		Slot *s, *next;

		for (s = hash->table [i]; s != NULL; s = next){
			next = s->next;

			if (hash->key_destroy_func != NULL)
				(*hash->key_destroy_func)(s->key);
			if (hash->value_destroy_func != NULL)
				(*hash->value_destroy_func)(s->value);
			c_free (s);
		}
	}
	c_free (hash->table);

	c_free (hash);
}

void
c_hash_table_print_stats (CHashTable *table)
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
c_hash_table_iter_init (CHashTableIter *it, CHashTable *hash_table)
{
	Iter *iter = (Iter*)it;

	memset (iter, 0, sizeof (Iter));
	iter->ht = hash_table;
	iter->slot_index = -1;
}

cboolean c_hash_table_iter_next (CHashTableIter *it, void * *key, void * *value)
{
	Iter *iter = (Iter*)it;

	CHashTable *hash = iter->ht;

	c_assert (iter->slot_index != -2);
	c_assert (sizeof (Iter) <= sizeof (CHashTableIter));

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

cboolean
c_direct_equal (const void * v1, const void * v2)
{
	return v1 == v2;
}

unsigned int
c_direct_hash (const void * v1)
{
	return C_POINTER_TO_UINT (v1);
}

cboolean
c_int_equal (const void * v1, const void * v2)
{
	return *(int *)v1 == *(int *)v2;
}

unsigned int
c_int_hash (const void * v1)
{
	return *(unsigned int *)v1;
}

cboolean
c_str_equal (const void * v1, const void * v2)
{
	return strcmp (v1, v2) == 0;
}

unsigned int
c_str_hash (const void * v1)
{
	unsigned int hash = 0;
	char *p = (char *) v1;

	while (*p++)
		hash = (hash << 5) - (hash + *p);

	return hash;
}
