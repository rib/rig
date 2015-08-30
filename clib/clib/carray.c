/*
 * Arrays
 *
 * Author:
 *   Chris Toshok (toshok@novell.com)
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

#include <stdlib.h>
#include <clib.h>

#define INITIAL_CAPACITY 16U

#define element_offset(p, i) ((p)->array.data + (i) * (p)->element_size)
#define element_length(p, i) ((i) * (p)->element_size)

typedef struct {
    c_array_t array;
    bool clear_;
    unsigned int element_size;
    bool zero_terminated;
    unsigned int capacity;
} c_array_priv_t;

static void
ensure_capacity(c_array_priv_t *priv, unsigned int capacity)
{
    unsigned int new_capacity;

    if (capacity <= priv->capacity)
        return;

    for (new_capacity = MAX(INITIAL_CAPACITY, (unsigned)(priv->capacity * 1.5));
         new_capacity < capacity;
         new_capacity *= 1.5)
        ;

    priv->array.data =
        c_realloc(priv->array.data, element_length(priv, new_capacity));

    if (priv->clear_) {
        memset(element_offset(priv, priv->capacity),
               0,
               element_length(priv, new_capacity - priv->capacity));
    }

    priv->capacity = new_capacity;
}

c_array_t *
c_array_new(bool zero_terminated, bool clear_, unsigned int element_size)
{
    c_array_priv_t *rv = c_new0(c_array_priv_t, 1);
    rv->zero_terminated = zero_terminated;
    rv->clear_ = clear_;
    rv->element_size = element_size;

    ensure_capacity(rv, INITIAL_CAPACITY);

    return (c_array_t *)rv;
}

c_array_t *
c_array_sized_new(bool zero_terminated,
                  bool clear_,
                  unsigned int element_size,
                  unsigned int reserved_size)
{
    c_array_priv_t *rv = c_new0(c_array_priv_t, 1);
    rv->zero_terminated = zero_terminated;
    rv->clear_ = clear_;
    rv->element_size = element_size;

    ensure_capacity(rv, reserved_size);

    return (c_array_t *)rv;
}

char *
c_array_free(c_array_t *array, bool free_segment)
{
    char *rv = NULL;

    c_return_val_if_fail(array != NULL, NULL);

    if (free_segment)
        c_free(array->data);
    else
        rv = array->data;

    c_free(array);

    return rv;
}

c_array_t *
c_array_append_vals(c_array_t *array, const void *data, unsigned int len)
{
    c_array_priv_t *priv = (c_array_priv_t *)array;

    c_return_val_if_fail(array != NULL, NULL);

    ensure_capacity(priv,
                    priv->array.len + len + (priv->zero_terminated ? 1 : 0));

    memmove(
        element_offset(priv, priv->array.len), data, element_length(priv, len));

    priv->array.len += len;

    if (priv->zero_terminated) {
        memset(element_offset(priv, priv->array.len), 0, priv->element_size);
    }

    return array;
}

c_array_t *
c_array_insert_vals(c_array_t *array,
                    unsigned int index_,
                    const void *data,
                    unsigned int len)
{
    c_array_priv_t *priv = (c_array_priv_t *)array;
    unsigned int extra = (priv->zero_terminated ? 1 : 0);

    c_return_val_if_fail(array != NULL, NULL);

    ensure_capacity(priv, array->len + len + extra);

    /* first move the existing elements out of the way */
    memmove(element_offset(priv, index_ + len),
            element_offset(priv, index_),
            element_length(priv, array->len - index_));

    /* then copy the new elements into the array */
    memmove(element_offset(priv, index_), data, element_length(priv, len));

    array->len += len;

    if (priv->zero_terminated) {
        memset(element_offset(priv, priv->array.len), 0, priv->element_size);
    }

    return array;
}

c_array_t *
c_array_remove_index(c_array_t *array, unsigned int index_)
{
    c_array_priv_t *priv = (c_array_priv_t *)array;

    c_return_val_if_fail(array != NULL, NULL);

    memmove(element_offset(priv, index_),
            element_offset(priv, index_ + 1),
            element_length(priv, array->len - index_));

    array->len--;

    if (priv->zero_terminated) {
        memset(element_offset(priv, priv->array.len), 0, priv->element_size);
    }

    return array;
}

c_array_t *
c_array_remove_index_fast(c_array_t *array, unsigned int index_)
{
    c_array_priv_t *priv = (c_array_priv_t *)array;

    c_return_val_if_fail(array != NULL, NULL);

    memmove(element_offset(priv, index_),
            element_offset(priv, array->len - 1),
            element_length(priv, 1));

    array->len--;

    if (priv->zero_terminated) {
        memset(element_offset(priv, priv->array.len), 0, priv->element_size);
    }

    return array;
}

c_array_t *
c_array_set_size(c_array_t *array, unsigned int length)
{
    c_array_priv_t *priv = (c_array_priv_t *)array;

    c_return_val_if_fail(array != NULL, NULL);
    c_return_val_if_fail(length >= 0, NULL);

    if (length > priv->capacity) {
        // grow the array
        ensure_capacity(priv, length);
    }

    array->len = length;

    return array;
}

unsigned int
c_array_get_element_size(c_array_t *array)
{
    c_array_priv_t *priv = (c_array_priv_t *)array;
    return priv->element_size;
}

void
c_array_sort(c_array_t *array, c_compare_func_t compare)
{
    c_array_priv_t *priv = (c_array_priv_t *)array;

    qsort(array->data, array->len, priv->element_size, compare);
}
