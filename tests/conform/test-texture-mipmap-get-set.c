#include <config.h>

#include <cglib/cglib.h>

#include <string.h>

#include "test-cg-fixtures.h"

#define TEXTURE_SIZE  128

static cg_texture_t *
make_texture (void)
{
  uint8_t *p, *tex_data;
  cg_texture_t *tex;
  int x, y;

  p = tex_data = c_malloc (TEXTURE_SIZE * TEXTURE_SIZE * 4);
  for (y = 0; y < TEXTURE_SIZE; y++)
    {
      for (x = 0; x < TEXTURE_SIZE; x++)
        {
          p[0] = 0xff;
          p[1] = 0;
          p[2] = 0;
          p[3] = 0xff;
          p += 4;
        }
    }

  tex = test_cg_texture_new_from_data (test_dev,
                                          TEXTURE_SIZE,
                                          TEXTURE_SIZE,
                                          TEST_CG_TEXTURE_NO_ATLAS,
                                          CG_PIXEL_FORMAT_RGBA_8888_PRE,
                                          TEXTURE_SIZE * 4,
                                          tex_data);

  cg_primitive_texture_set_auto_mipmap (tex, false);

  c_free (tex_data);

  return tex;
}

static void
update_mipmap_levels (cg_texture_t *tex)
{
  uint8_t *p, *tex_data;
  int x, y;

  p = tex_data = c_malloc ((TEXTURE_SIZE / 2) * (TEXTURE_SIZE / 2) * 4);

  for (y = 0; y < TEXTURE_SIZE / 2; y++)
    {
      for (x = 0; x < TEXTURE_SIZE / 2; x++)
        {
          p[0] = 0;
          p[1] = 0xff;
          p[2] = 0;
          p[3] = 0xff;
          p += 4;
        }
    }

  cg_texture_set_region (tex,
                           TEXTURE_SIZE / 2,
                           TEXTURE_SIZE / 2,
                           CG_PIXEL_FORMAT_RGBA_8888_PRE,
                           0, /* auto rowstride */
                           tex_data,
                           0, /* dest x */
                           0, /* dest y */
                           1, /* mipmap level */
                           NULL);
  p = tex_data;
  for (y = 0; y < TEXTURE_SIZE / 4; y++)
    {
      for (x = 0; x < TEXTURE_SIZE / 4; x++)
        {
          p[0] = 0;
          p[1] = 0;
          p[2] = 0xff;
          p[3] = 0xff;
          p += 4;
        }
    }

  cg_texture_set_region (tex,
                           TEXTURE_SIZE / 4,
                           TEXTURE_SIZE / 4,
                           CG_PIXEL_FORMAT_RGBA_8888_PRE,
                           0, /* auto rowstride */
                           tex_data,
                           0, /* dest x */
                           0, /* dest y */
                           2, /* mipmap level */
                           NULL);
  c_free (tex_data);
}

static void
validate_results (void)
{
  test_cg_check_pixel (test_fb,
                          TEXTURE_SIZE / 2,
                          TEXTURE_SIZE / 2,
                          0xff0000ff);
  test_cg_check_pixel (test_fb,
                          TEXTURE_SIZE + TEXTURE_SIZE / 4,
                          TEXTURE_SIZE / 4,
                          0x00ff00ff);
  test_cg_check_pixel (test_fb,
                          TEXTURE_SIZE + TEXTURE_SIZE / 2 + TEXTURE_SIZE / 8,
                          TEXTURE_SIZE / 8,
                          0x0000ffff);
}

static void
paint (cg_texture_t *texture)
{
  cg_pipeline_t *pipeline = cg_pipeline_new (test_dev);
  int x = 0, y = 0, size = TEXTURE_SIZE;

  cg_pipeline_set_layer_texture (pipeline, 0, texture);
  cg_pipeline_set_layer_filters (pipeline, 0,
                                   CG_PIPELINE_FILTER_NEAREST_MIPMAP_NEAREST,
                                   CG_PIPELINE_FILTER_NEAREST);

  cg_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   x, y,
                                   x + size,
                                   y + size);

  x += size;
  size /= 2;
  cg_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   x, y,
                                   x + size,
                                   y + size);

  x += size;
  size /= 2;
  cg_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   x, y,
                                   x + size,
                                   y + size);

  cg_object_unref (pipeline);
  cg_object_unref (texture);
}

void
test_texture_mipmap_get_set (void)
{
  cg_texture_t *texture = make_texture ();

  cg_framebuffer_orthographic (test_fb,
                                 0, 0,
                                 cg_framebuffer_get_width (test_fb),
                                 cg_framebuffer_get_height (test_fb),
                                 -1,
                                 100);

  update_mipmap_levels (texture);
  paint (texture);

  validate_results ();

  if (test_verbose ())
    c_print ("OK\n");
}

