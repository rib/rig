#include <config.h>

#include <cglib/cglib.h>
#include <string.h>

#include "test-cg-fixtures.h"

static cg_texture_2d_t *
create_texture (cg_device_t *dev)
{
  static const uint8_t data[] =
    {
      0xff, 0x00, 0x00, 0xff,
      0x00, 0xfa, 0x00, 0xfa
    };

  return cg_texture_2d_new_from_data (dev,
                                        2, 1, /* width/height */
                                        CG_PIXEL_FORMAT_RGBA_8888_PRE,
                                        4, /* rowstride */
                                        data,
                                        NULL /* error */);
}

void
test_alpha_test (void)
{
  cg_texture_t *tex = create_texture (test_dev);
  cg_pipeline_t *pipeline = cg_pipeline_new (test_dev);
  int fb_width = cg_framebuffer_get_width (test_fb);
  int fb_height = cg_framebuffer_get_height (test_fb);
  cg_color_t clear_color;

  cg_pipeline_set_layer_texture (pipeline, 0, tex);
  cg_pipeline_set_layer_filters (pipeline, 0,
                                   CG_PIPELINE_FILTER_NEAREST,
                                   CG_PIPELINE_FILTER_NEAREST);
  cg_pipeline_set_alpha_test_function (pipeline,
                                         CG_PIPELINE_ALPHA_FUNC_GEQUAL,
                                         254 / 255.0f /* alpha reference */);

  cg_color_init_from_4ub (&clear_color, 0x00, 0x00, 0xff, 0xff);
  cg_framebuffer_clear (test_fb,
                          CG_BUFFER_BIT_COLOR,
                          &clear_color);

  cg_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   -1, -1,
                                   1, 1);

  cg_object_unref (pipeline);
  cg_object_unref (tex);

  /* The left side of the framebuffer should use the first pixel from
   * the texture which is red */
  test_cg_check_region (test_fb,
                           2, 2,
                           fb_width / 2 - 4,
                           fb_height - 4,
                           0xff0000ff);
  /* The right side of the framebuffer should use the clear color
   * because the second pixel from the texture is clipped from the
   * alpha test */
  test_cg_check_region (test_fb,
                           fb_width / 2 + 2,
                           2,
                           fb_width / 2 - 4,
                           fb_height - 4,
                           0x0000ffff);

  if (test_verbose ())
    c_print ("OK\n");
}

