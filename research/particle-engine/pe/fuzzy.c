#include "fuzzy.h"

#include <math.h>

float fuzzy_float_get_real_value(struct fuzzy_float *variance,
				 GRand *rand)
{
	switch (variance->type) {

	case FLOAT_VARIANCE_LINEAR:
	{
		float v = variance->variance / 2;
		return (float)g_rand_double_range(rand,
						  variance->value - v,
						  variance->value + v);
	}
	case FLOAT_VARIANCE_PROPORTIONAL:
	{
		float v = variance->value * variance->variance;
		return (float)g_rand_double_range(rand,
						  variance->value - v,
						  variance->value + v);
	}
	case FLOAT_VARIANCE_IRWIN_HALL:
	{
		int i, n = 12;
		float u = 0, v;

		for (i = 0; i < n; i++)
			u += (float)g_rand_double_range(rand, 0.0f, 1.0f);

		u -= n / 2;
		u /= n / 2;

		v = variance->variance * u;
		return (float)g_rand_double_range(rand,
						  variance->value - v,
						  variance->value + v);
	}
	case FLOAT_VARIANCE_NONE:
	default:
		return variance->value;
	}
}

gdouble fuzzy_double_get_real_value(struct fuzzy_double *variance,
				    GRand *rand)
{
	switch (variance->type) {

	case DOUBLE_VARIANCE_LINEAR:
	{
		gdouble v = variance->variance / 2;
		return g_rand_double_range(rand,
					   variance->value - v,
					   variance->value + v);
	}
	case DOUBLE_VARIANCE_PROPORTIONAL:
	{
		gdouble v = variance->value * variance->variance;
		return g_rand_double_range(rand,
					   variance->value - v,
					   variance->value + v);
	}
	case DOUBLE_VARIANCE_NONE:
	default:
		return variance->value;
	}
}

void fuzzy_vector_get_real_value(struct fuzzy_vector *variance,
				 GRand *rand, float *value)
{
	unsigned int i;

	switch (variance->type) {

	case VECTOR_VARIANCE_LINEAR:
		for (i = 0; i < G_N_ELEMENTS(variance->value); i++) {
			float v = variance->variance[i] / 2;

			value[i] = (float)g_rand_double_range(rand,
							      variance->value[i] - v,
							      variance->value[i] + v);
		}
		break;
	case VECTOR_VARIANCE_PROPORTIONAL:
		for (i = 0; i < G_N_ELEMENTS(variance->value); i++) {
			float v = variance->value[i] * variance->variance[i];

			value[i] = (float)g_rand_double_range(rand,
							      variance->value[i] - v,
							      variance->value[i] + v);
		}
		break;
	case VECTOR_VARIANCE_IRWIN_HALL:
		for(i = 0; i < G_N_ELEMENTS(variance->value); i++) {
			int j, n = 12;
			float u = 0, v;

			for (j = 0; j < n; j++)
				u += (float)g_rand_double_range(rand, 0.0f, 1.0f);

			u -= n / 2;
			u /= n / 2;

			v = variance->variance[i] * u;
			value[i]= (float)g_rand_double_range(rand,
							     variance->value[i] - v,
							     variance->value[i] + v);
		}
		break;
	case VECTOR_VARIANCE_NONE:
	default:
		for (i = 0; i < G_N_ELEMENTS(variance->value); i++)
			value[i] = variance->value[i];
		break;
	}
}

void fuzzy_color_get_real_value(struct fuzzy_color *fuzzy_color, GRand *rand,
				float *hue, float *saturation, float *luminance)
{
	*hue = fmodf(fuzzy_float_get_real_value(&fuzzy_color->hue, rand), 360.0f);
	*saturation = CLAMP(fuzzy_float_get_real_value(&fuzzy_color->saturation, rand), 0.0f, 1.0f);
	*luminance = CLAMP(fuzzy_float_get_real_value(&fuzzy_color->luminance, rand), 0.0f, 1.0f);
}

void fuzzy_color_get_cogl_color(struct fuzzy_color *fuzzy_color, GRand *rand,
				CoglColor *color)
{
	float hue, saturation, luminance;

	fuzzy_color_get_real_value(fuzzy_color, rand,
				   &hue, &saturation, &luminance);

	cogl_color_init_from_hsl(color, hue, saturation, luminance);
}
