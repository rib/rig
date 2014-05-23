/*
 * Pointer Array
 *
 * Author:
 *   Aaron Bockover (abockover@novell.com)
 *   Gonzalo Paniagua Javier (gonzalo@novell.com)
 *   Jeffrey Stedfast (fejj@novell.com)
 *
 * (C) 2006,2011 Novell, Inc.
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
#include <clib.h>

typedef struct _UPtrArrayPriv {
	void * *pdata;
	unsigned int len;
	unsigned int size;
        CDestroyNotify element_free_func;
} UPtrArrayPriv;

static void 
c_ptr_array_grow(UPtrArrayPriv *array, unsigned int length)
{
	unsigned int new_length = array->len + length;

	c_return_if_fail(array != NULL);

	if(new_length <= array->size) {
		return;
	}

	array->size = 1;

	while(array->size < new_length) {
		array->size <<= 1;
	}

	array->size = MAX(array->size, 16);
	array->pdata = c_realloc(array->pdata, array->size * sizeof(void *));
}

UPtrArray *
c_ptr_array_new(void)
{
	return c_ptr_array_sized_new(0);
}

UPtrArray *
c_ptr_array_sized_new(unsigned int reserved_size)
{
	UPtrArrayPriv *array = c_new0(UPtrArrayPriv, 1);

	array->pdata = NULL;
	array->len = 0;
	array->size = 0;

	if(reserved_size > 0) {
		c_ptr_array_grow(array, reserved_size);
	}

	return (UPtrArray *)array;
}

UPtrArray *
c_ptr_array_new_with_free_func(CDestroyNotify element_free_func)
{
  UPtrArrayPriv *array = (UPtrArrayPriv *)c_ptr_array_sized_new (0);
  array->element_free_func = element_free_func;
  return (UPtrArray *)array;
}

void * *
c_ptr_array_free(UPtrArray *array, cboolean free_seg)
{
	void * *data = NULL;
	
	c_return_val_if_fail(array != NULL, NULL);

	if(free_seg) {
                UPtrArrayPriv *priv = (UPtrArrayPriv *)array;
                void * *pdata = priv->pdata;

                if (priv->element_free_func) {
                        CDestroyNotify free_func = priv->element_free_func;
                        int i;

                        for (i = priv->len - 1; i > 0; i--)
                                free_func (pdata[i]);
                }
		c_free(pdata);
	} else {
		data = array->pdata;
	}

	c_free(array);
	
	return data;
}

void
c_ptr_array_set_size(UPtrArray *array, int length)
{
	c_return_if_fail(array != NULL);

	if((size_t)length > array->len) {
		c_ptr_array_grow((UPtrArrayPriv *)array, length);
		memset(array->pdata + array->len, 0, (length - array->len) 
			* sizeof(void *));
	}

	array->len = length;
}

void
c_ptr_array_add(UPtrArray *array, void * data)
{
	c_return_if_fail(array != NULL);
	c_ptr_array_grow((UPtrArrayPriv *)array, 1);
	array->pdata[array->len++] = data;
}

void *
c_ptr_array_remove_index(UPtrArray *array, unsigned int index)
{
	void * removed_node;
	
	c_return_val_if_fail(array != NULL, NULL);
	c_return_val_if_fail(index >= 0 || index < array->len, NULL);

	removed_node = array->pdata[index];

	if(index != array->len - 1) {
		c_memmove(array->pdata + index, array->pdata + index + 1,
			(array->len - index - 1) * sizeof(void *));
	}
	
	array->len--;
	array->pdata[array->len] = NULL;

	return removed_node;
}

void *
c_ptr_array_remove_index_fast(UPtrArray *array, unsigned int index)
{
	void * removed_node;

	c_return_val_if_fail(array != NULL, NULL);
	c_return_val_if_fail(index >= 0 || index < array->len, NULL);

	removed_node = array->pdata[index];

	if(index != array->len - 1) {
		c_memmove(array->pdata + index, array->pdata + array->len - 1,
			sizeof(void *));
	}

	array->len--;
	array->pdata[array->len] = NULL;

	return removed_node;
}

cboolean
c_ptr_array_remove(UPtrArray *array, void * data)
{
	unsigned int i;

	c_return_val_if_fail(array != NULL, FALSE);

	for(i = 0; i < array->len; i++) {
		if(array->pdata[i] == data) {
			c_ptr_array_remove_index(array, i);
			return TRUE;
		}
	}

	return FALSE;
}

cboolean
c_ptr_array_remove_fast(UPtrArray *array, void * data)
{
	unsigned int i;

	c_return_val_if_fail(array != NULL, FALSE);

	for(i = 0; i < array->len; i++) {
		if(array->pdata[i] == data) {
			array->len--;
			if (array->len > 0)
				array->pdata [i] = array->pdata [array->len];
			else
				array->pdata [i] = NULL;
			return TRUE;
		}
	}

	return FALSE;
}

void 
c_ptr_array_foreach(UPtrArray *array, CFunc func, void * user_data)
{
	unsigned int i;

	for(i = 0; i < array->len; i++) {
		func(c_ptr_array_index(array, i), user_data);
	}
}

void
c_ptr_array_sort(UPtrArray *array, CCompareFunc compare)
{
	c_return_if_fail(array != NULL);
	qsort(array->pdata, array->len, sizeof(void *), compare);
}

void
c_ptr_array_sort_with_data (UPtrArray *array, CCompareDataFunc compare, void * user_data)
{
	c_return_if_fail (array != NULL);
	
	c_qsort_with_data (array->pdata, array->len, sizeof (void *), compare, user_data);
}
