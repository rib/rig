/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 *   Robert Bragg <robert@linux.intel.com>
 */

#include <cglib-config.h>

#include "cg-device-private.h"
#include "cg-pipeline-private.h"
#include "cg-pipeline-hash-table.h"
#include "cg-pipeline-cache.h"

typedef struct {
    cg_pipeline_cache_entry_t parent;

    /* Calculating the hash is a little bit expensive for pipelines so
     * we don't want to do it repeatedly for entries that are already in
     * the hash table. Instead we cache the value here and calculate it
     * outside of the c_hash_table_t. */
    unsigned int hash_value;

    /* c_hash_table_t annoyingly doesn't let us pass a user data pointer to
     * the hash and equal functions so to work around it we have to
     * store the pointer in every hash table entry. We will use this
     * entry as both the key and the value */
    cg_pipeline_hash_table_t *hash;

    /* The number of unique pipelines that had been created when this
     * pipeline was last accessed */
    int age;
} cg_pipeline_hash_table_entry_t;

static void
value_destroy_cb(void *value)
{
    cg_pipeline_hash_table_entry_t *entry = value;

    cg_object_unref(entry->parent.pipeline);

    c_slice_free(cg_pipeline_hash_table_entry_t, entry);
}

static unsigned int
entry_hash(const void *data)
{
    const cg_pipeline_hash_table_entry_t *entry = data;

    return entry->hash_value;
}

static bool
entry_equal(const void *a, const void *b)
{
    const cg_pipeline_hash_table_entry_t *entry_a = a;
    const cg_pipeline_hash_table_entry_t *entry_b = b;
    const cg_pipeline_hash_table_t *hash = entry_a->hash;

    return _cg_pipeline_equal(entry_a->parent.pipeline,
                              entry_b->parent.pipeline,
                              hash->main_state,
                              hash->layer_state,
                              0);
}

void
_cg_pipeline_hash_table_init(cg_pipeline_hash_table_t *hash,
                             cg_device_t *dev,
                             unsigned int main_state,
                             unsigned int layer_state,
                             const char *debug_string)
{
    hash->dev = dev;
    hash->n_unique_pipelines = 0;
    hash->debug_string = debug_string;
    hash->main_state = main_state;
    hash->layer_state = layer_state;
    /* We'll only start pruning once we get to 16 unique pipelines */
    hash->expected_min_size = 8;
    hash->table = c_hash_table_new_full(entry_hash,
                                        entry_equal,
                                        NULL, /* key destroy */
                                        value_destroy_cb);
}

void
_cg_pipeline_hash_table_destroy(cg_pipeline_hash_table_t *hash)
{
    c_hash_table_destroy(hash->table);
}

static void
collect_prunable_entries_cb(void *key, void *value, void *user_data)
{
    c_queue_t *entries = user_data;
    cg_pipeline_cache_entry_t *entry = value;

    if (entry->usage_count == 0)
        c_queue_push_tail(entries, entry);
}

static int
compare_pipeline_age_cb(const void *a, const void *b)
{
    const cg_pipeline_hash_table_entry_t *ae = a;
    const cg_pipeline_hash_table_entry_t *be = b;

    return be->age - ae->age;
}

static void
prune_old_pipelines(cg_pipeline_hash_table_t *hash)
{
    c_queue_t entries;
    c_llist_t *l;
    int i;

    /* Collect all of the prunable entries into a c_queue_t */
    c_queue_init(&entries);
    c_hash_table_foreach(hash->table, collect_prunable_entries_cb, &entries);

    /* Sort the entries by increasing order of age */
    entries.head = c_llist_sort(entries.head, compare_pipeline_age_cb);

    /* The +1 is to include the pipeline that we're about to add */
    hash->expected_min_size =
        (c_hash_table_size(hash->table) - entries.length + 1);

    /* Remove oldest half of the prunable pipelines. We still want to
     * keep some of the prunable entries that are recently used because
     * it's not unlikely that the application will recreate the same
     * pipeline */
    for (l = entries.head, i = 0; i < entries.length / 2; l = l->next, i++) {
        cg_pipeline_cache_entry_t *entry = l->data;

        c_hash_table_remove(hash->table, entry);
    }

    c_llist_free(entries.head);
}

cg_pipeline_cache_entry_t *
_cg_pipeline_hash_table_get(cg_pipeline_hash_table_t *hash,
                            cg_pipeline_t *key_pipeline)
{
    cg_pipeline_hash_table_entry_t dummy_entry;
    cg_pipeline_hash_table_entry_t *entry;
    unsigned int copy_state;

    dummy_entry.parent.pipeline = key_pipeline;
    dummy_entry.hash = hash;
    dummy_entry.hash_value =
        _cg_pipeline_hash(key_pipeline, hash->main_state, hash->layer_state, 0);
    entry = c_hash_table_lookup(hash->table, &dummy_entry);

    if (entry) {
        entry->age = hash->n_unique_pipelines;
        return &entry->parent;
    }

    if (hash->n_unique_pipelines == 50)
        c_warning("Over 50 separate %s have been generated which is very "
                  "unusual, so something is probably wrong!\n",
                  hash->debug_string);

    /* If we are going to have more than twice the expected minimum
     * number of pipelines in the hash then we'll try pruning and update
     * the minimum */
    if (c_hash_table_size(hash->table) >= hash->expected_min_size * 2)
        prune_old_pipelines(hash);

    entry = c_slice_new(cg_pipeline_hash_table_entry_t);
    entry->parent.usage_count = 0;
    entry->hash = hash;
    entry->hash_value = dummy_entry.hash_value;
    entry->age = hash->n_unique_pipelines;

    copy_state = hash->main_state;
    if (hash->layer_state)
        copy_state |= CG_PIPELINE_STATE_LAYERS;

    /* Create a new pipeline that is a child of the root pipeline
     * instead of a normal copy so that the template pipeline won't hold
     * a reference to the original pipeline */
    entry->parent.pipeline = _cg_pipeline_deep_copy(hash->dev,
                                                    key_pipeline,
                                                    copy_state,
                                                    hash->layer_state);

    c_hash_table_insert(hash->table, entry, entry);

    hash->n_unique_pipelines++;

    return &entry->parent;
}
