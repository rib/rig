/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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

#ifndef __CG_OBJECT_PRIVATE_H
#define __CG_OBJECT_PRIVATE_H

#include <clib.h>

#include "cg-types.h"
#include "cg-object.h"
#include "cg-debug.h"

/* XXX: sadly we didn't fully consider when we copied the cairo API
 * for _set_user_data that the callback doesn't get a pointer to the
 * instance which is desired in most cases. This means you tend to end
 * up creating micro allocations for the private data just so you can
 * pair up the data of interest with the original instance for
 * identification when it is later destroyed.
 *
 * Internally we use a small hack to avoid needing these micro
 * allocations by actually passing the instance as a second argument
 * to the callback */
typedef void (*cg_user_data_destroy_internal_callback_t)(void *user_data,
                                                         void *instance);

typedef struct _cg_object_class_t {
    const char *name;
    void *virt_free;
    void *virt_unref;
} cg_object_class_t;

#define CG_OBJECT_N_PRE_ALLOCATED_USER_DATA_ENTRIES 2

typedef struct {
    cg_user_data_key_t *key;
    void *user_data;
    cg_user_data_destroy_internal_callback_t destroy;
} cg_user_data_entry_t;

/* All CGlib objects inherit from this base object by adding a member:
 *
 *   cg_object_t _parent;
 *
 * at the top of its main structure. This structure is initialized
 * when you call _cg_#type_name#_object_new (new_object);
 */
struct _cg_object_t {
    cg_object_class_t *klass;

    cg_user_data_entry_t user_data_entry
    [CG_OBJECT_N_PRE_ALLOCATED_USER_DATA_ENTRIES];
    c_array_t *user_data_array;
    int n_user_data_entries;

    unsigned int ref_count;
};

/* Helper macro to encapsulate the common code for COGL reference
   counted objects */

#ifdef CG_OBJECT_DEBUG

#define _CG_OBJECT_DEBUG_NEW(type_name, obj)                                   \
    CG_NOTE(OBJECT,                                                            \
            "COGL cg_" C_STRINGIFY(type_name) "_t NEW   %p %i",                \
            (obj),                                                             \
            (obj)->ref_count)

#define _CG_OBJECT_DEBUG_REF(type_name, object)                                \
    C_STMT_START                                                               \
    {                                                                          \
        cg_object_t *__obj = (cg_object_t *)object;                            \
        CG_NOTE(OBJECT,                                                        \
                "COGL %s REF %p %i",                                           \
                (__obj)->klass->name,                                          \
                (__obj),                                                       \
                (__obj)->ref_count);                                           \
    }                                                                          \
    C_STMT_END

#define _CG_OBJECT_DEBUG_UNREF(type_name, object)                              \
    C_STMT_START                                                               \
    {                                                                          \
        cg_object_t *__obj = (cg_object_t *)object;                            \
        CG_NOTE(OBJECT,                                                        \
                "COGL %s UNREF %p %i",                                         \
                (__obj)->klass->name,                                          \
                (__obj),                                                       \
                (__obj)->ref_count - 1);                                       \
    }                                                                          \
    C_STMT_END

#define CG_OBJECT_DEBUG_FREE(obj)                                              \
    CG_NOTE(OBJECT, "COGL %s FREE %p", (obj)->klass->name, (obj))

#else /* !CG_OBJECT_DEBUG */

#define _CG_OBJECT_DEBUG_NEW(type_name, obj)
#define _CG_OBJECT_DEBUG_REF(type_name, obj)
#define _CG_OBJECT_DEBUG_UNREF(type_name, obj)
#define CG_OBJECT_DEBUG_FREE(obj)

#endif /* CG_OBJECT_DEBUG */

#define CG_OBJECT_COMMON_DEFINE_WITH_CODE(TypeName, type_name, code)           \
    cg_object_class_t _cg_##type_name##_class;                                 \
    static unsigned int _cg_object_##type_name##_count;                        \
    static inline void _cg_object_##type_name##_inc(void)                      \
    {                                                                          \
        _cg_object_##type_name##_count++;                                      \
    }                                                                          \
    static inline void _cg_object_##type_name##_dec(void)                      \
    {                                                                          \
        _cg_object_##type_name##_count--;                                      \
    }                                                                          \
    static void _cg_object_##type_name##_indirect_free(cg_object_t *obj)       \
    {                                                                          \
        _cg_##type_name##_free((cg_##type_name##_t *)obj);                     \
        _cg_object_##type_name##_dec();                                        \
    }                                                                          \
    static cg_##type_name##_t *_cg_##type_name##_object_new(                   \
        cg_##type_name##_t *new_obj)                                           \
    {                                                                          \
        cg_object_t *obj = (cg_object_t *)&new_obj->_parent;                   \
        obj->ref_count = 0;                                                    \
        cg_object_ref(obj);                                                    \
        obj->n_user_data_entries = 0;                                          \
        obj->user_data_array = NULL;                                           \
                                                                               \
        obj->klass = &_cg_##type_name##_class;                                 \
        if (!obj->klass->virt_free) {                                          \
            _cg_object_##type_name##_count = 0;                                \
                                                                               \
            if (_cg_debug_instances == NULL)                                   \
                _cg_debug_instances =                                          \
                    c_hash_table_new(c_str_hash, c_str_equal);                 \
                                                                               \
            obj->klass->virt_free = _cg_object_##type_name##_indirect_free;    \
            obj->klass->virt_unref = _cg_object_default_unref;                 \
            obj->klass->name = "cg_" #type_name "_t",                          \
            c_hash_table_insert(_cg_debug_instances,                           \
                                (void *)obj->klass->name,                      \
                                &_cg_object_##type_name##_count);              \
                                                                               \
            {                                                                  \
                code;                                                          \
            }                                                                  \
        }                                                                      \
                                                                               \
        _cg_object_##type_name##_inc();                                        \
        _CG_OBJECT_DEBUG_NEW(type_name, obj);                                  \
        return new_obj;                                                        \
    }

#define CG_OBJECT_DEFINE_WITH_CODE(TypeName, type_name, code)                  \
    CG_OBJECT_COMMON_DEFINE_WITH_CODE(                                         \
        TypeName, type_name, code) bool cg_is_##type_name(void *object)        \
    {                                                                          \
        cg_object_t *obj = object;                                             \
                                                                               \
        if (object == NULL)                                                    \
            return false;                                                      \
                                                                               \
        return obj->klass == &_cg_##type_name##_class;                         \
    }

#define CG_OBJECT_INTERNAL_DEFINE_WITH_CODE(TypeName, type_name, code)         \
    CG_OBJECT_COMMON_DEFINE_WITH_CODE(                                         \
        TypeName, type_name, code) bool _cg_is_##type_name(void *object)       \
    {                                                                          \
        cg_object_t *obj = object;                                             \
                                                                               \
        if (object == NULL)                                                    \
            return false;                                                      \
                                                                               \
        return obj->klass == &_cg_##type_name##_class;                         \
    }

#define CG_OBJECT_DEFINE(TypeName, type_name)                                  \
    CG_OBJECT_DEFINE_WITH_CODE(TypeName, type_name, (void)0)

#define CG_OBJECT_INTERNAL_DEFINE(TypeName, type_name)                         \
    CG_OBJECT_INTERNAL_DEFINE_WITH_CODE(TypeName, type_name, (void)0)

void _cg_object_set_user_data(cg_object_t *object,
                              cg_user_data_key_t *key,
                              void *user_data,
                              cg_user_data_destroy_internal_callback_t destroy);

void _cg_object_default_unref(void *obj);

#endif /* __CG_OBJECT_PRIVATE_H */
