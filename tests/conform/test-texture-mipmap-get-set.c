#include <cogl/cogl.h>

#include <string.h>

#include "test-utils.h"

#define TEXTURE_SIZE  128

static CoglTexture *
make_texture (void)
{
  uint8_t *p, *tex_data;
  CoglTexture *tex;
  int x, y;

  p = tex_data = g_malloc (TEXTURE_SIZE * TEXTURE_SIZE * 4);
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

  tex = test_utils_texture_new_from_data (test_ctx,
                                          TEXTURE_SIZE,
                                          TEXTURE_SIZE,
                                          TEST_UTILS_TEXTURE_NO_ATLAS,
                                          COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                          COGL_PIXEL_FORMAT_ANY,
                                          TEXTURE_SIZE * 4,
                                          tex_data);

  cogl_primitive_texture_set_auto_mipmap (COGL_PRIMITIVE_TEXTURE (tex), FALSE);

  g_free (tex_data);

  return tex;
}

static void
update_mipmap_levels (CoglTexture *tex)
{
  uint8_t *p, *tex_data;
  int x, y;

  p = tex_data = g_malloc ((TEXTURE_SIZE / 2) * (TEXTURE_SIZE / 2) * 4);

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

  cogl_texture_set_region (tex,
                           TEXTURE_SIZE / 2,
                           TEXTURE_SIZE / 2,
                           COGL_PIXEL_FORMAT_RGBA_8888_PRE,
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

  cogl_texture_set_region (tex,
                           TEXTURE_SIZE / 4,
                           TEXTURE_SIZE / 4,
                           COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                           0, /* auto rowstride */
                           tex_data,
                           0, /* dest x */
                           0, /* dest y */
                           2, /* mipmap level */
                           NULL);
  g_free (tex_data);
}

static void
validate_results (void)
{
  test_utils_check_pixel (test_fb,
                          TEXTURE_SIZE / 2,
                          TEXTURE_SIZE / 2,
                          0xff0000ff);
  test_utils_check_pixel (test_fb,
                          TEXTURE_SIZE + TEXTURE_SIZE / 4,
                          TEXTURE_SIZE / 4,
                          0x00ff00ff);
  test_utils_check_pixel (test_fb,
                          TEXTURE_SIZE + TEXTURE_SIZE / 2 + TEXTURE_SIZE / 8,
                          TEXTURE_SIZE / 8,
                          0x0000ffff);
}

static void
paint (CoglTexture *texture)
{
  CoglPipeline *pipeline = cogl_pipeline_new (test_ctx);
  int x = 0, y = 0, size = TEXTURE_SIZE;

  cogl_pipeline_set_layer_texture (pipeline, 0, texture);
  cogl_pipeline_set_layer_filters (pipeline, 0,
                                   COGL_PIPELINE_FILTER_NEAREST_MIPMAP_NEAREST,
                                   COGL_PIPELINE_FILTER_NEAREST);

  cogl_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   x, y,
                                   x + size,
                                   y + size);

  x += size;
  size /= 2;
  cogl_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   x, y,
                                   x + size,
                                   y + size);

  x += size;
  size /= 2;
  cogl_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   x, y,
                                   x + size,
                                   y + size);

  cogl_object_unref (pipeline);
  cogl_object_unref (texture);
}

void
test_texture_mipmap_get_set (void)
{
  CoglTexture *texture = make_texture ();

  cogl_framebuffer_orthographic (test_fb,
                                 0, 0,
                                 cogl_framebuffer_get_width (test_fb),
                                 cogl_framebuffer_get_height (test_fb),
                                 -1,
                                 100);

  update_mipmap_levels (texture);
  paint (texture);

  validate_results ();

  if (cogl_test_verbose ())
    g_print ("OK\n");
}

