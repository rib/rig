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

/*
 *         fuzzy.h -- Support for approximate values.
 *
 * A fuzzy value consists of three properties:
 *
 *      1) Value - this is the target/mean/base/ideal value to which fuzziness
 *         is applied.
 *
 *      2) Variance - this is the amount of variance that is to applied to the
 *         target value.
 *
 *      3) Type - this controls the type of fuzziness that is applied to a
 *         value. Examples include:
 *
 *               LINEAR       The possible values are linearly spread across a
 *                            range centred around the value, and the size of
 *                            which is determined by the variance. For example,
 *                            with a value of 100 and a variance of 50, the
 *                            range of possible values is linearly distributed
 *                            from [75, 125] (100 ± 25).
 *
 *               PROPORTIONAL Similar to LINEAR, but the range of values is
 *                            expressed as a proportion of the target value,
 *                            like a traditional percentage error. For example,
 *                            with a value of 100 and a variance of 0.5, the
 *                            range of possible values is linearly distributed
 *                            from [50, 150] (100 ± 50%).
 *
 *               IRWIN HALL   This is a fast computation to roughly approximate
 *                            a normal distribution, so the random values will
 *                            be normally distributed across the variance range.
 *
 *               NONE         No fuzziness, this represents a real and
 *                            deterministic value.
 *
 * Each fuzzy type has an associated method which can be used for obtaining real
 * values:
 *
 *    fuzzy_<type>_get_real_value(<type> variance, rand, [out-values])
 *
 * If you would like reproducible fuzziness, then use a c_rand_t with a known seed.
 */
#ifndef _FUZZY_H
#define _FUZZY_H

#include <cglib/cglib.h>
#include <clib.h>

struct fuzzy_float {
	float value;
	float variance;
	enum {
		FLOAT_VARIANCE_NONE,
		FLOAT_VARIANCE_LINEAR,
		FLOAT_VARIANCE_PROPORTIONAL,
		FLOAT_VARIANCE_IRWIN_HALL
	} type;
};

float fuzzy_float_get_real_value(struct fuzzy_float *variance,
				 c_rand_t *rand);

/*
 * A double precision value.
 */
struct fuzzy_double {
	double value;
	double variance;
	enum {
		DOUBLE_VARIANCE_NONE,
		DOUBLE_VARIANCE_LINEAR,
		DOUBLE_VARIANCE_PROPORTIONAL
	} type;
};

double fuzzy_double_get_real_value(struct fuzzy_double *variance,
				    c_rand_t *rand);

/*
 * A 3D vector, can be used for introducing fuzziness to positions, velocities
 * etc.
 */
struct fuzzy_vector {
	float value[3];
	float variance[3];
	enum {
		VECTOR_VARIANCE_NONE,
		VECTOR_VARIANCE_LINEAR,
		VECTOR_VARIANCE_PROPORTIONAL,
		VECTOR_VARIANCE_IRWIN_HALL
	} type;
};

void fuzzy_vector_get_real_value(struct fuzzy_vector *variance,
				 c_rand_t *rand, float *value);

struct fuzzy_color {
	struct fuzzy_float hue;
	struct fuzzy_float saturation;
	struct fuzzy_float luminance;
};

void fuzzy_color_get_real_value(struct fuzzy_color *fuzzy_color, c_rand_t *rand,
				float *hue, float *saturation, float *luminance);

void fuzzy_color_get_cg_color(struct fuzzy_color *fuzzy_color, c_rand_t *rand,
				cg_color_t *color);

#endif /* _FUZZY_H */
