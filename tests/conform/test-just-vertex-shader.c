#include <config.h>

#include <cglib/cglib.h>

#include <string.h>

#include "test-cg-fixtures.h"

typedef struct _TestState {
    int paddiing;
} TestState;

static cg_texture_t *
create_dummy_texture(void)
{
  /* Create a dummy 1x1 green texture to replace the color from the
     vertex shader */
  static const uint8_t data[4] = { 0x00, 0xff, 0x00, 0xff };

  return test_cg_texture_new_from_data(test_dev,
                                       1, 1, /* size */
                                       TEST_CG_TEXTURE_NONE,
                                       CG_PIXEL_FORMAT_RGB_888,
                                       4, /* rowstride */
                                       data);
}

static void
paint(TestState *state)
{
    cg_pipeline_t *pipeline = cg_pipeline_new(test_dev);
    cg_texture_t *tex;
    cg_snippet_t *snippet;

    cg_framebuffer_clear4f(test_fb, CG_BUFFER_BIT_COLOR, 0, 0, 0, 1);

    /* Set the primary vertex color as red */
    cg_pipeline_set_color4f(pipeline, 1, 0, 0, 1);

    /* Override the vertex color in the texture environment with a
       constant green color provided by a texture */
    tex = create_dummy_texture();
    cg_pipeline_set_layer_texture(pipeline, 0, tex);
    cg_object_unref(tex);

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_LAYER_FRAGMENT, NULL, NULL);
    cg_snippet_set_replace(snippet, "frag = cg_texel0;\n");
    cg_pipeline_add_layer_snippet(pipeline, 0, snippet);
    cg_object_unref(snippet);

    /* Set up a dummy vertex shader that does nothing but the usual
       modelview-projection transform */
    snippet = cg_snippet_new(CG_SNIPPET_HOOK_VERTEX,
                             NULL, /* no declarations */
                             NULL); /* no "post" code */
    cg_snippet_set_replace(snippet,
                           "  cg_position_out = "
                           "cg_modelview_projection_matrix * "
                           "cg_position_in;\n"
                           "  cg_color_out = cg_color_in;\n"
                           "  cg_tex_coord0_out = cg_tex_coord_in;\n");

    /* Draw something without the snippet */
    cg_framebuffer_draw_rectangle(test_fb, pipeline, 0, 0, 50, 50);

    /* Draw it again using the snippet. It should look exactly the same */
    cg_pipeline_add_snippet(pipeline, snippet);
    cg_object_unref(snippet);

    cg_framebuffer_draw_rectangle(test_fb, pipeline, 50, 0, 100, 50);

    cg_object_unref(pipeline);
}

static void
validate_result(cg_framebuffer_t *framebuffer)
{
    /* Non-shader version */
    test_cg_check_pixel(framebuffer, 25, 25, 0x00ff0000);
    /* Shader version */
    test_cg_check_pixel(framebuffer, 75, 25, 0x00ff0000);
}

void
test_just_vertex_shader(void)
{
    TestState state;

    cg_framebuffer_orthographic(test_fb,
                                0, 0,
                                cg_framebuffer_get_width (test_fb),
                                cg_framebuffer_get_height (test_fb),
                                -1,
                                100);

    paint(&state);
    validate_result(test_fb);

    if (test_verbose())
        c_print("OK\n");
}
