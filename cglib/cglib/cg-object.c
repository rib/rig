/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
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
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#include <cglib-config.h>

#include <clib.h>
#include <string.h>

#include "cg-util.h"
#include "cg-types.h"
#include "cg-object-private.h"

void *
cg_object_ref(void *object)
{
    cg_object_t *obj = object;

    c_return_val_if_fail(object != NULL, NULL);

    obj->ref_count++;
    return object;
}

void
_cg_object_default_unref(void *object)
{
    cg_object_t *obj = object;

    c_return_if_fail(object != NULL);
    c_return_if_fail(obj->ref_count > 0);

    if (--obj->ref_count < 1) {
        void (*free_func)(void * obj);

        if (obj->n_user_data_entries) {
            int i;
            int count = MIN(obj->n_user_data_entries,
                            CG_OBJECT_N_PRE_ALLOCATED_USER_DATA_ENTRIES);

            for (i = 0; i < count; i++) {
                cg_user_data_entry_t *entry = &obj->user_data_entry[i];
                if (entry->destroy)
                    entry->destroy(entry->user_data, obj);
            }

            if (obj->user_data_array != NULL) {
                for (i = 0; i < obj->user_data_array->len; i++) {
                    cg_user_data_entry_t *entry =
                        &c_array_index(
                            obj->user_data_array, cg_user_data_entry_t, i);

                    if (entry->destroy)
                        entry->destroy(entry->user_data, obj);
                }
                c_array_free(obj->user_data_array, true);
            }
        }

        CG_OBJECT_DEBUG_FREE(obj);
        free_func = obj->klass->virt_free;
        free_func(obj);
    }
}

void
cg_object_unref(void *obj)
{
    void (*unref_func)(void *) = ((cg_object_t *)obj)->klass->virt_unref;
    unref_func(obj);
}

/* XXX: Unlike for cg_object_get_user_data this code will return
 * an empty entry if available and no entry for the given key can be
 * found. */
static cg_user_data_entry_t *
_cg_object_find_entry(cg_object_t *object,
                      cg_user_data_key_t *key)
{
    cg_user_data_entry_t *entry = NULL;
    int count;
    int i;

    count = MIN(object->n_user_data_entries,
                CG_OBJECT_N_PRE_ALLOCATED_USER_DATA_ENTRIES);

    for (i = 0; i < count; i++) {
        cg_user_data_entry_t *current = &object->user_data_entry[i];
        if (current->key == key)
            return current;
        if (current->user_data == NULL)
            entry = current;
    }

    if (C_UNLIKELY(object->user_data_array != NULL)) {
        for (i = 0; i < object->user_data_array->len; i++) {
            cg_user_data_entry_t *current =
                &c_array_index(
                    object->user_data_array, cg_user_data_entry_t, i);

            if (current->key == key)
                return current;
            if (current->user_data == NULL)
                entry = current;
        }
    }

    return entry;
}

void
_cg_object_set_user_data(cg_object_t *object,
                         cg_user_data_key_t *key,
                         void *user_data,
                         cg_user_data_destroy_internal_callback_t destroy)
{
    cg_user_data_entry_t new_entry;
    cg_user_data_entry_t *entry;

    if (user_data) {
        new_entry.key = key;
        new_entry.user_data = user_data;
        new_entry.destroy = destroy;
    } else
        memset(&new_entry, 0, sizeof(new_entry));

    entry = _cg_object_find_entry(object, key);
    if (entry) {
        if (C_LIKELY(entry->destroy))
            entry->destroy(entry->user_data, object);
    } else {
        /* NB: Setting a value of NULL is documented to delete the
         * corresponding entry so we can return immediately in this
         * case. */
        if (user_data == NULL)
            return;

        if (C_LIKELY(object->n_user_data_entries <
                     CG_OBJECT_N_PRE_ALLOCATED_USER_DATA_ENTRIES))
            entry = &object->user_data_entry[object->n_user_data_entries++];
        else {
            if (C_UNLIKELY(object->user_data_array == NULL)) {
                object->user_data_array =
                    c_array_new(false, false, sizeof(cg_user_data_entry_t));
            }

            c_array_set_size(object->user_data_array,
                             object->user_data_array->len + 1);
            entry = &c_array_index(object->user_data_array,
                                   cg_user_data_entry_t,
                                   object->user_data_array->len - 1);

            object->n_user_data_entries++;
        }
    }

    *entry = new_entry;
}

void
cg_object_set_user_data(cg_object_t *object,
                        cg_user_data_key_t *key,
                        void *user_data,
                        cg_user_data_destroy_callback_t destroy)
{
    _cg_object_set_user_data(object,
                             key,
                             user_data,
                             (cg_user_data_destroy_internal_callback_t)destroy);
}

void *
cg_object_get_user_data(cg_object_t *object, cg_user_data_key_t *key)
{
    int count;
    int i;

    count = MIN(object->n_user_data_entries,
                CG_OBJECT_N_PRE_ALLOCATED_USER_DATA_ENTRIES);

    for (i = 0; i < count; i++) {
        cg_user_data_entry_t *entry = &object->user_data_entry[i];
        if (entry->key == key)
            return entry->user_data;
    }

    if (object->user_data_array != NULL) {
        for (i = 0; i < object->user_data_array->len; i++) {
            cg_user_data_entry_t *entry =
                &c_array_index(
                    object->user_data_array, cg_user_data_entry_t, i);

            if (entry->key == key)
                return entry->user_data;
        }
    }

    return NULL;
}

void
cg_debug_object_foreach_type(CGlibDebugObjectForeachTypeCallback func,
                             void *user_data)
{
    c_hash_table_iter_t iter;
    unsigned int *instance_count;
    cg_debug_object_type_info_t info;

    c_hash_table_iter_init(&iter, _cg_debug_instances);
    while (c_hash_table_iter_next(
               &iter, (void *)&info.name, (void *)&instance_count)) {
        info.instance_count = *instance_count;
        func(&info, user_data);
    }
}

static void
print_instances_cb(const cg_debug_object_type_info_t *info,
                   void *user_data)
{
    c_print("\t%s: %u\n", info->name, info->instance_count);
}

void
cg_debug_object_print_instances(void)
{
    c_print("CGlib instances:\n");

    cg_debug_object_foreach_type(print_instances_cb, NULL);
}
