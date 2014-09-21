/*
 * Copyright © 2013 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"

#include <math.h>

#include <clib.h>

#include "fuzzy.h"

#define IRWIN_HALL_SUM_LEN 12

float fuzzy_float_get_real_value(struct fuzzy_float *variance,
                                 c_rand_t *rand)
{
    switch (variance->type) {
    case FLOAT_VARIANCE_LINEAR:
    {
        float v = variance->variance / 2;
        return c_rand_float_range(rand,
                                  variance->value - v,
                                  variance->value + v);
    }
    case FLOAT_VARIANCE_PROPORTIONAL:
    {
        float v = variance->value * variance->variance;
        return c_rand_float_range(rand,
                                  variance->value - v,
                                  variance->value + v);
    }
    case FLOAT_VARIANCE_IRWIN_HALL:
    {
        float v = variance->variance / 2;
        float f = 0;
        int i;

        for (i = 0; i < IRWIN_HALL_SUM_LEN; i++)
            f += (float)c_rand_float_range(rand,
                                           variance->value - v,
                                           variance->value + v);

        return f / 12.0f;
    }
    case FLOAT_VARIANCE_NONE:
    default:
        return variance->value;
    }
}

double fuzzy_double_get_real_value(struct fuzzy_double *variance,
                                   c_rand_t *rand)
{
    switch (variance->type) {
    case DOUBLE_VARIANCE_LINEAR:
    {
        double v = variance->variance / 2;
        return c_rand_double_range(rand,
                                   variance->value - v,
                                   variance->value + v);
    }
    case DOUBLE_VARIANCE_PROPORTIONAL:
    {
        double v = variance->value * variance->variance;
        return c_rand_double_range(rand,
                                   variance->value - v,
                                   variance->value + v);
    }
    case DOUBLE_VARIANCE_NONE:
    default:
        return variance->value;
    }
}

void fuzzy_vector_get_real_value(struct fuzzy_vector *variance,
                                 c_rand_t *rand, float *value)
{
    unsigned int i;

    switch (variance->type) {

    case VECTOR_VARIANCE_LINEAR:
        for (i = 0; i < C_N_ELEMENTS(variance->value); i++) {
            float v = variance->variance[i] / 2;

            value[i] = c_rand_float_range(rand,
                                          variance->value[i] - v,
                                          variance->value[i] + v);
        }
        break;
    case VECTOR_VARIANCE_PROPORTIONAL:
        for (i = 0; i < C_N_ELEMENTS(variance->value); i++) {
            float v = variance->value[i] * variance->variance[i];

            value[i] = c_rand_float_range(rand,
                                          variance->value[i] - v,
                                          variance->value[i] + v);
        }
        break;
    case VECTOR_VARIANCE_IRWIN_HALL:
        for(i = 0; i < C_N_ELEMENTS(variance->value); i++) {
            float v = variance->variance[i] / 2;
            float f = 0;
            int j;

            for (j = 0; j < IRWIN_HALL_SUM_LEN; j++) {
                f += c_rand_float_range(rand,
                                        variance->value[i] - v,
                                        variance->value[i] + v);
            }

            value[i] = f / 12.0f;
        }
        break;
    case VECTOR_VARIANCE_NONE:
    default:
        for (i = 0; i < C_N_ELEMENTS(variance->value); i++)
            value[i] = variance->value[i];
        break;
    }
}

void fuzzy_color_get_real_value(struct fuzzy_color *fuzzy_color, c_rand_t *rand,
                                float *hue, float *saturation, float *luminance)
{
    *hue = fmodf(fuzzy_float_get_real_value(&fuzzy_color->hue, rand), 360.0f);
    *saturation = CLAMP(fuzzy_float_get_real_value(&fuzzy_color->saturation, rand), 0.0f, 1.0f);
    *luminance = CLAMP(fuzzy_float_get_real_value(&fuzzy_color->luminance, rand), 0.0f, 1.0f);
}

void fuzzy_color_get_cg_color(struct fuzzy_color *fuzzy_color, c_rand_t *rand,
                              cg_color_t *color)
{
    float hue, saturation, luminance;

    fuzzy_color_get_real_value(fuzzy_color, rand,
                               &hue, &saturation, &luminance);

    cg_color_init_from_hsl(color, hue, saturation, luminance);
}
