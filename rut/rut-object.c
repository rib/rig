/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012  Intel Corporation
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

#include <rut-config.h>

#include "rut-type.h"
#include "rut-object.h"
#include "rut-refcount-debug.h"

void
rut_object_init(rut_object_base_t *object, rut_type_t *type)
{
    object->type = type;
    object->ref_count = 1;

    _rut_refcount_debug_object_created(object);
}

rut_object_t *
_rut_object_alloc(size_t bytes, rut_type_t *type, rut_type_init_t type_init)
{
    rut_object_t *object;

    if (C_UNLIKELY(type->name == NULL))
        type_init();

    if (type->magazine)
        object = rut_magazine_chunk_alloc(type->magazine);
    else
        object = c_slice_alloc(bytes);

    rut_object_init(object, type);

    return object;
}

rut_object_t *
_rut_object_alloc0(size_t bytes, rut_type_t *type, rut_type_init_t type_init)
{
    rut_object_t *object = c_slice_alloc0(bytes);

    if (C_UNLIKELY(type->name == NULL))
        type_init();

    rut_object_init(object, type);

    return object;
}

void
_rut_object_free(size_t bytes, void *object)
{
    rut_object_base_t *base = object;
    rut_type_t *type = base->type;
    rut_trait_implementation_t *trait, *tmp;

    c_list_for_each_safe(trait, tmp, &type->destructors, destructor_link)
    {
        trait->destructor(object);
    }

    if (type->magazine)
        rut_magazine_chunk_free(type->magazine, object);
    else
        c_slice_free1(bytes, object);
}
