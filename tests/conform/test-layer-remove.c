#include <config.h>

#include <cglib/cglib.h>

#include "test-cg-fixtures.h"

#define TEST_SQUARE_SIZE 10

static cg_pipeline_t *
create_two_layer_pipeline(void)
{
    cg_pipeline_t *pipeline = cg_pipeline_new(test_dev);
    cg_snippet_t *snippet;
    int loc;
    const float red[] = { 1, 0, 0, 1 };
    const float green[] = { 0, 1, 0, 1 };

    /* The pipeline is initially black */
    cg_pipeline_set_color4ub(pipeline, 0, 0, 0, 255);

    /* The first layer adds a full red component */
    snippet = cg_snippet_new(CG_SNIPPET_HOOK_LAYER_FRAGMENT,
                             "uniform vec4 test_color0;\n",
                             NULL);
    cg_snippet_set_replace(snippet, "frag += test_color0;\n");
    cg_pipeline_add_layer_snippet(pipeline,
                                  0,/* layer num */
                                  snippet);
    cg_object_unref(snippet);

    /* The second layer adds a full green component */
    snippet = cg_snippet_new(CG_SNIPPET_HOOK_LAYER_FRAGMENT,
                             "uniform vec4 test_color1;\n",
                             NULL);
    cg_snippet_set_replace(snippet, "frag += test_color1;\n");
    cg_pipeline_add_layer_snippet(pipeline,
                                  1,/* layer num */
                                  snippet);
    cg_object_unref(snippet);

    loc = cg_pipeline_get_uniform_location(pipeline, "test_color0");
    cg_pipeline_set_uniform_float(pipeline, loc, 4, 1, red);

    loc = cg_pipeline_get_uniform_location(pipeline, "test_color1");
    cg_pipeline_set_uniform_float(pipeline, loc, 4, 1, green);

    return pipeline;
}

static void
test_color(cg_pipeline_t *pipeline,
           uint32_t color,
           int pos)
{
  cg_framebuffer_draw_rectangle(test_fb,
                                pipeline,
                                pos * TEST_SQUARE_SIZE,
                                0,
                                pos * TEST_SQUARE_SIZE + TEST_SQUARE_SIZE,
                                TEST_SQUARE_SIZE);
  test_cg_check_pixel(test_fb,
                      pos * TEST_SQUARE_SIZE + TEST_SQUARE_SIZE / 2,
                      TEST_SQUARE_SIZE / 2,
                      color);
}

void
test_layer_remove(void)
{
    cg_pipeline_t *pipeline0, *pipeline1;
    int pos = 0;
    cg_snippet_t *add_blue_snippet;

    cg_framebuffer_orthographic(test_fb,
                                0, 0,
                                cg_framebuffer_get_width(test_fb),
                                cg_framebuffer_get_height(test_fb),
                                -1,
                                100);

    /** TEST 1 **/
    /* Basic sanity check that the pipeline combines the two colors
     * together properly */
    pipeline0 = create_two_layer_pipeline();
    test_color(pipeline0, 0xffff00ff, pos++);
    cg_object_unref(pipeline0);

    /** TEST 2 **/
    /* Check that we can remove the second layer */
    pipeline0 = create_two_layer_pipeline();
    cg_pipeline_remove_layer(pipeline0, 1);
    test_color(pipeline0, 0xff0000ff, pos++);
    cg_object_unref(pipeline0);

    /** TEST 3 **/
    /* Check that we can remove the first layer */
    pipeline0 = create_two_layer_pipeline();
    cg_pipeline_remove_layer(pipeline0, 0);
    test_color(pipeline0, 0x00ff00ff, pos++);
    cg_object_unref(pipeline0);

    /** TEST 4 **/
    /* Check that we can make a copy and remove a layer from the
     * original pipeline */
    pipeline0 = create_two_layer_pipeline();
    pipeline1 = cg_pipeline_copy(pipeline0);
    cg_pipeline_remove_layer(pipeline0, 1);
    test_color(pipeline0, 0xff0000ff, pos++);
    test_color(pipeline1, 0xffff00ff, pos++);
    cg_object_unref(pipeline0);
    cg_object_unref(pipeline1);

    /** TEST 5 **/
    /* Check that we can make a copy and remove the second layer from the
     * new pipeline */
    pipeline0 = create_two_layer_pipeline();
    pipeline1 = cg_pipeline_copy(pipeline0);
    cg_pipeline_remove_layer(pipeline1, 1);
    test_color(pipeline0, 0xffff00ff, pos++);
    test_color(pipeline1, 0xff0000ff, pos++);
    cg_object_unref(pipeline0);
    cg_object_unref(pipeline1);

    /** TEST 6 **/
    /* Check that we can make a copy and remove the first layer from the
     * new pipeline */
    pipeline0 = create_two_layer_pipeline();
    pipeline1 = cg_pipeline_copy(pipeline0);
    cg_pipeline_remove_layer(pipeline1, 0);
    test_color(pipeline0, 0xffff00ff, pos++);
    test_color(pipeline1, 0x00ff00ff, pos++);
    cg_object_unref(pipeline0);
    cg_object_unref(pipeline1);

    add_blue_snippet = cg_snippet_new(CG_SNIPPET_HOOK_LAYER_FRAGMENT,
                                      NULL, /* declarations */
                                      NULL); /* post */
    cg_snippet_set_replace(add_blue_snippet, "frag += vec4(0.0, 0.0, 1.0, 1.0);\n");

    /** TEST 7 **/
    /* Check that we can modify a layer in a child pipeline */
    pipeline0 = create_two_layer_pipeline();
    pipeline1 = cg_pipeline_copy(pipeline0);
    cg_pipeline_add_layer_snippet(pipeline1, 0, add_blue_snippet);
    test_color(pipeline0, 0xffff00ff, pos++);
    test_color(pipeline1, 0x00ffffff, pos++);
    cg_object_unref(pipeline0);
    cg_object_unref(pipeline1);

    /** TEST 8 **/
    /* Check that we can modify a layer in a child pipeline but then remove it */
    pipeline0 = create_two_layer_pipeline();
    pipeline1 = cg_pipeline_copy(pipeline0);
    cg_pipeline_add_layer_snippet(pipeline1, 0, add_blue_snippet);
    cg_pipeline_remove_layer(pipeline1, 0);
    test_color(pipeline0, 0xffff00ff, pos++);
    test_color(pipeline1, 0x00ff00ff, pos++);
    cg_object_unref(pipeline0);
    cg_object_unref(pipeline1);

    if(test_verbose())
        c_print("OK\n");
}
