/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
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

#include <cglib-config.h>

#include <clib.h>
#include <string.h>

#include <test-fixtures/test-cg-fixtures.h>

#include "cg-bitmask.h"
#include "cg-util.h"
#include "cg-flags.h"

/* This code assumes that we can cast an unsigned long to a pointer
   and back without losing any data */
_C_STATIC_ASSERT(sizeof(unsigned long) <= sizeof(void *),
                  "This toolchain breaks CGlib's assumption that it can "
                  "safely cast an unsigned long to a pointer without "
                  "loosing data");

#define ARRAY_INDEX(bit_num) ((bit_num) / (sizeof(unsigned long) * 8))
#define BIT_INDEX(bit_num) ((bit_num) & (sizeof(unsigned long) * 8 - 1))
#define BIT_MASK(bit_num) (1UL << BIT_INDEX(bit_num))

bool
_cg_bitmask_get_from_array(const CGlibBitmask *bitmask,
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
_cg_bitmask_convert_to_array(CGlibBitmask *bitmask)
{
    c_array_t *array;
    /* Fetch the old values */
    unsigned long old_values = _cg_bitmask_to_bits(bitmask);

    array = c_array_new(false, /* not zero-terminated */
                        true, /* do clear new entries */
                        sizeof(unsigned long));
    /* Copy the old values back in */
    c_array_append_val(array, old_values);

    *bitmask = (struct _cg_bitmask_imaginary_type_t *)array;
}

void
_cg_bitmask_set_in_array(CGlibBitmask *bitmask, unsigned int bit_num, bool value)
{
    c_array_t *array;
    unsigned int array_index;
    unsigned long new_value_mask;

    /* If the bitmask is not already an array then we need to allocate one */
    if (!_cg_bitmask_has_array(bitmask))
        _cg_bitmask_convert_to_array(bitmask);

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
_cg_bitmask_set_bits(CGlibBitmask *dst, const CGlibBitmask *src)
{
    if (_cg_bitmask_has_array(src)) {
        c_array_t *src_array, *dst_array;
        int i;

        if (!_cg_bitmask_has_array(dst))
            _cg_bitmask_convert_to_array(dst);

        dst_array = (c_array_t *)*dst;
        src_array = (c_array_t *)*src;

        if (dst_array->len < src_array->len)
            c_array_set_size(dst_array, src_array->len);

        for (i = 0; i < src_array->len; i++)
            c_array_index(dst_array, unsigned long, i) |=
                c_array_index(src_array, unsigned long, i);
    } else if (_cg_bitmask_has_array(dst)) {
        c_array_t *dst_array;

        dst_array = (c_array_t *)*dst;

        c_array_index(dst_array, unsigned long, 0) |= _cg_bitmask_to_bits(src);
    } else
        *dst = _cg_bitmask_from_bits(_cg_bitmask_to_bits(dst) |
                                     _cg_bitmask_to_bits(src));
}

void
_cg_bitmask_set_range_in_array(CGlibBitmask *bitmask,
                               unsigned int n_bits,
                               bool value)
{
    c_array_t *array;
    unsigned int array_index, bit_index;

    if (n_bits == 0)
        return;

    /* If the bitmask is not already an array then we need to allocate one */
    if (!_cg_bitmask_has_array(bitmask))
        _cg_bitmask_convert_to_array(bitmask);

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
_cg_bitmask_xor_bits(CGlibBitmask *dst, const CGlibBitmask *src)
{
    if (_cg_bitmask_has_array(src)) {
        c_array_t *src_array, *dst_array;
        int i;

        if (!_cg_bitmask_has_array(dst))
            _cg_bitmask_convert_to_array(dst);

        dst_array = (c_array_t *)*dst;
        src_array = (c_array_t *)*src;

        if (dst_array->len < src_array->len)
            c_array_set_size(dst_array, src_array->len);

        for (i = 0; i < src_array->len; i++)
            c_array_index(dst_array, unsigned long, i) ^=
                c_array_index(src_array, unsigned long, i);
    } else if (_cg_bitmask_has_array(dst)) {
        c_array_t *dst_array;

        dst_array = (c_array_t *)*dst;

        c_array_index(dst_array, unsigned long, 0) ^= _cg_bitmask_to_bits(src);
    } else
        *dst = _cg_bitmask_from_bits(_cg_bitmask_to_bits(dst) ^
                                     _cg_bitmask_to_bits(src));
}

void
_cg_bitmask_clear_all_in_array(CGlibBitmask *bitmask)
{
    c_array_t *array = (c_array_t *)*bitmask;

    memset(array->data, 0, sizeof(unsigned long) * array->len);
}

void
_cg_bitmask_foreach(const CGlibBitmask *bitmask,
                    cg_bitmask_foreach_func_t func,
                    void *user_data)
{
    if (_cg_bitmask_has_array(bitmask)) {
        c_array_t *array = (c_array_t *)*bitmask;
        const unsigned long *values = &c_array_index(array, unsigned long, 0);
        int bit_num;

        CG_FLAGS_FOREACH_START(values, array->len, bit_num)
        {
            if (!func(bit_num, user_data))
                return;
        }
        CG_FLAGS_FOREACH_END;
    } else {
        unsigned long mask = _cg_bitmask_to_bits(bitmask);
        int bit_num;

        CG_FLAGS_FOREACH_START(&mask, 1, bit_num)
        {
            if (!func(bit_num, user_data))
                return;
        }
        CG_FLAGS_FOREACH_END;
    }
}

void
_cg_bitmask_set_flags_array(const CGlibBitmask *bitmask,
                            unsigned long *flags)
{
    const c_array_t *array = (const c_array_t *)*bitmask;
    int i;

    for (i = 0; i < array->len; i++)
        flags[i] |= c_array_index(array, unsigned long, i);
}

int
_cg_bitmask_popcount_in_array(const CGlibBitmask *bitmask)
{
    const c_array_t *array = (const c_array_t *)*bitmask;
    int pop = 0;
    int i;

    for (i = 0; i < array->len; i++)
        pop += _cg_util_popcountl(c_array_index(array, unsigned long, i));

    return pop;
}

int
_cg_bitmask_popcount_upto_in_array(const CGlibBitmask *bitmask, int upto)
{
    const c_array_t *array = (const c_array_t *)*bitmask;

    if (upto >= array->len * sizeof(unsigned long) * 8)
        return _cg_bitmask_popcount_in_array(bitmask);
    else {
        unsigned long top_mask;
        int array_index = ARRAY_INDEX(upto);
        int bit_index = BIT_INDEX(upto);
        int pop = 0;
        int i;

        for (i = 0; i < array_index; i++)
            pop += _cg_util_popcountl(c_array_index(array, unsigned long, i));

        top_mask = c_array_index(array, unsigned long, array_index);

        return pop + _cg_util_popcountl(top_mask & ((1UL << bit_index) - 1));
    }
}

typedef struct {
    int n_bits;
    int *bits;
} check_data_t;

static bool
check_bit(int bit_num, void *user_data)
{
    check_data_t *data = user_data;
    int i;

    for (i = 0; i < data->n_bits; i++)
        if (data->bits[i] == bit_num) {
            data->bits[i] = -1;
            return true;
        }

    c_assert_not_reached();

    return true;
}

static void
verify_bits(const CGlibBitmask *bitmask, ...)
{
    check_data_t data;
    va_list ap, ap_copy;
    int i;

    va_start(ap, bitmask);
    C_VA_COPY(ap_copy, ap);

    for (data.n_bits = 0; va_arg(ap, int)!= -1; data.n_bits++)
        ;

    data.bits = alloca(data.n_bits * (sizeof(int)));

    C_VA_COPY(ap, ap_copy);

    for (i = 0; i < data.n_bits; i++)
        data.bits[i] = va_arg(ap, int);

    _cg_bitmask_foreach(bitmask, check_bit, &data);

    for (i = 0; i < data.n_bits; i++)
        c_assert_cmpint(data.bits[i], ==, -1);

    c_assert_cmpint(_cg_bitmask_popcount(bitmask), ==, data.n_bits);

    for (i = 0; i < 1024; i++) {
        int upto_popcount = 0;
        int j;

        C_VA_COPY(ap, ap_copy);

        for (j = 0; j < data.n_bits; j++)
            if (va_arg(ap, int)< i)
                upto_popcount++;

        c_assert_cmpint(
            _cg_bitmask_popcount_upto(bitmask, i), ==, upto_popcount);

        C_VA_COPY(ap, ap_copy);

        for (j = 0; j < data.n_bits; j++)
            if (va_arg(ap, int)== i)
                break;

        c_assert_cmpint(_cg_bitmask_get(bitmask, i), ==, (j < data.n_bits));
    }
}

TEST(check_bitmask_api)
{
    CGlibBitmask bitmask;
    CGlibBitmask other_bitmask;
    /* A dummy bit to make it use arrays sometimes */
    int dummy_bit;
    int i;

    test_cg_init();

    for (dummy_bit = -1; dummy_bit < 256; dummy_bit += 40) {
        _cg_bitmask_init(&bitmask);
        _cg_bitmask_init(&other_bitmask);

        if (dummy_bit != -1)
            _cg_bitmask_set(&bitmask, dummy_bit, true);

        verify_bits(&bitmask, dummy_bit, -1);

        _cg_bitmask_set(&bitmask, 1, true);
        _cg_bitmask_set(&bitmask, 4, true);
        _cg_bitmask_set(&bitmask, 5, true);

        verify_bits(&bitmask, 1, 4, 5, dummy_bit, -1);

        _cg_bitmask_set(&bitmask, 4, false);

        verify_bits(&bitmask, 1, 5, dummy_bit, -1);

        _cg_bitmask_clear_all(&bitmask);

        verify_bits(&bitmask, -1);

        if (dummy_bit != -1)
            _cg_bitmask_set(&bitmask, dummy_bit, true);

        verify_bits(&bitmask, dummy_bit, -1);

        _cg_bitmask_set(&bitmask, 1, true);
        _cg_bitmask_set(&bitmask, 4, true);
        _cg_bitmask_set(&bitmask, 5, true);
        _cg_bitmask_set(&other_bitmask, 5, true);
        _cg_bitmask_set(&other_bitmask, 6, true);

        _cg_bitmask_set_bits(&bitmask, &other_bitmask);

        verify_bits(&bitmask, 1, 4, 5, 6, dummy_bit, -1);
        verify_bits(&other_bitmask, 5, 6, -1);

        _cg_bitmask_set(&bitmask, 6, false);

        verify_bits(&bitmask, 1, 4, 5, dummy_bit, -1);

        _cg_bitmask_xor_bits(&bitmask, &other_bitmask);

        verify_bits(&bitmask, 1, 4, 6, dummy_bit, -1);
        verify_bits(&other_bitmask, 5, 6, -1);

        _cg_bitmask_set_range(&bitmask, 5, true);

        verify_bits(&bitmask, 0, 1, 2, 3, 4, 6, dummy_bit, -1);

        _cg_bitmask_set_range(&bitmask, 4, false);

        verify_bits(&bitmask, 4, 6, dummy_bit, -1);

        _cg_bitmask_destroy(&other_bitmask);
        _cg_bitmask_destroy(&bitmask);
    }

    /* Extra tests for really long bitmasks */
    _cg_bitmask_init(&bitmask);
    _cg_bitmask_set_range(&bitmask, 400, true);
    _cg_bitmask_init(&other_bitmask);
    _cg_bitmask_set(&other_bitmask, 5, true);
    _cg_bitmask_xor_bits(&bitmask, &other_bitmask);

    for (i = 0; i < 1024; i++)
        c_assert_cmpint(_cg_bitmask_get(&bitmask, i),
                        ==,
                        (i == 5 ? false : i < 400 ? true : false));

    _cg_bitmask_set_range(&other_bitmask, 500, true);
    _cg_bitmask_set_bits(&bitmask, &other_bitmask);

    for (i = 0; i < 1024; i++)
        c_assert_cmpint(_cg_bitmask_get(&bitmask, i), ==, (i < 500));

    test_cg_fini();
}
