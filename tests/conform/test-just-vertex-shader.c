#include <cogl/cogl.h>

#include <string.h>

#include "test-utils.h"

typedef struct _TestState
{
  int paddiing;
} TestState;

static CoglTexture *
create_dummy_texture (void)
{
  /* Create a dummy 1x1 green texture to replace the color from the
     vertex shader */
  static const uint8_t data[4] = { 0x00, 0xff, 0x00, 0xff };

  return test_utils_texture_new_from_data (test_ctx,
                                           1, 1, /* size */
                                           TEST_UTILS_TEXTURE_NONE,
                                           COGL_PIXEL_FORMAT_RGB_888,
                                           4, /* rowstride */
                                           data);
}

static void
paint (TestState *state)
{
  CoglPipeline *pipeline = cogl_pipeline_new (test_ctx);
  CoglTexture *tex;
  CoglError *error = NULL;
  CoglSnippet *snippet;

  cogl_framebuffer_clear4f (test_fb, COGL_BUFFER_BIT_COLOR, 0, 0, 0, 1);

  /* Set the primary vertex color as red */
  cogl_pipeline_set_color4f (pipeline, 1, 0, 0, 1);

  /* Override the vertex color in the texture environment with a
     constant green color provided by a texture */
  tex = create_dummy_texture ();
  cogl_pipeline_set_layer_texture (pipeline, 0, tex);
  cogl_object_unref (tex);
  if (!cogl_pipeline_set_layer_combine (pipeline, 0,
                                        "RGBA=REPLACE(TEXTURE)",
                                        &error))
    {
      g_warning ("Error setting layer combine: %s", error->message);
      g_assert_not_reached ();
    }

  /* Set up a dummy vertex shader that does nothing but the usual
     modelview-projection transform */
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX,
                              NULL, /* no declarations */
                              NULL); /* no "post" code */
  cogl_snippet_set_replace (snippet,
                            "  cogl_position_out = "
                            "cogl_modelview_projection_matrix * "
                            "cogl_position_in;\n"
                            "  cogl_color_out = cogl_color_in;\n"
                            "  cogl_tex_coord0_out = cogl_tex_coord_in;\n");

  /* Draw something without the snippet */
  cogl_framebuffer_draw_rectangle (test_fb, pipeline, 0, 0, 50, 50);

  /* Draw it again using the snippet. It should look exactly the same */
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  cogl_framebuffer_draw_rectangle (test_fb, pipeline, 50, 0, 100, 50);

  cogl_object_unref (pipeline);
}

static void
validate_result (CoglFramebuffer *framebuffer)
{
  /* Non-shader version */
  test_utils_check_pixel (framebuffer, 25, 25, 0x00ff0000);
  /* Shader version */
  test_utils_check_pixel (framebuffer, 75, 25, 0x00ff0000);
}

void
test_just_vertex_shader (void)
{
  TestState state;

  cogl_framebuffer_orthographic (test_fb,
                                 0, 0,
                                 cogl_framebuffer_get_width (test_fb),
                                 cogl_framebuffer_get_height (test_fb),
                                 -1,
                                 100);

  paint (&state);
  validate_result (test_fb);

  if (cogl_test_verbose ())
    c_print ("OK\n");
}
