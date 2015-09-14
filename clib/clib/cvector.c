/*
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
 */

#include "clib-config.h"

#include "cvector.h"

#include <clib.h>
#include <math.h>
#include <string.h>

#define X 0
#define Y 1
#define Z 2
#define W 3

void
c_vector3_init(float *vector, float x, float y, float z)
{
    vector[X] = x;
    vector[Y] = y;
    vector[Z] = z;
}

void
c_vector3_init_zero(float *vector)
{
    memset(vector, 0, sizeof(float) * 3);
}

bool
c_vector3_equal(const void *v1, const void *v2)
{
    float *vector0 = (float *)v1;
    float *vector1 = (float *)v2;

    c_return_val_if_fail(v1 != NULL, false);
    c_return_val_if_fail(v2 != NULL, false);

    /* There's no point picking an arbitrary epsilon that's appropriate
     * for comparing the components so we just use == that will at least
     * consider -0 and 0 to be equal. */
    return vector0[X] == vector1[X] && vector0[Y] == vector1[Y] &&
           vector0[Z] == vector1[Z];
}

bool
c_vector3_equal_with_epsilon(const float *vector0,
                              const float *vector1,
                              float epsilon)
{
    c_return_val_if_fail(vector0 != NULL, false);
    c_return_val_if_fail(vector1 != NULL, false);

    if (fabsf(vector0[X] - vector1[X]) < epsilon &&
        fabsf(vector0[Y] - vector1[Y]) < epsilon &&
        fabsf(vector0[Z] - vector1[Z]) < epsilon)
        return true;
    else
        return false;
}

float *
c_vector3_copy(const float *vector)
{
    if (vector)
        return c_slice_copy(sizeof(float) * 3, vector);
    return NULL;
}

void
c_vector3_free(float *vector)
{
    c_slice_free1(sizeof(float) * 3, vector);
}

void
c_vector3_invert(float *vector)
{
    vector[X] = -vector[X];
    vector[Y] = -vector[Y];
    vector[Z] = -vector[Z];
}

void
c_vector3_add(float *result, const float *a, const float *b)
{
    result[X] = a[X] + b[X];
    result[Y] = a[Y] + b[Y];
    result[Z] = a[Z] + b[Z];
}

void
c_vector3_subtract(float *result, const float *a, const float *b)
{
    result[X] = a[X] - b[X];
    result[Y] = a[Y] - b[Y];
    result[Z] = a[Z] - b[Z];
}

void
c_vector3_multiply_scalar(float *vector, float scalar)
{
    vector[X] *= scalar;
    vector[Y] *= scalar;
    vector[Z] *= scalar;
}

void
c_vector3_divide_scalar(float *vector, float scalar)
{
    float one_over_scalar = 1.0f / scalar;
    vector[X] *= one_over_scalar;
    vector[Y] *= one_over_scalar;
    vector[Z] *= one_over_scalar;
}

void
c_vector3_normalize(float *vector)
{
    float mag_squared =
        vector[X] * vector[X] + vector[Y] * vector[Y] + vector[Z] * vector[Z];

    if (mag_squared > 0.0f) {
        float one_over_mag = 1.0f / sqrtf(mag_squared);
        vector[X] *= one_over_mag;
        vector[Y] *= one_over_mag;
        vector[Z] *= one_over_mag;
    }
}

float
c_vector3_magnitude(const float *vector)
{
    return sqrtf(vector[X] * vector[X] + vector[Y] * vector[Y] +
                 vector[Z] * vector[Z]);
}

void
c_vector3_cross_product(float *result, const float *a, const float *b)
{
    float tmp[3];

    tmp[X] = a[Y] * b[Z] - a[Z] * b[Y];
    tmp[Y] = a[Z] * b[X] - a[X] * b[Z];
    tmp[Z] = a[X] * b[Y] - a[Y] * b[X];
    result[X] = tmp[X];
    result[Y] = tmp[Y];
    result[Z] = tmp[Z];
}

float
c_vector3_dot_product(const float *a, const float *b)
{
    return a[X] * b[X] + a[Y] * b[Y] + a[Z] * b[Z];
}

float
c_vector3_distance(const float *a, const float *b)
{
    float dx = b[X] - a[X];
    float dy = b[Y] - a[Y];
    float dz = b[Z] - a[Z];

    return sqrtf(dx * dx + dy * dy + dz * dz);
}

#if 0
void
c_vector4_init (float *vector, float x, float y, float z)
{
    vector[X] = x;
    vector[Y] = y;
    vector[Z] = z;
    vector[W] = w;
}

void
c_vector4_init_zero (float *vector)
{
    memset (vector, 0, sizeof (float) * 4);
}

bool
c_vector4_equal (const void *v0, const void *v1)
{
    c_return_val_if_fail (v1 != NULL, false);
    c_return_val_if_fail (v2 != NULL, false);

    return memcmp (v1, v2, sizeof (float) * 4) == 0 ? true : false;
}

float *
c_vector4_copy (float *vector)
{
    if (vector)
        return c_slice_copy(sizeof(float) * 4, vector);
    return NULL;
}

void
c_vector4_free (float *vector)
{
    c_slice_free1(sizeof(float) * 4, vector);
}

void
c_vector4_invert (float *vector)
{
    vector.x = -vector.x;
    vector.y = -vector.y;
    vector.z = -vector.z;
    vector.w = -vector.w;
}

void
c_vector4_add (float *result,
                float *a,
                float *b)
{
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    result.w = a.w + b.w;
}

void
c_vector4_subtract (float *result,
                     float *a,
                     float *b)
{
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    result.w = a.w - b.w;
}

void
c_vector4_divide (float *vector,
                   float scalar)
{
    float one_over_scalar = 1.0f / scalar;
    result.x *= one_over_scalar;
    result.y *= one_over_scalar;
    result.z *= one_over_scalar;
    result.w *= one_over_scalar;
}

#endif
