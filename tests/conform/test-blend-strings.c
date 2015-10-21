#include <config.h>

#include <cglib/cglib.h>

#include <string.h>

#include "test-cg-fixtures.h"

#define QUAD_WIDTH 20

#define RED 0
#define GREEN 1
#define BLUE 2
#define ALPHA 3

#define MASK_RED(COLOR)   ((COLOR & 0xff000000) >> 24)
#define MASK_GREEN(COLOR) ((COLOR & 0xff0000) >> 16)
#define MASK_BLUE(COLOR)  ((COLOR & 0xff00) >> 8)
#define MASK_ALPHA(COLOR) (COLOR & 0xff)

#define BLEND_CONSTANT_UNUSED 0xDEADBEEF

typedef struct _test_state_t
{
  int padding;
} test_state_t;


static void
test_blend(test_state_t *state,
           int x,
           int y,
           uint32_t src_color,
           uint32_t dst_color,
           const char *blend_string,
           uint32_t blend_constant,
           uint32_t expected_result)
{
  /* src color */
  uint8_t Sr = MASK_RED(src_color);
  uint8_t Sg = MASK_GREEN(src_color);
  uint8_t Sb = MASK_BLUE(src_color);
  uint8_t Sa = MASK_ALPHA(src_color);
  /* dest color */
  uint8_t Dr = MASK_RED(dst_color);
  uint8_t Dg = MASK_GREEN(dst_color);
  uint8_t Db = MASK_BLUE(dst_color);
  uint8_t Da = MASK_ALPHA(dst_color);
  /* blend constant - when applicable */
  uint8_t Br = MASK_RED(blend_constant);
  uint8_t Bg = MASK_GREEN(blend_constant);
  uint8_t Bb = MASK_BLUE(blend_constant);
  uint8_t Ba = MASK_ALPHA(blend_constant);
  cg_color_t blend_const_color;

  cg_pipeline_t *pipeline;
  bool status;
  cg_error_t *error = NULL;
  int y_off;
  int x_off;

  /* First write out the destination color without any blending... */
  pipeline = cg_pipeline_new(test_dev);
  cg_pipeline_set_color4ub(pipeline, Dr, Dg, Db, Da);
  cg_pipeline_set_blend(pipeline, "RGBA = ADD (SRC_COLOR, 0)", NULL);
  cg_framebuffer_draw_rectangle(test_fb,
                                pipeline,
                                x * QUAD_WIDTH,
                                y * QUAD_WIDTH,
                                x * QUAD_WIDTH + QUAD_WIDTH,
                                y * QUAD_WIDTH + QUAD_WIDTH);
  cg_object_unref(pipeline);

  /*
   * Now blend a rectangle over our well defined destination:
   */

  pipeline = cg_pipeline_new(test_dev);
  cg_pipeline_set_color4ub(pipeline, Sr, Sg, Sb, Sa);

  status = cg_pipeline_set_blend(pipeline, blend_string, &error);
  if (!status) {
      /* It's not strictly a test failure; you need a more capable GPU or
       * driver to test this blend string. */
      if (test_verbose ()) {
          c_debug("Failed to test blend string %s: %s",
                  blend_string, error->message);
          c_print("Skipping\n");
      }
      return;
  }

  cg_color_init_from_4ub(&blend_const_color, Br, Bg, Bb, Ba);
  cg_pipeline_set_blend_constant(pipeline, &blend_const_color);

  cg_framebuffer_draw_rectangle(test_fb,
                                pipeline,
                                x * QUAD_WIDTH,
                                y * QUAD_WIDTH,
                                x * QUAD_WIDTH + QUAD_WIDTH,
                                y * QUAD_WIDTH + QUAD_WIDTH);
  cg_object_unref (pipeline);

  /* See what we got... */

  y_off = y * QUAD_WIDTH + (QUAD_WIDTH / 2);
  x_off = x * QUAD_WIDTH + (QUAD_WIDTH / 2);

  if (test_verbose()) {
      c_print("test_blend (%d, %d):\n%s\n", x, y, blend_string);
      c_print("  src color = %02x, %02x, %02x, %02x\n", Sr, Sg, Sb, Sa);
      c_print("  dst color = %02x, %02x, %02x, %02x\n", Dr, Dg, Db, Da);
      if (blend_constant != BLEND_CONSTANT_UNUSED)
          c_print("  blend constant = %02x, %02x, %02x, %02x\n",
                  Br, Bg, Bb, Ba);
      else
          c_print("  blend constant = UNUSED\n");
  }

  test_cg_check_pixel(test_fb, x_off, y_off, expected_result);
}

static void
paint(test_state_t *state)
{
  test_blend(state, 0, 0, /* position */
             0xff0000ff, /* src */
             0xffffffff, /* dst */
             "RGBA = ADD (SRC_COLOR, 0)",
             BLEND_CONSTANT_UNUSED,
             0xff0000ff); /* expected */

  test_blend(state, 1, 0, /* position */
             0x11223344, /* src */
             0x11223344, /* dst */
             "RGBA = ADD (SRC_COLOR, DST_COLOR)",
             BLEND_CONSTANT_UNUSED,
             0x22446688); /* expected */

  test_blend(state, 2, 0, /* position */
             0x80808080, /* src */
             0xffffffff, /* dst */
             "RGBA = ADD (SRC_COLOR * (CONSTANT), 0)",
             0x80808080, /* constant (RGBA all = 0.5 when normalized) */
             0x40404040); /* expected */

  test_blend(state, 3, 0, /* position */
             0x80000080, /* src (alpha = 0.5 when normalized) */
             0x40000000, /* dst */
             "RGBA = ADD (SRC_COLOR * (SRC_COLOR[A]),"
             "            DST_COLOR * (1-SRC_COLOR[A]))",
             BLEND_CONSTANT_UNUSED,
             0x60000040); /* expected */
}

void
test_blend_strings(void)
{
  test_state_t state;

  cg_framebuffer_orthographic(test_fb, 0, 0,
                              cg_framebuffer_get_width (test_fb),
                              cg_framebuffer_get_height (test_fb),
                              -1,
                              100);

  paint (&state);

  if (test_verbose ())
    c_print ("OK\n");
}

