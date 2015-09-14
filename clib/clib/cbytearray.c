/*
 * Arrays
 *
 * Author:
 *   Ueoff Norton  (gnorton@novell.com)
 *
 * (C) 2010 Novell, Inc.
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

c_byte_array_t *
c_byte_array_new()
{
    return (c_byte_array_t *)c_array_new(false, true, 1);
}

uint8_t *
c_byte_array_free(c_byte_array_t *array, bool free_segment)
{
    return (uint8_t *)c_array_free((c_array_t *)array, free_segment);
}

c_byte_array_t *
c_byte_array_append(c_byte_array_t *array,
                    const uint8_t *data,
                    unsigned int len)
{
    return (c_byte_array_t *)c_array_append_vals((c_array_t *)array, data, len);
}

c_byte_array_t *
c_byte_array_set_size(c_byte_array_t *array, unsigned int len)
{
    c_array_set_size((c_array_t *)array, len);
    return array;
}
