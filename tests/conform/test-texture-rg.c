#include <config.h>

#include <cglib/cglib.h>

#include <string.h>

#include "test-cg-fixtures.h"

#define TEX_WIDTH 8
#define TEX_HEIGHT 8

static cg_texture_2d_t *
make_texture (void)
{
  uint8_t tex_data[TEX_WIDTH * TEX_HEIGHT * 2], *p = tex_data;
  int x, y;

  for (y = 0; y < TEX_HEIGHT; y++)
    for (x = 0; x < TEX_WIDTH; x++)
      {
        *(p++) = x * 256 / TEX_WIDTH;
        *(p++) = y * 256 / TEX_HEIGHT;
      }

  return cg_texture_2d_new_from_data (test_dev,
                                        TEX_WIDTH, TEX_HEIGHT,
                                        CG_PIXEL_FORMAT_RG_88,
                                        TEX_WIDTH * 2,
                                        tex_data,
                                        NULL);
}

void
test_texture_rg (void)
{
  cg_pipeline_t *pipeline;
  cg_texture_2d_t *tex;
  int fb_width, fb_height;
  int x, y;

  fb_width = cg_framebuffer_get_width (test_fb);
  fb_height = cg_framebuffer_get_height (test_fb);

  tex = make_texture ();

  c_assert (cg_texture_get_components (tex) == CG_TEXTURE_COMPONENTS_RG);

  pipeline = cg_pipeline_new (test_dev);

  cg_pipeline_set_layer_texture (pipeline, 0, tex);
  cg_pipeline_set_layer_filters (pipeline,
                                   0,
                                   CG_PIPELINE_FILTER_NEAREST,
                                   CG_PIPELINE_FILTER_NEAREST);

  cg_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   -1.0f, 1.0f,
                                   1.0f, -1.0f);

  for (y = 0; y < TEX_HEIGHT; y++)
    for (x = 0; x < TEX_WIDTH; x++)
      {
        test_cg_check_pixel_rgb (test_fb,
                                    x * fb_width / TEX_WIDTH +
                                    fb_width / (TEX_WIDTH * 2),
                                    y * fb_height / TEX_HEIGHT +
                                    fb_height / (TEX_HEIGHT * 2),
                                    x * 256 / TEX_WIDTH,
                                    y * 256 / TEX_HEIGHT,
                                    0);
      }

  cg_object_unref (pipeline);
  cg_object_unref (tex);
}
