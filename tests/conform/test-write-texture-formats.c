#include <config.h>

#include <cglib/cglib.h>
#include <stdarg.h>

#include "test-cg-fixtures.h"

/*
 * This tests writing data to an RGBA texture in all of the available
 * pixel formats
 */

static void
test_color (cg_texture_t *texture,
            uint32_t expected_pixel)
{
  uint8_t received_pixel[4];

  cg_texture_get_data (texture,
                         CG_PIXEL_FORMAT_RGBA_8888_PRE,
                         4, /* rowstride */
                         received_pixel);

  test_cg_compare_pixel_and_alpha (received_pixel, expected_pixel);
}

static void
test_write_byte (cg_pixel_format_t format,
                 uint8_t byte,
                 uint32_t expected_pixel)
{
  cg_texture_t *texture = test_cg_create_color_texture (test_dev, 0);

  cg_texture_set_region (texture,
                           1, 1, /* width / height */
                           format,
                           1, /* rowstride */
                           &byte,
                           0, 0, /* dst_x, dst_y */
                           0, /* level */
                           NULL); /* don't catch errors */

  test_color (texture, expected_pixel);

  cg_object_unref (texture);
}

static void
test_write_short (cg_pixel_format_t format,
                  uint16_t value,
                  uint32_t expected_pixel)
{
  cg_texture_t *texture = test_cg_create_color_texture (test_dev, 0);

  cg_texture_set_region (texture,
                           1, 1, /* width / height */
                           format,
                           2, /* rowstride */
                           (uint8_t *) &value,
                           0, 0, /* dst_x, dst_y */
                           0, /* level */
                           NULL); /* don't catch errors */

  test_color (texture, expected_pixel);

  cg_object_unref (texture);
}

static void
test_write_bytes (cg_pixel_format_t format,
                  uint32_t value,
                  uint32_t expected_pixel)
{
  cg_texture_t *texture = test_cg_create_color_texture (test_dev, 0);

  value = C_UINT32_TO_BE (value);

  cg_texture_set_region (texture,
                           1, 1, /* width / height */
                           format,
                           4, /* rowstride */
                           (uint8_t *) &value,
                           0, 0, /* dst_x, dst_y */
                           0, /* level */
                           NULL); /* don't catch errors */

  test_color (texture, expected_pixel);

  cg_object_unref (texture);
}

static void
test_write_int (cg_pixel_format_t format,
                uint32_t expected_pixel,
                ...)
{
  va_list ap;
  int bits;
  uint32_t tex_data = 0;
  int bits_sum = 0;
  cg_texture_t *texture = test_cg_create_color_texture (test_dev, 0);

  va_start (ap, expected_pixel);

  /* Convert the va args into a single 32-bit value */
  while ((bits = va_arg (ap, int)) != -1)
    {
      uint32_t value = (va_arg (ap, int) * ((1 << bits) - 1) + 127) / 255;

      bits_sum += bits;

      tex_data |= value << (32 - bits_sum);
    }

  va_end (ap);

  cg_texture_set_region (texture,
                           1, 1, /* width / height */
                           format,
                           4, /* rowstride */
                           (uint8_t *) &tex_data,
                           0, 0, /* dst_x, dst_y */
                           0, /* level */
                           NULL); /* don't catch errors */

  test_color (texture, expected_pixel);

  cg_object_unref (texture);
}

void
test_write_texture_formats (void)
{
  test_write_byte (CG_PIXEL_FORMAT_A_8, 0x34, 0x00000034);

  /* We should always be able to read from an RG buffer regardless of
   * whether RG textures are supported because Cogl will do the
   * conversion for us */
  test_write_bytes (CG_PIXEL_FORMAT_RG_88, 0x123456ff, 0x123400ff);

  test_write_short (CG_PIXEL_FORMAT_RGB_565, 0x0843, 0x080819ff);
  test_write_short (CG_PIXEL_FORMAT_RGBA_4444_PRE, 0x1234, 0x11223344);
  test_write_short (CG_PIXEL_FORMAT_RGBA_5551_PRE, 0x0887, 0x081019ff);

  test_write_bytes (CG_PIXEL_FORMAT_RGB_888, 0x123456ff, 0x123456ff);
  test_write_bytes (CG_PIXEL_FORMAT_BGR_888, 0x563412ff, 0x123456ff);

  test_write_bytes (CG_PIXEL_FORMAT_RGBA_8888_PRE, 0x12345678, 0x12345678);
  test_write_bytes (CG_PIXEL_FORMAT_BGRA_8888_PRE, 0x56341278, 0x12345678);
  test_write_bytes (CG_PIXEL_FORMAT_ARGB_8888_PRE, 0x78123456, 0x12345678);
  test_write_bytes (CG_PIXEL_FORMAT_ABGR_8888_PRE, 0x78563412, 0x12345678);

  test_write_int (CG_PIXEL_FORMAT_RGBA_1010102_PRE,
                  0x123456ff,
                  10, 0x12, 10, 0x34, 10, 0x56, 2, 0xff,
                  -1);
  test_write_int (CG_PIXEL_FORMAT_BGRA_1010102_PRE,
                  0x123456ff,
                  10, 0x56, 10, 0x34, 10, 0x12, 2, 0xff,
                  -1);
  test_write_int (CG_PIXEL_FORMAT_ARGB_2101010_PRE,
                  0x123456ff,
                  2, 0xff, 10, 0x12, 10, 0x34, 10, 0x56,
                  -1);
  test_write_int (CG_PIXEL_FORMAT_ABGR_2101010_PRE,
                  0x123456ff,
                  2, 0xff, 10, 0x56, 10, 0x34, 10, 0x12,
                  -1);

  if (test_verbose ())
    c_print ("OK\n");
}
