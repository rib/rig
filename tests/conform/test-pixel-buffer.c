#include <config.h>

#include <cglib/cglib.h>
#include <string.h>

#include "test-cg-fixtures.h"

#define BITMAP_SIZE 256

/*
 * Creates a 256 x 256 with image data split into four quadrants. The
 * colours of these in reading order will be: blue, green, cyan,
 * red */
static void
generate_bitmap_data (uint8_t *data,
                      int stride)
{
  int y, x;

  for (y = 0; y < BITMAP_SIZE; y++)
    {
      for (x = 0; x < BITMAP_SIZE; x++)
        {
          int color_num = x / (BITMAP_SIZE / 2) + y / (BITMAP_SIZE / 2) * 2 + 1;
          *(data++) = (color_num & 4) ? 255 : 0;
          *(data++) = (color_num & 2) ? 255 : 0;
          *(data++) = (color_num & 1) ? 255 : 0;
          *(data++) = 255;
        }
      data += stride - BITMAP_SIZE * 4;
    }
}

static cg_bitmap_t *
create_bitmap (void)
{
  cg_bitmap_t *bitmap;
  cg_buffer_t *buffer;

  bitmap = cg_bitmap_new_with_size (test_dev,
                                      BITMAP_SIZE,
                                      BITMAP_SIZE,
                                      CG_PIXEL_FORMAT_RGBA_8888);
  buffer = cg_bitmap_get_buffer (bitmap);

  c_assert (cg_is_pixel_buffer (buffer));
  c_assert (cg_is_buffer (buffer));

  cg_buffer_set_update_hint (buffer, CG_BUFFER_UPDATE_HINT_DYNAMIC);
  c_assert_cmpint (cg_buffer_get_update_hint (buffer),
                   ==,
                   CG_BUFFER_UPDATE_HINT_DYNAMIC);

  return bitmap;
}

static cg_bitmap_t *
create_and_fill_bitmap (void)
{
  cg_bitmap_t *bitmap = create_bitmap ();
  cg_buffer_t *buffer = cg_bitmap_get_buffer (bitmap);
  uint8_t *map;
  unsigned int stride;

  stride = cg_bitmap_get_rowstride (bitmap);

  map = cg_buffer_map (buffer,
                         CG_BUFFER_ACCESS_WRITE,
                         CG_BUFFER_MAP_HINT_DISCARD,
                         NULL); /* don't catch errors */
  c_assert (map);

  generate_bitmap_data (map, stride);

  cg_buffer_unmap (buffer);

  return bitmap;
}

static cg_texture_t *
create_texture_from_bitmap (cg_bitmap_t *bitmap)
{
  cg_texture_2d_t *texture;

  texture = cg_texture_2d_new_from_bitmap (bitmap);

  c_assert (texture != NULL);

  return texture;
}

static cg_pipeline_t *
create_pipeline_from_texture (cg_texture_t *texture)
{
  cg_pipeline_t *pipeline = cg_pipeline_new (test_dev);

  cg_pipeline_set_layer_texture (pipeline, 0, texture);
  cg_pipeline_set_layer_filters (pipeline,
                                   0, /* layer_num */
                                   CG_PIPELINE_FILTER_NEAREST,
                                   CG_PIPELINE_FILTER_NEAREST);

  return pipeline;
}

static void
check_colours (uint32_t color0,
               uint32_t color1,
               uint32_t color2,
               uint32_t color3)
{
  int fb_width = cg_framebuffer_get_width (test_fb);
  int fb_height = cg_framebuffer_get_height (test_fb);

  test_cg_check_region (test_fb,
                           1, 1, /* x/y */
                           fb_width / 2 - 2, /* width */
                           fb_height / 2 - 2, /* height */
                           color0);
  test_cg_check_region (test_fb,
                           fb_width / 2 + 1, /* x */
                           1, /* y */
                           fb_width / 2 - 2, /* width */
                           fb_height / 2 - 2, /* height */
                           color1);
  test_cg_check_region (test_fb,
                           1, /* x */
                           fb_height / 2 + 1, /* y */
                           fb_width / 2 - 2, /* width */
                           fb_height / 2 - 2, /* height */
                           color2);
  test_cg_check_region (test_fb,
                           fb_width / 2 + 1, /* x */
                           fb_height / 2 + 1, /* y */
                           fb_width / 2 - 2, /* width */
                           fb_height / 2 - 2, /* height */
                           color3);
}

void
test_pixel_buffer_map (void)
{
  cg_bitmap_t *bitmap = create_and_fill_bitmap ();
  cg_pipeline_t *pipeline;
  cg_texture_t *texture;

  texture = create_texture_from_bitmap (bitmap);
  pipeline = create_pipeline_from_texture (texture);

  cg_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   -1.0f, 1.0f,
                                   1.0f, -1.0f);

  cg_object_unref (bitmap);
  cg_object_unref (texture);
  cg_object_unref (pipeline);

  check_colours (0x0000ffff,
                 0x00ff00ff,
                 0x00ffffff,
                 0xff0000ff);

  if (test_verbose ())
    c_print ("OK\n");
}

void
test_pixel_buffer_set_data (void)
{
  cg_bitmap_t *bitmap = create_bitmap ();
  cg_buffer_t *buffer = cg_bitmap_get_buffer (bitmap);
  cg_pipeline_t *pipeline;
  cg_texture_t *texture;
  uint8_t *data;
  unsigned int stride;

  stride = cg_bitmap_get_rowstride (bitmap);

  data = c_malloc (stride * BITMAP_SIZE);

  generate_bitmap_data (data, stride);

  cg_buffer_set_data (buffer,
                        0, /* offset */
                        data,
                        stride * (BITMAP_SIZE - 1) +
                        BITMAP_SIZE * 4,
                        NULL /* don't catch errors */);

  c_free (data);

  texture = create_texture_from_bitmap (bitmap);
  pipeline = create_pipeline_from_texture (texture);

  cg_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   -1.0f, 1.0f,
                                   1.0f, -1.0f);

  cg_object_unref (bitmap);
  cg_object_unref (texture);
  cg_object_unref (pipeline);

  check_colours (0x0000ffff,
                 0x00ff00ff,
                 0x00ffffff,
                 0xff0000ff);

  if (test_verbose ())
    c_print ("OK\n");
}

static cg_texture_t *
create_white_texture (void)
{
  cg_texture_2d_t *texture;
  uint8_t *data = c_malloc (BITMAP_SIZE * BITMAP_SIZE * 4);

  memset (data, 255, BITMAP_SIZE * BITMAP_SIZE * 4);

  texture = cg_texture_2d_new_from_data (test_dev,
                                           BITMAP_SIZE,
                                           BITMAP_SIZE,
                                           CG_PIXEL_FORMAT_RGBA_8888,
                                           BITMAP_SIZE * 4, /* rowstride */
                                           data,
                                           NULL); /* don't catch errors */

  c_free (data);

  return texture;
}

void
test_pixel_buffer_sub_region (void)
{
  cg_bitmap_t *bitmap = create_and_fill_bitmap ();
  cg_pipeline_t *pipeline;
  cg_texture_t *texture;

  texture = create_white_texture ();

  /* Replace the top-right quadrant of the texture with the red part
   * of the bitmap */
  cg_texture_set_region_from_bitmap (texture,
                                       BITMAP_SIZE / 2, /* src_x */
                                       BITMAP_SIZE / 2, /* src_y */
                                       BITMAP_SIZE / 2, /* width */
                                       BITMAP_SIZE / 2, /* height */
                                       bitmap,
                                       BITMAP_SIZE / 2, /* dst_x */
                                       0, /* dst_y */
                                       0, /* level */
                                       NULL /* don't catch errors */);

  pipeline = create_pipeline_from_texture (texture);

  cg_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   -1.0f, 1.0f,
                                   1.0f, -1.0f);

  cg_object_unref (bitmap);
  cg_object_unref (texture);
  cg_object_unref (pipeline);

  check_colours (0xffffffff,
                 0xff0000ff,
                 0xffffffff,
                 0xffffffff);

  if (test_verbose ())
    c_print ("OK\n");
}
