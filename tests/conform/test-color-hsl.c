#include <config.h>

#include <math.h>
#include <string.h>

#include <cglib/cglib.h>

#include "test-cg-fixtures.h"

#define cc_assert_float(a, b)         \
  do {                                  \
    if (fabsf ((a) - (b)) >= 0.0001f)   \
      c_assert_cmpfloat ((a), ==, (b)); \
  } while (0)

void
test_color_hsl (void)
{
  cg_color_t color;
  float hue, saturation, luminance;

  cg_color_init_from_4ub(&color, 108, 198, 78, 255);
  cg_color_to_hsl(&color, &hue, &saturation, &luminance);

  cc_assert_float(hue, 105.f);
  cc_assert_float(saturation, 0.512821);
  cc_assert_float(luminance, 0.541176);

  memset(&color, 0, sizeof (cg_color_t));
  cg_color_init_from_hsl(&color, hue, saturation, luminance);

  cc_assert_float(color.red, 108.0f / 255.0f);
  cc_assert_float(color.green, 198.0f / 255.0f);
  cc_assert_float(color.blue, 78.0f / 255.0f);
  cc_assert_float(color.alpha, 1.0f);

  memset(&color, 0, sizeof (cg_color_t));
  cg_color_init_from_hsl(&color, hue, 0, luminance);

  cc_assert_float(color.red, luminance);
  cc_assert_float(color.green, luminance);
  cc_assert_float(color.blue, luminance);
  cc_assert_float(color.alpha, 1.0f);

  if (test_verbose ())
    c_print ("OK\n");
}
