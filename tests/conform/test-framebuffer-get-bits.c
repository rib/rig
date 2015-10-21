#include <config.h>

#include <cglib/cglib.h>

#include "test-cg-fixtures.h"

void
test_framebuffer_get_bits (void)
{
  cg_texture_2d_t *tex_a =
    cg_texture_2d_new_with_size (test_dev,
                                   16, 16); /* width/height */
  cg_offscreen_t *offscreen_a =
    cg_offscreen_new_with_texture (tex_a);
  cg_framebuffer_t *fb_a = offscreen_a;
  cg_texture_2d_t *tex_rgba =
    cg_texture_2d_new_with_size (test_dev,
                                   16, 16); /* width/height */
  cg_offscreen_t *offscreen_rgba =
    cg_offscreen_new_with_texture (tex_rgba);
  cg_framebuffer_t *fb_rgba = offscreen_rgba;

  cg_texture_set_components (tex_a,
                               CG_TEXTURE_COMPONENTS_A);
  cg_framebuffer_allocate (fb_a, NULL);
  cg_framebuffer_allocate (fb_rgba, NULL);

  c_assert_cmpint (cg_framebuffer_get_red_bits (fb_a), ==, 0);
  c_assert_cmpint (cg_framebuffer_get_green_bits (fb_a), ==, 0);
  c_assert_cmpint (cg_framebuffer_get_blue_bits (fb_a), ==, 0);
  c_assert_cmpint (cg_framebuffer_get_alpha_bits (fb_a), >=, 1);

  c_assert_cmpint (cg_framebuffer_get_red_bits (fb_rgba), >=, 1);
  c_assert_cmpint (cg_framebuffer_get_green_bits (fb_rgba), >=, 1);
  c_assert_cmpint (cg_framebuffer_get_blue_bits (fb_rgba), >=, 1);
  c_assert_cmpint (cg_framebuffer_get_alpha_bits (fb_rgba), >=, 1);

  cg_object_unref (fb_rgba);
  cg_object_unref (tex_rgba);
  cg_object_unref (fb_a);
  cg_object_unref (tex_a);
}
