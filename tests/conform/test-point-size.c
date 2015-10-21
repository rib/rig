#include <config.h>

#include <cglib/cglib.h>

#include "test-cg-fixtures.h"

/* This test assumes the GL driver supports point sizes up to 16
   pixels. Cogl should probably have some way of querying the size so
   we start from that instead */
#define MAX_POINT_SIZE 16
/* The size of the area that we'll paint each point in */
#define POINT_BOX_SIZE (MAX_POINT_SIZE * 2)

static int
calc_coord_offset (int pos, int pos_index, int point_size)
{
  switch (pos_index)
    {
    case 0: return pos - point_size / 2 - 2;
    case 1: return pos - point_size / 2 + 2;
    case 2: return pos + point_size / 2 - 2;
    case 3: return pos + point_size / 2 + 2;
    }

  c_assert_not_reached ();
}

static void
verify_point_size (cg_framebuffer_t *test_fb,
                   int x_pos,
                   int y_pos,
                   int point_size)
{
  int y, x;

  for (y = 0; y < 4; y++)
    for (x = 0; x < 4; x++)
      {
        bool in_point = x >= 1 && x <= 2 && y >= 1 && y <= 2;
        uint32_t expected_pixel = in_point ? 0x00ff00ff : 0xff0000ff;

        test_cg_check_pixel (test_fb,
                                calc_coord_offset (x_pos, x, point_size),
                                calc_coord_offset (y_pos, y, point_size),
                                expected_pixel);
      }
}

void
test_point_size (void)
{
  int fb_width = cg_framebuffer_get_width (test_fb);
  int fb_height = cg_framebuffer_get_height (test_fb);
  int point_size;
  int x_pos;

  cg_framebuffer_orthographic (test_fb,
                                 0, 0, /* x_1, y_1 */
                                 fb_width, /* x_2 */
                                 fb_height /* y_2 */,
                                 -1, 100 /* near/far */);

  cg_framebuffer_clear4f (test_fb,
                            CG_BUFFER_BIT_COLOR,
                            1.0f, 0.0f, 0.0f, 1.0f);

  /* Try a rendering a single point with a few different point
     sizes */
  for (x_pos = 0, point_size = MAX_POINT_SIZE;
       point_size >= 4;
       x_pos += POINT_BOX_SIZE, point_size /= 2)
    {
      cg_pipeline_t *pipeline = cg_pipeline_new (test_dev);
      cg_vertex_p2_t point = { x_pos + POINT_BOX_SIZE / 2,
                             POINT_BOX_SIZE / 2 };
      cg_primitive_t *prim =
        cg_primitive_new_p2 (test_dev,
                               CG_VERTICES_MODE_POINTS,
                               1, /* n_vertices */
                               &point);

      cg_pipeline_set_point_size (pipeline, point_size);
      cg_pipeline_set_color4ub (pipeline, 0, 255, 0, 255);
      cg_primitive_draw (prim, test_fb, pipeline);

      cg_object_unref (prim);
      cg_object_unref (pipeline);
    }

  /* Verify all of the points where drawn at the right size */
  for (x_pos = 0, point_size = MAX_POINT_SIZE;
       point_size >= 4;
       x_pos += POINT_BOX_SIZE, point_size /= 2)
    verify_point_size (test_fb,
                       x_pos + POINT_BOX_SIZE / 2,
                       POINT_BOX_SIZE / 2,
                       point_size);

  if (test_verbose ())
    c_print ("OK\n");
}
