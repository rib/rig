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

#include <config.h>

#include <stdlib.h>
#include <ulib.h>

#define INITIAL_CAPACITY 16

#define element_offset(p,i) ((p)->array.data + (i) * (p)->element_size)
#define element_length(p,i) ((i) * (p)->element_size)

typedef struct {
	UArray array;
	uboolean clear_;
	unsigned int element_size;
	uboolean zero_terminated;
	unsigned int capacity;
} UArrayPriv;

static void
ensure_capacity (UArrayPriv *priv, unsigned int capacity)
{
	unsigned int new_capacity;
	
	if (capacity <= priv->capacity)
		return;

        for (new_capacity = MAX (INITIAL_CAPACITY, priv->capacity * 2);
             new_capacity < capacity;
             new_capacity *= 2)
          ;
	
	priv->array.data = u_realloc (priv->array.data, element_length (priv, new_capacity));
	
	if (priv->clear_) {
		memset (element_offset (priv, priv->capacity),
			0,
			element_length (priv, new_capacity - priv->capacity));
	}
	
	priv->capacity = new_capacity;
}

UArray *
u_array_new (uboolean zero_terminated,
	     uboolean clear_,
	     unsigned int element_size)
{
	UArrayPriv *rv = u_new0 (UArrayPriv, 1);
	rv->zero_terminated = zero_terminated;
	rv->clear_ = clear_;
	rv->element_size = element_size;

	ensure_capacity (rv, INITIAL_CAPACITY);

	return (UArray*)rv;
}

UArray *
u_array_sized_new (uboolean zero_terminated,
	     uboolean clear_,
	     unsigned int element_size,
		 unsigned int reserved_size)
{
	UArrayPriv *rv = u_new0 (UArrayPriv, 1);
	rv->zero_terminated = zero_terminated;
	rv->clear_ = clear_;
	rv->element_size = element_size;

	ensure_capacity (rv, reserved_size);

	return (UArray*)rv;
}

char*
u_array_free (UArray *array,
	      uboolean free_segment)
{
	char* rv = NULL;

	u_return_val_if_fail (array != NULL, NULL);

	if (free_segment)
		u_free (array->data);
	else
		rv = array->data;

	u_free (array);

	return rv;
}

UArray *
u_array_append_vals (UArray *array,
		     const void * data,
		     unsigned int len)
{
	UArrayPriv *priv = (UArrayPriv*)array;

	u_return_val_if_fail (array != NULL, NULL);

	ensure_capacity (priv, priv->array.len + len + (priv->zero_terminated ? 1 : 0));
  
	memmove (element_offset (priv, priv->array.len),
		 data,
		 element_length (priv, len));

	priv->array.len += len;

	if (priv->zero_terminated) {
		memset (element_offset (priv, priv->array.len),
			0,
			priv->element_size);
	}

	return array;
}

UArray*
u_array_insert_vals (UArray *array,
		     unsigned int index_,
		     const void * data,
		     unsigned int len)
{
	UArrayPriv *priv = (UArrayPriv*)array;
	unsigned int extra = (priv->zero_terminated ? 1 : 0);

	u_return_val_if_fail (array != NULL, NULL);

	ensure_capacity (priv, array->len + len + extra);
  
	/* first move the existing elements out of the way */
	memmove (element_offset (priv, index_ + len),
		 element_offset (priv, index_),
		 element_length (priv, array->len - index_));

	/* then copy the new elements into the array */
	memmove (element_offset (priv, index_),
		 data,
		 element_length (priv, len));

	array->len += len;

	if (priv->zero_terminated) {
		memset (element_offset (priv, priv->array.len),
			0,
			priv->element_size);
	}

	return array;
}

UArray*
u_array_remove_index (UArray *array,
		      unsigned int index_)
{
	UArrayPriv *priv = (UArrayPriv*)array;

	u_return_val_if_fail (array != NULL, NULL);

	memmove (element_offset (priv, index_),
		 element_offset (priv, index_ + 1),
		 element_length (priv, array->len - index_));

	array->len --;

	if (priv->zero_terminated) {
		memset (element_offset (priv, priv->array.len),
			0,
			priv->element_size);
	}

	return array;
}

UArray*
u_array_remove_index_fast (UArray *array,
		      unsigned int index_)
{
	UArrayPriv *priv = (UArrayPriv*)array;

	u_return_val_if_fail (array != NULL, NULL);

	memmove (element_offset (priv, index_),
		 element_offset (priv, array->len - 1),
		 element_length (priv, 1));

	array->len --;

	if (priv->zero_terminated) {
		memset (element_offset (priv, priv->array.len),
			0,
			priv->element_size);
	}

	return array;
}

UArray *
u_array_set_size (UArray *array, int length)
{
	UArrayPriv *priv = (UArrayPriv*)array;

	u_return_val_if_fail (array != NULL, NULL);
	u_return_val_if_fail (length >= 0, NULL);

	if (length > priv->capacity) {
		// grow the array
		ensure_capacity (priv, length);
	}

	array->len = length;

        return array;
}

unsigned int
u_array_get_element_size (UArray *array)
{
  UArrayPriv *priv = (UArrayPriv*)array;
  return priv->element_size;
}

void
u_array_sort (UArray *array, UCompareFunc compare)
{
  UArrayPriv *priv = (UArrayPriv*)array;

  qsort (array->data, array->len, priv->element_size, compare);
}

