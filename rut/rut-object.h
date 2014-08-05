/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013,2014  Intel Corporation.
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
 */

#ifndef _RUT_OBJECT_H_
#define _RUT_OBJECT_H_

#include "rut-type.h"
#include "rut-refcount-debug.h"

C_BEGIN_DECLS

/* We largely give up having compile time type safety for rut_object_ts
 * since the type system is intended to be dynamic and most apis
 * dealing with rut_object_ts, care more about traits than about object
 * types. Using strong types would require us to always be casting
 * objects between different trait types which would make code more
 * verbose and by explicitly casting we'd lose the compile time type
 * safety anyway.
 */
typedef void rut_object_t;

/* Put one of these structs as the first member of your own struct
 * so that it can be used with the Rut type system.
 *
 * Allocate instances of your struct using rut_object_alloc() or
 * zeroed using rut_object_alloc0().
 */
typedef struct _rut_object_base_t {
    rut_type_t *type;
    int ref_count;
} rut_object_base_t;

typedef void (*rut_type_init_t)(void);

rut_object_t *
_rut_object_alloc(size_t bytes, rut_type_t *type, rut_type_init_t type_init);

rut_object_t *
_rut_object_alloc0(size_t bytes, rut_type_t *type, rut_type_init_t type_init);

#define rut_object_alloc(TYPE, TYPE_PTR, TYPE_INIT_FUNC)                       \
    (TYPE *)_rut_object_alloc(sizeof(TYPE), TYPE_PTR, TYPE_INIT_FUNC)

#define rut_object_alloc0(TYPE, TYPE_PTR, TYPE_INIT_FUNC)                      \
    (TYPE *)_rut_object_alloc0(sizeof(TYPE), TYPE_PTR, TYPE_INIT_FUNC)

void _rut_object_free(size_t bytes, void *object);

#define rut_object_free(TYPE, MEM) _rut_object_free(sizeof(TYPE), MEM);

void rut_object_init(rut_object_base_t *object_properties, rut_type_t *type);

static inline const rut_type_t *
rut_object_get_type(const rut_object_t *object)
{
    rut_object_base_t *obj = (rut_object_base_t *)object;
    return obj->type;
}

static inline void *
rut_object_get_properties(const rut_object_t *object,
                          rut_trait_id_t trait)
{
    rut_object_base_t *obj = (rut_object_base_t *)object;
    size_t props_offset = obj->type->traits[trait].props_offset;
    return (uint8_t *)obj + props_offset;
}

static inline void *
rut_object_get_vtable(const void *object, rut_trait_id_t trait)
{
    rut_object_base_t *obj = (rut_object_base_t *)object;
    return obj->type->traits[trait].vtable;
}

static inline bool
rut_object_is(const void *object, rut_trait_id_t trait)
{
    rut_object_base_t *obj = (rut_object_base_t *)object;
    return _rut_bitmask_get(&obj->type->traits_mask, trait);
}

static inline const char *
rut_object_get_type_name(void *object)
{
    rut_object_base_t *obj = (rut_object_base_t *)object;
    return obj->type->name;
}

static inline void *
rut_object_ref(void *object)
{
    rut_object_base_t *base = (rut_object_base_t *)object;

    _rut_refcount_debug_ref(object);

    base->ref_count++;
    return object;
}

static inline void
rut_object_unref(void *object)
{
    rut_object_base_t *base = (rut_object_base_t *)object;

    if (--(base->ref_count) < 1)
        base->type->free(object);

    _rut_refcount_debug_unref(object);
}

/* Wherever possible it is recommended that you use rut_object_claim()
 * and rut_object_release() instead of rut_object_ref()/_unref()
 * because it allows for much more powerfull debugging.
 *
 * This will track relationships between objects which is extremely
 * useful when tracking down leaks and we should even be able to use
 * the graph for automatically detecting leaks in the future by
 * detecting disconnected sub-graphs of objects.
 */
static inline void *
rut_object_claim(void *object, void *owner)
{
    rut_object_base_t *base = (rut_object_base_t *)object;

    _rut_refcount_debug_claim(object, owner);

    base->ref_count++;
    return object;
}

static inline void
rut_object_release(void *object, void *owner)
{
    rut_object_base_t *base = (rut_object_base_t *)object;

    if (--(base->ref_count) < 1)
        base->type->free(object);

    _rut_refcount_debug_release(object, owner);
}

C_END_DECLS

#endif /* _RUT_OBJECT_H_ */
