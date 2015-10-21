#include <config.h>

#include <cglib/cglib.h>

#include "test-cg-fixtures.h"

typedef cg_vertex_p2c4_t Vertex;

static void
setup_orthographic_modelview (void)
{
  c_matrix_t matrix;
  int fb_width = cg_framebuffer_get_width (test_fb);
  int fb_height = cg_framebuffer_get_height (test_fb);

  /* Set up a non-identity modelview matrix. When the journal is
   * flushed it will usually flush the identity matrix. Using the
   * non-default matrix ensures that we test that Cogl restores the
   * matrix we asked for. The matrix sets up an orthographic transform
   * in the modelview matrix */

  c_matrix_init_identity (&matrix);
  c_matrix_orthographic (&matrix,
                            0.0f, 0.0f, /* x_1 y_1 */
                            fb_width,
                            fb_height,
                            -1.0f, /* nearval */
                            1.0f /* farval */);
  cg_framebuffer_set_modelview_matrix (test_fb, &matrix);
}

static void
create_primitives (cg_primitive_t *primitives[2])
{
  static const Vertex vertex_data[8] =
    {
      /* triangle strip 1 */
      {   0,   0, 255, 0, 0, 255 },
      {   0, 100, 255, 0, 0, 255 },
      { 100,   0, 255, 0, 0, 255 },
      { 100, 100, 255, 0, 0, 255 },
      /* triangle strip 2 */
      { 200,   0, 0, 0, 255, 255 },
      { 200, 100, 0, 0, 255, 255 },
      { 300,   0, 0, 0, 255, 255 },
      { 300, 100, 0, 0, 255, 255 },
    };

  primitives[0] = cg_primitive_new_p2c4 (test_dev,
                                           CG_VERTICES_MODE_TRIANGLE_STRIP,
                                           C_N_ELEMENTS (vertex_data),
                                           vertex_data);
  cg_primitive_set_n_vertices (primitives[0], 4);

  primitives[1] = cg_primitive_copy (primitives[0]);
  cg_primitive_set_first_vertex (primitives[1], 4);
  cg_primitive_set_n_vertices (primitives[1], 4);
}

static cg_pipeline_t *
create_pipeline (void)
{
  cg_pipeline_t *pipeline = cg_pipeline_new (test_dev);

  cg_pipeline_set_color4ub (pipeline, 0, 255, 0, 255);

  return pipeline;
}

void
test_primitive_and_journal (void)
{
  cg_primitive_t *primitives[2];
  cg_pipeline_t *pipeline;

  setup_orthographic_modelview ();
  create_primitives (primitives);
  pipeline = create_pipeline ();

  /* Set a clip to clip all three rectangles to just the bottom half.
   * The journal flushes its own clip state so this verifies that the
   * clip state is correctly restored for the second primitive. */
  cg_framebuffer_push_rectangle_clip (test_fb,
                                        0, 50, 300, 100);

  cg_primitive_draw (primitives[0], test_fb, pipeline);

  /* Draw a rectangle using the journal in-between the two primitives.
   * This should test that the journal gets flushed correctly and that
   * the modelview matrix is restored. Half of the rectangle should be
   * overriden by the second primitive */
  cg_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   100, 0, /* x1/y1 */
                                   300, 100 /* x2/y2 */);

  cg_primitive_draw (primitives[1], test_fb, pipeline);

  /* Check the three rectangles */
  test_cg_check_region (test_fb,
                           1, 51,
                           98, 48,
                           0xff0000ff);
  test_cg_check_region (test_fb,
                           101, 51,
                           98, 48,
                           0x00ff00ff);
  test_cg_check_region (test_fb,
                           201, 51,
                           98, 48,
                           0x0000ffff);

  /* Check that the top half of all of the rectangles was clipped */
  test_cg_check_region (test_fb,
                           1, 1,
                           298, 48,
                           0x000000ff);

  cg_framebuffer_pop_clip (test_fb);

  if (test_verbose ())
    c_print ("OK\n");
}

