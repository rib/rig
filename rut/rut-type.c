/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012,2013  Intel Corporation
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

#include <stdbool.h>

#include "rut-type.h"
#include "rut-bitmask.h"

static int rut_next_trait_id = RUT_N_BUILTIN_TRAITS;

static bool
find_max_id(int bit_num, void *user_data)
{
    int *max_id = user_data;
    if (bit_num > *max_id)
        *max_id = bit_num;

    return true;
}

void
rut_type_add_trait(rut_type_t *type,
                   rut_trait_id_t id,
                   size_t instance_priv_offset,
                   void *interface_vtable)
{
    int max_id = 0;

    _rut_bitmask_foreach(&type->traits_mask, find_max_id, &max_id);

    if (id > max_id) {
        type->traits = c_realloc(type->traits,
                                 sizeof(rut_trait_implementation_t) * (id + 1));
    }

    _rut_bitmask_set(&type->traits_mask, id, true);
    type->traits[id].props_offset = instance_priv_offset;
    type->traits[id].vtable = interface_vtable;
    type->traits[id].destructor = NULL;
}

void
rut_trait_set_destructor(rut_type_t *type,
                         rut_trait_id_t id,
                         RutTraitDestructor trait_destructor)
{
    type->traits[id].destructor = trait_destructor;
    c_list_insert(type->destructors.prev, &type->traits[id].destructor_link);
}

void
rut_type_init(rut_type_t *type,
              const char *name,
              rut_type_destructor_t destructor)
{
    /* Note: we can assume the type is zeroed at this point */

    type->name = name;
    type->free = destructor;

    _rut_bitmask_init(&type->traits_mask);
    c_list_init(&type->destructors);
}

void
rut_type_set_magazine(rut_type_t *type, rut_magazine_t *magazine)
{
    type->magazine = magazine;
}

void
rut_ensure_trait_id(int *trait_id)
{
    if (*trait_id == 0) {
        *trait_id = rut_next_trait_id++;
    }
}
