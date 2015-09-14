/*
 * Hashtable implementation
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

#include <clib-config.h>

#include <stdio.h>
#include <math.h>
#include <clib.h>

typedef struct _slot_t slot_t;

struct _slot_t {
    void *key;
    void *value;
    slot_t *next;
};

static void *KEYMARKER_REMOVED = &KEYMARKER_REMOVED;

struct _c_hash_table_t {
    c_hash_func_t hash_func;
    c_equal_func_t key_equal_func;

    slot_t **table;
    int table_size;
    int in_use;
    int threshold;
    int last_rehash;
    c_destroy_func_t value_destroy_func, key_destroy_func;
};

typedef struct {
    c_hash_table_t *ht;
    int slot_index;
    slot_t *slot;
} Iter;

static const unsigned int prime_tbl[] = {
    11,      19,      37,      73,      109,     163,     251,
    367,     557,     823,     1237,    1861,    2777,    4177,
    6247,    9371,    14057,   21089,   31627,   47431,   71143,
    106721,  160073,  240101,  360163,  540217,  810343,  1215497,
    1823231, 2734867, 4102283, 6153409, 9230113, 13845163
};

static bool
test_prime(int x)
{
    if ((x & 1) != 0) {
        int n;
        for (n = 3; n < (int)sqrt(x); n += 2) {
            if ((x % n) == 0)
                return false;
        }
        return true;
    }
    // There is only one even prime - 2.
    return (x == 2);
}

static int
calc_prime(int x)
{
    int i;

    for (i = (x & (~1)) - 1; i < C_MAXINT32; i += 2) {
        if (test_prime(i))
            return i;
    }
    return x;
}

unsigned int
c_spaced_primes_closest(unsigned int x)
{
    int i;

    for (i = 0; i < C_N_ELEMENTS(prime_tbl); i++) {
        if (x <= prime_tbl[i])
            return prime_tbl[i];
    }
    return calc_prime(x);
}

c_hash_table_t *
c_hash_table_new(c_hash_func_t hash_func,
                 c_equal_func_t key_equal_func)
{
    c_hash_table_t *hash;

    if (hash_func == NULL)
        hash_func = c_direct_hash;
    if (key_equal_func == NULL)
        key_equal_func = c_direct_equal;
    hash = c_new0(c_hash_table_t, 1);

    hash->hash_func = hash_func;
    hash->key_equal_func = key_equal_func;

    hash->table_size = c_spaced_primes_closest(1);
    hash->table = c_new0(slot_t *, hash->table_size);
    hash->last_rehash = hash->table_size;

    return hash;
}

c_hash_table_t *
c_hash_table_new_full(c_hash_func_t hash_func,
                      c_equal_func_t key_equal_func,
                      c_destroy_func_t key_destroy_func,
                      c_destroy_func_t value_destroy_func)
{
    c_hash_table_t *hash = c_hash_table_new(hash_func, key_equal_func);
    if (hash == NULL)
        return NULL;

    hash->key_destroy_func = key_destroy_func;
    hash->value_destroy_func = value_destroy_func;

    return hash;
}

#if 0
static void
dump_hash_table (c_hash_table_t *hash)
{
    int i;

    for (i = 0; i < hash->table_size; i++) {
        slot_t *s;

        for (s = hash->table [i]; s != NULL; s = s->next) {
            unsigned int hashcode = (*hash->hash_func)(s->key);
            unsigned int slot = (hashcode) % hash->table_size;
            printf ("key %p hash %x on slot %d correct slot %d tb size %d\n", s->key, hashcode, i, slot, hash->table_size);
        }
    }
}
#endif

#ifdef SANITY_CHECK
static void
sanity_check(c_hash_table_t *hash)
{
    int i;

    for (i = 0; i < hash->table_size; i++) {
        slot_t *s;

        for (s = hash->table[i]; s != NULL; s = s->next) {
            unsigned int hashcode = (*hash->hash_func)(s->key);
            unsigned int slot = (hashcode) % hash->table_size;
            if (slot != i) {
                dump_hashcode_func = 1;
                hashcode = (*hash->hash_func)(s->key);
                dump_hashcode_func = 0;
                c_error("Key %p (bucket %d) on invalid bucket %d (hashcode %x) "
                        "(tb size %d)",
                        s->key,
                        slot,
                        i,
                        hashcode,
                        hash->table_size);
            }
        }
    }
}
#else

#define sanity_check(HASH)                                                     \
    do {                                                                       \
    } while (0)

#endif

static void
do_rehash(c_hash_table_t *hash)
{
    int current_size, i;
    slot_t **table;

    /* printf ("Resizing diff=%d slots=%d\n", hash->in_use - hash->last_rehash,
     * hash->table_size); */
    hash->last_rehash = hash->table_size;
    current_size = hash->table_size;
    hash->table_size = c_spaced_primes_closest(hash->in_use);
    /* printf ("New size: %d\n", hash->table_size); */
    table = hash->table;
    hash->table = c_new0(slot_t *, hash->table_size);

    for (i = 0; i < current_size; i++) {
        slot_t *s, *next;

        for (s = table[i]; s != NULL; s = next) {
            unsigned int hashcode =
                ((*hash->hash_func)(s->key)) % hash->table_size;
            next = s->next;

            s->next = hash->table[hashcode];
            hash->table[hashcode] = s;
        }
    }
    c_free(table);
}

static void
rehash(c_hash_table_t *hash)
{
    int diff = ABS(hash->last_rehash - hash->in_use);

    /* These are the factors to play with to change the rehashing strategy */
    /* I played with them with a large range, and could not really get */
    /* something that was too good, maybe the tests are not that great */
    if (!(diff * 0.75 > hash->table_size * 2))
        return;
    do_rehash(hash);
    sanity_check(hash);
}

void
c_hash_table_insert_replace(c_hash_table_t *hash,
                            void *key,
                            void *value,
                            bool replace)
{
    unsigned int hashcode;
    slot_t *s;
    c_equal_func_t equal;

    c_return_if_fail(hash != NULL);
    sanity_check(hash);

    equal = hash->key_equal_func;
    if (hash->in_use >= hash->threshold)
        rehash(hash);

    hashcode = ((*hash->hash_func)(key)) % hash->table_size;
    for (s = hash->table[hashcode]; s != NULL; s = s->next) {
        if ((*equal)(s->key, key)) {
            if (replace) {
                if (hash->key_destroy_func != NULL)
                    (*hash->key_destroy_func)(s->key);
                s->key = key;
            }
            if (hash->value_destroy_func != NULL)
                (*hash->value_destroy_func)(s->value);
            s->value = value;
            sanity_check(hash);
            return;
        }
    }
    s = c_new(slot_t, 1);
    s->key = key;
    s->value = value;
    s->next = hash->table[hashcode];
    hash->table[hashcode] = s;
    hash->in_use++;
    sanity_check(hash);
}

c_llist_t *
c_hash_table_get_keys(c_hash_table_t *hash)
{
    c_hash_table_iter_t iter;
    c_llist_t *rv = NULL;
    void *key;

    c_hash_table_iter_init(&iter, hash);

    while (c_hash_table_iter_next(&iter, &key, NULL))
        rv = c_llist_prepend(rv, key);

    return c_llist_reverse(rv);
}

c_llist_t *
c_hash_table_get_values(c_hash_table_t *hash)
{
    c_hash_table_iter_t iter;
    c_llist_t *rv = NULL;
    void *value;

    c_hash_table_iter_init(&iter, hash);

    while (c_hash_table_iter_next(&iter, NULL, &value))
        rv = c_llist_prepend(rv, value);

    return c_llist_reverse(rv);
}

unsigned int
c_hash_table_size(c_hash_table_t *hash)
{
    c_return_val_if_fail(hash != NULL, 0);

    return hash->in_use;
}

void *
c_hash_table_lookup(c_hash_table_t *hash, const void *key)
{
    void *orig_key, *value;

    if (c_hash_table_lookup_extended(hash, key, &orig_key, &value))
        return value;
    else
        return NULL;
}

bool
c_hash_table_lookup_extended(c_hash_table_t *hash,
                             const void *key,
                             void **orig_key,
                             void **value)
{
    c_equal_func_t equal;
    slot_t *s;
    unsigned int hashcode;

    c_return_val_if_fail(hash != NULL, false);
    sanity_check(hash);
    equal = hash->key_equal_func;

    hashcode = ((*hash->hash_func)(key)) % hash->table_size;

    for (s = hash->table[hashcode]; s != NULL; s = s->next) {
        if ((*equal)(s->key, key)) {
            if (orig_key)
                *orig_key = s->key;
            if (value)
                *value = s->value;
            return true;
        }
    }
    return false;
}

bool
c_hash_table_contains(c_hash_table_t *hash, const void *key)
{
    return c_hash_table_lookup_extended(hash, key, NULL, NULL);
}

void
c_hash_table_foreach(c_hash_table_t *hash,
                     c_hash_iter_func_t func,
                     void *user_data)
{
    int i;

    c_return_if_fail(hash != NULL);
    c_return_if_fail(func != NULL);

    for (i = 0; i < hash->table_size; i++) {
        slot_t *s;

        for (s = hash->table[i]; s != NULL; s = s->next)
            (*func)(s->key, s->value, user_data);
    }
}

void *
c_hash_table_find(c_hash_table_t *hash,
                  c_hash_iter_remove_func_t predicate,
                  void *user_data)
{
    int i;

    c_return_val_if_fail(hash != NULL, NULL);
    c_return_val_if_fail(predicate != NULL, NULL);

    for (i = 0; i < hash->table_size; i++) {
        slot_t *s;

        for (s = hash->table[i]; s != NULL; s = s->next)
            if ((*predicate)(s->key, s->value, user_data))
                return s->value;
    }
    return NULL;
}

void
c_hash_table_remove_all(c_hash_table_t *hash)
{
    int i;

    c_return_if_fail(hash != NULL);

    for (i = 0; i < hash->table_size; i++) {
        slot_t *s;

        while (hash->table[i]) {
            s = hash->table[i];
            c_hash_table_remove(hash, s->key);
        }
    }
}

static bool
_c_hash_table_remove_value(c_hash_table_t *hash,
                           const void *key,
                           void **value)
{
    c_equal_func_t equal;
    slot_t *s, *last;
    unsigned int hashcode;

    c_return_val_if_fail(hash != NULL, false);
    sanity_check(hash);
    equal = hash->key_equal_func;

    hashcode = ((*hash->hash_func)(key)) % hash->table_size;
    last = NULL;
    for (s = hash->table[hashcode]; s != NULL; s = s->next) {
        if ((*equal)(s->key, key)) {
            *value = s->value;

            if (hash->key_destroy_func != NULL)
                (*hash->key_destroy_func)(s->key);
            if (hash->value_destroy_func != NULL)
                (*hash->value_destroy_func)(s->value);

            if (last == NULL)
                hash->table[hashcode] = s->next;
            else
                last->next = s->next;
            c_free(s);
            hash->in_use--;
            sanity_check(hash);
            return true;
        }
        last = s;
    }
    sanity_check(hash);
    return false;
}

bool
c_hash_table_remove(c_hash_table_t *hash, const void *key)
{
    void *value;

    return _c_hash_table_remove_value(hash, key, &value);
}

void *
c_hash_table_remove_value(c_hash_table_t *hash, const void *key)
{
    void *value = NULL;

    _c_hash_table_remove_value(hash, key, &value);

    return value;
}

unsigned int
c_hash_table_foreach_remove(c_hash_table_t *hash,
                            c_hash_iter_remove_func_t func,
                            void *user_data)
{
    int i;
    int count = 0;

    c_return_val_if_fail(hash != NULL, 0);
    c_return_val_if_fail(func != NULL, 0);

    sanity_check(hash);
    for (i = 0; i < hash->table_size; i++) {
        slot_t *s, *last;

        last = NULL;
        for (s = hash->table[i]; s != NULL; ) {
            if ((*func)(s->key, s->value, user_data)) {
                slot_t *n;

                if (hash->key_destroy_func != NULL)
                    (*hash->key_destroy_func)(s->key);
                if (hash->value_destroy_func != NULL)
                    (*hash->value_destroy_func)(s->value);
                if (last == NULL) {
                    hash->table[i] = s->next;
                    n = s->next;
                } else {
                    last->next = s->next;
                    n = last->next;
                }
                c_free(s);
                hash->in_use--;
                count++;
                s = n;
            } else {
                last = s;
                s = s->next;
            }
        }
    }
    sanity_check(hash);
    if (count > 0)
        rehash(hash);
    return count;
}

bool
c_hash_table_steal(c_hash_table_t *hash, const void *key)
{
    c_equal_func_t equal;
    slot_t *s, *last;
    unsigned int hashcode;

    c_return_val_if_fail(hash != NULL, false);
    sanity_check(hash);
    equal = hash->key_equal_func;

    hashcode = ((*hash->hash_func)(key)) % hash->table_size;
    last = NULL;
    for (s = hash->table[hashcode]; s != NULL; s = s->next) {
        if ((*equal)(s->key, key)) {
            if (last == NULL)
                hash->table[hashcode] = s->next;
            else
                last->next = s->next;
            c_free(s);
            hash->in_use--;
            sanity_check(hash);
            return true;
        }
        last = s;
    }
    sanity_check(hash);
    return false;
}

unsigned int
c_hash_table_foreach_steal(c_hash_table_t *hash,
                           c_hash_iter_remove_func_t func,
                           void *user_data)
{
    int i;
    int count = 0;

    c_return_val_if_fail(hash != NULL, 0);
    c_return_val_if_fail(func != NULL, 0);

    sanity_check(hash);
    for (i = 0; i < hash->table_size; i++) {
        slot_t *s, *last;

        last = NULL;
        for (s = hash->table[i]; s != NULL; ) {
            if ((*func)(s->key, s->value, user_data)) {
                slot_t *n;

                if (last == NULL) {
                    hash->table[i] = s->next;
                    n = s->next;
                } else {
                    last->next = s->next;
                    n = last->next;
                }
                c_free(s);
                hash->in_use--;
                count++;
                s = n;
            } else {
                last = s;
                s = s->next;
            }
        }
    }
    sanity_check(hash);
    if (count > 0)
        rehash(hash);
    return count;
}

void
c_hash_table_destroy(c_hash_table_t *hash)
{
    int i;

    c_return_if_fail(hash != NULL);

    for (i = 0; i < hash->table_size; i++) {
        slot_t *s, *next;

        for (s = hash->table[i]; s != NULL; s = next) {
            next = s->next;

            if (hash->key_destroy_func != NULL)
                (*hash->key_destroy_func)(s->key);
            if (hash->value_destroy_func != NULL)
                (*hash->value_destroy_func)(s->value);
            c_free(s);
        }
    }
    c_free(hash->table);

    c_free(hash);
}

void
c_hash_table_print_stats(c_hash_table_t *table)
{
    int i, max_chain_index, chain_size, max_chain_size;
    slot_t *node;

    max_chain_size = 0;
    max_chain_index = -1;
    for (i = 0; i < table->table_size; i++) {
        chain_size = 0;
        for (node = table->table[i]; node; node = node->next)
            chain_size++;
        if (chain_size > max_chain_size) {
            max_chain_size = chain_size;
            max_chain_index = i;
        }
    }

    printf("Size: %d Table Size: %d Max Chain Length: %d at %d\n",
           table->in_use,
           table->table_size,
           max_chain_size,
           max_chain_index);
}

static void
print_key_value_cb (void *key, void *value, void *user_data)
{
    printf("key = %p, value = %p\n", key, value);
}

void
c_hash_table_print(c_hash_table_t *table)
{
    c_hash_table_foreach(table, print_key_value_cb, NULL);
}

void
c_hash_table_iter_init(c_hash_table_iter_t *it, c_hash_table_t *hash_table)
{
    Iter *iter = (Iter *)it;

    memset(iter, 0, sizeof(Iter));
    iter->ht = hash_table;
    iter->slot_index = -1;
}

bool
c_hash_table_iter_next(c_hash_table_iter_t *it, void **key, void **value)
{
    Iter *iter = (Iter *)it;

    c_hash_table_t *hash = iter->ht;

    c_assert(iter->slot_index != -2);
    c_assert(sizeof(Iter) <= sizeof(c_hash_table_iter_t));

    if (!iter->slot) {
        while (true) {
            iter->slot_index++;
            if (iter->slot_index >= hash->table_size) {
                iter->slot_index = -2;
                return false;
            }
            if (hash->table[iter->slot_index])
                break;
        }
        iter->slot = hash->table[iter->slot_index];
    }

    if (key)
        *key = iter->slot->key;
    if (value)
        *value = iter->slot->value;
    iter->slot = iter->slot->next;

    return true;
}

bool
c_direct_equal(const void *v1, const void *v2)
{
    return v1 == v2;
}

unsigned int
c_direct_hash(const void *v1)
{
    return C_POINTER_TO_UINT(v1);
}

bool
c_int_equal(const void *v1, const void *v2)
{
    return *(int *)v1 == *(int *)v2;
}

unsigned int
c_int_hash(const void *v1)
{
    return *(unsigned int *)v1;
}

bool
c_int64_equal(const void *v1, const void *v2)
{
    return *(int64_t *)v1 == *(int64_t *)v2;
}

unsigned int
c_int64_hash(const void *v1)
{
    return (unsigned int)*(int64_t *)v1;
}

bool
c_str_equal(const void *v1, const void *v2)
{
    return strcmp(v1, v2) == 0;
}

unsigned int
c_str_hash(const void *v1)
{
    unsigned int hash = 0;
    char *p = (char *)v1;

    while (*p++)
        hash = (hash << 5) - (hash + *p);

    return hash;
}
