/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2010 Intel Corporation.
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
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#include <rut-config.h>

#include <clib.h>
#include <string.h>

#include "rut-bitmask.h"
#include "rut-flags.h"

/* This code assumes that we can cast an unsigned long to a pointer
   and back without losing any data */
C_STATIC_ASSERT(sizeof(unsigned long) <= sizeof(void *),
                "Size of unsigned long larger than size of pointer!");

#define ARRAY_INDEX(bit_num) ((bit_num) / (sizeof(unsigned long) * 8))
#define BIT_INDEX(bit_num) ((bit_num) & (sizeof(unsigned long) * 8 - 1))
#define BIT_MASK(bit_num) (1UL << BIT_INDEX(bit_num))

bool
set_bit_cb(int bit, void *user_data)
{
    RutBitmask *b = user_data;
    _rut_bitmask_set(b, bit, true);
    return true;
}

void
_rut_bitmask_init_from_bitmask(RutBitmask *bitmask, const RutBitmask *src)
{
    _rut_bitmask_foreach(src, set_bit_cb, bitmask);
}

bool
_rut_bitmask_get_from_array(const RutBitmask *bitmask,
                            unsigned int bit_num)
{
    c_array_t *array = (c_array_t *)*bitmask;

    /* If the index is off the end of the array then assume the bit is
       not set */
    if (bit_num >= sizeof(unsigned long) * 8 * array->len)
        return false;
    else
        return !!(c_array_index(array, unsigned long, ARRAY_INDEX(bit_num)) &
                  BIT_MASK(bit_num));
}

static void
_rut_bitmask_convert_to_array(RutBitmask *bitmask)
{
    c_array_t *array;
    /* Fetch the old values */
    unsigned long old_values = _rut_bitmask_to_bits(bitmask);

    array = c_array_new(false, /* not zero-terminated */
                        true, /* do clear new entries */
                        sizeof(unsigned long));
    /* Copy the old values back in */
    c_array_append_val(array, old_values);

    *bitmask = (struct _rut_bitmask_imaginary_type_t *)array;
}

void
_rut_bitmask_set_in_array(RutBitmask *bitmask, unsigned int bit_num, bool value)
{
    c_array_t *array;
    unsigned int array_index;
    unsigned long new_value_mask;

    /* If the bitmask is not already an array then we need to allocate one */
    if (!_rut_bitmask_has_array(bitmask))
        _rut_bitmask_convert_to_array(bitmask);

    array = (c_array_t *)*bitmask;

    array_index = ARRAY_INDEX(bit_num);
    /* Grow the array if necessary. This will clear the new data */
    if (array_index >= array->len)
        c_array_set_size(array, array_index + 1);

    new_value_mask = BIT_MASK(bit_num);

    if (value)
        c_array_index(array, unsigned long, array_index) |= new_value_mask;
    else
        c_array_index(array, unsigned long, array_index) &= ~new_value_mask;
}

void
_rut_bitmask_set_bits(RutBitmask *dst, const RutBitmask *src)
{
    if (_rut_bitmask_has_array(src)) {
        c_array_t *src_array, *dst_array;
        int i;

        if (!_rut_bitmask_has_array(dst))
            _rut_bitmask_convert_to_array(dst);

        dst_array = (c_array_t *)*dst;
        src_array = (c_array_t *)*src;

        if (dst_array->len < src_array->len)
            c_array_set_size(dst_array, src_array->len);

        for (i = 0; i < src_array->len; i++)
            c_array_index(dst_array, unsigned long, i) |=
                c_array_index(src_array, unsigned long, i);
    } else if (_rut_bitmask_has_array(dst)) {
        c_array_t *dst_array;

        dst_array = (c_array_t *)*dst;

        c_array_index(dst_array, unsigned long, 0) |= _rut_bitmask_to_bits(src);
    } else
        *dst = _rut_bitmask_from_bits(_rut_bitmask_to_bits(dst) |
                                      _rut_bitmask_to_bits(src));
}

void
_rut_bitmask_set_range_in_array(RutBitmask *bitmask,
                                unsigned int n_bits,
                                bool value)
{
    c_array_t *array;
    unsigned int array_index, bit_index;

    if (n_bits == 0)
        return;

    /* If the bitmask is not already an array then we need to allocate one */
    if (!_rut_bitmask_has_array(bitmask))
        _rut_bitmask_convert_to_array(bitmask);

    array = (c_array_t *)*bitmask;

    /* Get the array index of the top most value that will be touched */
    array_index = ARRAY_INDEX(n_bits - 1);
    /* Get the bit index of the top most value */
    bit_index = BIT_INDEX(n_bits - 1);
    /* Grow the array if necessary. This will clear the new data */
    if (array_index >= array->len)
        c_array_set_size(array, array_index + 1);

    if (value) {
        /* Set the bits that are touching this index */
        c_array_index(array, unsigned long, array_index) |=
            ~0UL >> (sizeof(unsigned long) * 8 - 1 - bit_index);

        /* Set all of the bits in any lesser indices */
        memset(array->data, 0xff, sizeof(unsigned long) * array_index);
    } else {
        /* Clear the bits that are touching this index */
        c_array_index(array, unsigned long, array_index) &= ~1UL << bit_index;

        /* Clear all of the bits in any lesser indices */
        memset(array->data, 0x00, sizeof(unsigned long) * array_index);
    }
}

void
_rut_bitmask_xor_bits(RutBitmask *dst, const RutBitmask *src)
{
    if (_rut_bitmask_has_array(src)) {
        c_array_t *src_array, *dst_array;
        int i;

        if (!_rut_bitmask_has_array(dst))
            _rut_bitmask_convert_to_array(dst);

        dst_array = (c_array_t *)*dst;
        src_array = (c_array_t *)*src;

        if (dst_array->len < src_array->len)
            c_array_set_size(dst_array, src_array->len);

        for (i = 0; i < src_array->len; i++)
            c_array_index(dst_array, unsigned long, i) ^=
                c_array_index(src_array, unsigned long, i);
    } else if (_rut_bitmask_has_array(dst)) {
        c_array_t *dst_array;

        dst_array = (c_array_t *)*dst;

        c_array_index(dst_array, unsigned long, 0) ^= _rut_bitmask_to_bits(src);
    } else
        *dst = _rut_bitmask_from_bits(_rut_bitmask_to_bits(dst) ^
                                      _rut_bitmask_to_bits(src));
}

void
_rut_bitmask_clear_all_in_array(RutBitmask *bitmask)
{
    c_array_t *array = (c_array_t *)*bitmask;

    memset(array->data, 0, sizeof(unsigned long) * array->len);
}

void
_rut_bitmask_foreach(const RutBitmask *bitmask,
                     rut_bitmask_foreach_func_t func,
                     void *user_data)
{
    if (_rut_bitmask_has_array(bitmask)) {
        c_array_t *array = (c_array_t *)*bitmask;
        const unsigned long *values = &c_array_index(array, unsigned long, 0);
        int bit_num;

        RUT_FLAGS_FOREACH_START(values, array->len, bit_num)
        {
            if (!func(bit_num, user_data))
                return;
        }
        RUT_FLAGS_FOREACH_END;
    } else {
        unsigned long mask = _rut_bitmask_to_bits(bitmask);
        int bit_num;

        RUT_FLAGS_FOREACH_START(&mask, 1, bit_num)
        {
            if (!func(bit_num, user_data))
                return;
        }
        RUT_FLAGS_FOREACH_END;
    }
}

typedef struct _equal_state_t {
    const RutBitmask *b;
    bool equal;
} equal_state_t;

bool
check_bit_cb(int bit, void *user_data)
{
    equal_state_t *state = user_data;

    if (!_rut_bitmask_get(state->b, bit)) {
        state->equal = false;
        return false;
    }
    return true;
}

bool
_rut_bitmask_equal(const RutBitmask *a, const RutBitmask *b)
{
    equal_state_t state = { b, true };
    _rut_bitmask_foreach(a, check_bit_cb, &state);
    return state.equal;
}

void
_rut_bitmask_set_flags_array(const RutBitmask *bitmask,
                             unsigned long *flags)
{
    const c_array_t *array = (const c_array_t *)*bitmask;
    int i;

    for (i = 0; i < array->len; i++)
        flags[i] |= c_array_index(array, unsigned long, i);
}

int
_rut_bitmask_popcount_in_array(const RutBitmask *bitmask)
{
    const c_array_t *array = (const c_array_t *)*bitmask;
    int pop = 0;
    int i;

    for (i = 0; i < array->len; i++)
        pop += __builtin_popcountl(c_array_index(array, unsigned long, i));

    return pop;
}

int
_rut_bitmask_popcount_upto_in_array(const RutBitmask *bitmask, int upto)
{
    const c_array_t *array = (const c_array_t *)*bitmask;

    if (upto >= array->len * sizeof(unsigned long) * 8)
        return _rut_bitmask_popcount_in_array(bitmask);
    else {
        unsigned long top_mask;
        int array_index = ARRAY_INDEX(upto);
        int bit_index = BIT_INDEX(upto);
        int pop = 0;
        int i;

        for (i = 0; i < array_index; i++)
            pop += __builtin_popcountl(c_array_index(array, unsigned long, i));

        top_mask = c_array_index(array, unsigned long, array_index);

        return pop + __builtin_popcountl(top_mask & ((1UL << bit_index) - 1));
    }
}
