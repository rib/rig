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

#ifndef _RUT_TYPE_H_
#define _RUT_TYPE_H_

#include <sys/types.h>

#include <clib.h>

#include "rut-bitmask.h"
#include "rut-magazine.h"

C_BEGIN_DECLS

/* Indices for builtin traits */
typedef enum _rut_trait_id_t {
    RUT_TRAIT_ID_GRAPHABLE = 1,
    RUT_TRAIT_ID_INTROSPECTABLE,
    RUT_TRAIT_ID_PAINTABLE,
    RUT_TRAIT_ID_TRANSFORMABLE,
    RUT_TRAIT_ID_COMPONENTABLE,
    RUT_TRAIT_ID_SIZABLE,
    RUT_TRAIT_ID_COMPOSITE_SIZABLE,
    RUT_TRAIT_ID_PRIMABLE,
    RUT_TRAIT_ID_MESHABLE,
    RUT_TRAIT_ID_INPUTABLE,
    RUT_TRAIT_ID_PICKABLE,
    RUT_TRAIT_ID_IMAGE_SIZE_DEPENDENT,
    RUT_TRAIT_ID_RENDERER,
    RUT_TRAIT_ID_SELECTABLE,
    RUT_TRAIT_ID_MIMABLE,
    RUT_TRAIT_ID_CAMERA,

    RUT_N_BUILTIN_TRAITS
} rut_trait_id_t;

typedef void (*RutTraitDestructor)(void *object);

/* A trait implementation optionally defines:
 * - An offset into an instance for some arbitrary data
 * - A table of function pointers specific to that trait
 * - A destructor that can be used to clean up data when
 *   an instance associated with the trait is freed.
 */
typedef struct _rut_trait_implementation_t {
    /* Interfaces with a destructor get linked into rut_type_t::destructors */
    c_list_t destructor_link;

    size_t props_offset;
    void *vtable;
    RutTraitDestructor destructor;

} rut_trait_implementation_t;

/* The pointer to a rut_type_t variable serves as a unique identifier for
 * a type and the rut_type_t itself contains a bitmask of traits associated
 * with the type.
 *
 * A trait may imply a set of functions and/or a set of properties are
 * associated with the type.
 *
 * vtables is an array of pointers to trait function vtables where
 * each index corresponds to a bit set in the traits mask.
 */
typedef struct _rut_type_t {
    RutBitmask traits_mask;
    rut_trait_implementation_t *traits;
    rut_magazine_t *magazine;
    void (*free)(void *object);
    c_list_t destructors;

    const char *name;
} rut_type_t;

/* Note: the type destructor is called before any trait destructors */
typedef void (*rut_type_destructor_t)(void *object);

void rut_type_init(rut_type_t *type,
                   const char *name,
                   rut_type_destructor_t type_destructor);

void rut_type_set_magazine(rut_type_t *type, rut_magazine_t *magazine);

void rut_type_add_trait(rut_type_t *type,
                        rut_trait_id_t id,
                        size_t instance_priv_offset,
                        void *interface_vtable);

void rut_trait_set_destructor(rut_type_t *type,
                              rut_trait_id_t id,
                              RutTraitDestructor trait_destructor);

void rut_ensure_trait_id(int *trait_id);

C_END_DECLS

#endif /* _RUT_TYPE_H_ */
