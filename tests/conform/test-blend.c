#include <config.h>

#include <cglib/cglib.h>

#include <string.h>

#include "test-cg-fixtures.h"

static void
paint (void)
{
  cg_pipeline_t *pipeline = cg_pipeline_new (test_dev);
  int width = cg_framebuffer_get_width (test_fb);
  int half_width = width / 2;
  int height = cg_framebuffer_get_height (test_fb);
  cg_vertex_p2_t tri0_vertices[] = {
        { 0, 0 },
        { 0, height },
        { half_width, height },
  };
  cg_vertex_p2c4_t tri1_vertices[] = {
        { half_width, 0, 0x80, 0x80, 0x80, 0x80 },
        { half_width, height, 0x80, 0x80, 0x80, 0x80 },
        { width, height, 0x80, 0x80, 0x80, 0x80 },
  };
  cg_primitive_t *tri0;
  cg_primitive_t *tri1;

  cg_framebuffer_clear4f (test_fb, CG_BUFFER_BIT_COLOR, 0, 0, 0, 0);

  tri0 = cg_primitive_new_p2 (test_dev, CG_VERTICES_MODE_TRIANGLES,
                                3, tri0_vertices);
  tri1 = cg_primitive_new_p2c4 (test_dev, CG_VERTICES_MODE_TRIANGLES,
                                  3, tri1_vertices);

  /* Check that cogl correctly handles the case where we draw
   * different primitives same pipeline and switch from using the
   * opaque color associated with the pipeline and using a colour
   * attribute with an alpha component which implies blending is
   * required.
   *
   * If Cogl gets this wrong then then in all likelyhood the second
   * primitive will be drawn with blending still disabled.
   */

  cg_primitive_draw (tri0, test_fb, pipeline);
  cg_primitive_draw (tri1, test_fb, pipeline);

  test_cg_check_pixel_and_alpha (test_fb,
                                    half_width + 5,
                                    height - 5,
                                    0x80808080);
}

void
test_blend (void)
{
  cg_framebuffer_orthographic (test_fb, 0, 0,
                                 cg_framebuffer_get_width (test_fb),
                                 cg_framebuffer_get_height (test_fb),
                                 -1,
                                 100);

  paint ();
}

