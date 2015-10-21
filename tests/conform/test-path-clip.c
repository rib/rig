#include <config.h>

#include <cglib/cglib.h>
#include <cogl-path/cogl-path.h>

#include <string.h>

#include "test-cg-fixtures.h"

void
test_path_clip (void)
{
  cg_path_t *path;
  cg_pipeline_t *pipeline;
  int fb_width, fb_height;

  fb_width = cg_framebuffer_get_width (test_fb);
  fb_height = cg_framebuffer_get_height (test_fb);

  cg_framebuffer_orthographic (test_fb,
                                 0, 0, fb_width, fb_height, -1, 100);

  path = cg_path_new (test_dev);

  cg_framebuffer_clear4f (test_fb,
                            CG_BUFFER_BIT_COLOR,
                            1.0f, 0.0f, 0.0f, 1.0f);

  /* Make an L-shape with the top right corner left untouched */
  cg_path_move_to (path, 0, fb_height);
  cg_path_line_to (path, fb_width, fb_height);
  cg_path_line_to (path, fb_width, fb_height / 2);
  cg_path_line_to (path, fb_width / 2, fb_height / 2);
  cg_path_line_to (path, fb_width / 2, 0);
  cg_path_line_to (path, 0, 0);
  cg_path_close (path);

  cg_framebuffer_push_path_clip (test_fb, path);

  /* Try to fill the framebuffer with a blue rectangle. This should be
   * clipped to leave the top right quadrant as is */
  pipeline = cg_pipeline_new (test_dev);
  cg_pipeline_set_color4ub (pipeline, 0, 0, 255, 255);
  cg_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   0, 0, fb_width, fb_height);

  cg_framebuffer_pop_clip (test_fb);

  cg_object_unref (pipeline);
  cg_object_unref (path);

  /* Check each of the four quadrants */
  test_cg_check_pixel (test_fb,
                          fb_width / 4, fb_height / 4,
                          0x0000ffff);
  test_cg_check_pixel (test_fb,
                          fb_width * 3 / 4, fb_height / 4,
                          0xff0000ff);
  test_cg_check_pixel (test_fb,
                          fb_width / 4, fb_height * 3 / 4,
                          0x0000ffff);
  test_cg_check_pixel (test_fb,
                          fb_width * 3 / 4, fb_height * 3 / 4,
                          0x0000ffff);

  if (test_verbose ())
    c_print ("OK\n");
}
