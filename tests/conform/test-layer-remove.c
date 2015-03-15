#include <config.h>

#include <cogl/cogl.h>

#include "test-cg-fixtures.h"

#define TEST_SQUARE_SIZE 10

static cg_pipeline_t *
create_two_layer_pipeline (void)
{
  cg_pipeline_t *pipeline = cg_pipeline_new (test_dev);
  cg_color_t color;

  /* The pipeline is initially black */
  cg_pipeline_set_color4ub (pipeline, 0, 0, 0, 255);

  /* The first layer adds a full red component */
  cg_color_init_from_4ub (&color, 255, 0, 0, 255);
  cg_pipeline_set_layer_combine_constant (pipeline, 0, &color);
  cg_pipeline_set_layer_combine (pipeline,
                                   0, /* layer_num */
                                   "RGBA=ADD(PREVIOUS,CONSTANT)",
                                   NULL);

  /* The second layer adds a full green component */
  cg_color_init_from_4ub (&color, 0, 255, 0, 255);
  cg_pipeline_set_layer_combine_constant (pipeline, 1, &color);
  cg_pipeline_set_layer_combine (pipeline,
                                   1, /* layer_num */
                                   "RGBA=ADD(PREVIOUS,CONSTANT)",
                                   NULL);

  return pipeline;
}

static void
test_color (cg_pipeline_t *pipeline,
            uint32_t color,
            int pos)
{
  cg_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   pos * TEST_SQUARE_SIZE,
                                   0,
                                   pos * TEST_SQUARE_SIZE + TEST_SQUARE_SIZE,
                                   TEST_SQUARE_SIZE);
  test_cg_check_pixel (test_fb,
                          pos * TEST_SQUARE_SIZE + TEST_SQUARE_SIZE / 2,
                          TEST_SQUARE_SIZE / 2,
                          color);
}

void
test_layer_remove (void)
{
  cg_pipeline_t *pipeline0, *pipeline1;
  cg_color_t color;
  int pos = 0;

  cg_framebuffer_orthographic (test_fb,
                                 0, 0,
                                 cg_framebuffer_get_width (test_fb),
                                 cg_framebuffer_get_height (test_fb),
                                 -1,
                                 100);

  /** TEST 1 **/
  /* Basic sanity check that the pipeline combines the two colors
   * together properly */
  pipeline0 = create_two_layer_pipeline ();
  test_color (pipeline0, 0xffff00ff, pos++);
  cg_object_unref (pipeline0);

  /** TEST 2 **/
  /* Check that we can remove the second layer */
  pipeline0 = create_two_layer_pipeline ();
  cg_pipeline_remove_layer (pipeline0, 1);
  test_color (pipeline0, 0xff0000ff, pos++);
  cg_object_unref (pipeline0);

  /** TEST 3 **/
  /* Check that we can remove the first layer */
  pipeline0 = create_two_layer_pipeline ();
  cg_pipeline_remove_layer (pipeline0, 0);
  test_color (pipeline0, 0x00ff00ff, pos++);
  cg_object_unref (pipeline0);

  /** TEST 4 **/
  /* Check that we can make a copy and remove a layer from the
   * original pipeline */
  pipeline0 = create_two_layer_pipeline ();
  pipeline1 = cg_pipeline_copy (pipeline0);
  cg_pipeline_remove_layer (pipeline0, 1);
  test_color (pipeline0, 0xff0000ff, pos++);
  test_color (pipeline1, 0xffff00ff, pos++);
  cg_object_unref (pipeline0);
  cg_object_unref (pipeline1);

  /** TEST 5 **/
  /* Check that we can make a copy and remove the second layer from the
   * new pipeline */
  pipeline0 = create_two_layer_pipeline ();
  pipeline1 = cg_pipeline_copy (pipeline0);
  cg_pipeline_remove_layer (pipeline1, 1);
  test_color (pipeline0, 0xffff00ff, pos++);
  test_color (pipeline1, 0xff0000ff, pos++);
  cg_object_unref (pipeline0);
  cg_object_unref (pipeline1);

  /** TEST 6 **/
  /* Check that we can make a copy and remove the first layer from the
   * new pipeline */
  pipeline0 = create_two_layer_pipeline ();
  pipeline1 = cg_pipeline_copy (pipeline0);
  cg_pipeline_remove_layer (pipeline1, 0);
  test_color (pipeline0, 0xffff00ff, pos++);
  test_color (pipeline1, 0x00ff00ff, pos++);
  cg_object_unref (pipeline0);
  cg_object_unref (pipeline1);

  /** TEST 7 **/
  /* Check that we can modify a layer in a child pipeline */
  pipeline0 = create_two_layer_pipeline ();
  pipeline1 = cg_pipeline_copy (pipeline0);
  cg_color_init_from_4ub (&color, 0, 0, 255, 255);
  cg_pipeline_set_layer_combine_constant (pipeline1, 0, &color);
  test_color (pipeline0, 0xffff00ff, pos++);
  test_color (pipeline1, 0x00ffffff, pos++);
  cg_object_unref (pipeline0);
  cg_object_unref (pipeline1);

  /** TEST 8 **/
  /* Check that we can modify a layer in a child pipeline but then remove it */
  pipeline0 = create_two_layer_pipeline ();
  pipeline1 = cg_pipeline_copy (pipeline0);
  cg_color_init_from_4ub (&color, 0, 0, 255, 255);
  cg_pipeline_set_layer_combine_constant (pipeline1, 0, &color);
  cg_pipeline_remove_layer (pipeline1, 0);
  test_color (pipeline0, 0xffff00ff, pos++);
  test_color (pipeline1, 0x00ff00ff, pos++);
  cg_object_unref (pipeline0);
  cg_object_unref (pipeline1);

  if (test_verbose ())
    c_print ("OK\n");
}
