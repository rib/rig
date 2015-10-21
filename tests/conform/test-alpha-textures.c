#include <config.h>

#include <cglib/cglib.h>

#include <string.h>

#include "test-cg-fixtures.h"

static void
create_pipeline(cg_texture_t **tex_out,
                cg_pipeline_t **pipeline_out)
{
  cg_texture_2d_t *tex;
  cg_pipeline_t *pipeline;
  static const uint8_t tex_data[] =
    { 0x00, 0x44, 0x88, 0xcc };
  cg_snippet_t *snippet;

  tex = cg_texture_2d_new_from_data (test_dev,
                                       2, 2, /* width/height */
                                       CG_PIXEL_FORMAT_A_8, /* format */
                                       2, /* rowstride */
                                       tex_data,
                                       NULL);

  pipeline = cg_pipeline_new (test_dev);

  cg_pipeline_set_layer_filters (pipeline,
                                   0, /* layer */
                                   CG_PIPELINE_FILTER_NEAREST,
                                   CG_PIPELINE_FILTER_NEAREST);
  cg_pipeline_set_layer_wrap_mode (pipeline,
                                     0, /* layer */
                                     CG_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);

  /* This is the layer snippet used by cogl-pango */
  snippet = cg_snippet_new(CG_SNIPPET_HOOK_LAYER_FRAGMENT, NULL, NULL);
  cg_snippet_set_replace(snippet, "frag *= cg_texel0.a;\n");
  cg_pipeline_add_layer_snippet(pipeline, 0, snippet);
  cg_object_unref(snippet);

  cg_pipeline_set_layer_texture (pipeline,
                                 0, /* layer */
                                 tex);

  *pipeline_out = pipeline;
  *tex_out = tex;
}

void
test_alpha_textures (void)
{
  cg_texture_t *tex1, *tex2;
  cg_pipeline_t *pipeline1, *pipeline2;
  int fb_width = cg_framebuffer_get_width (test_fb);
  int fb_height = cg_framebuffer_get_height (test_fb);
  uint8_t replacement_data[1] = { 0xff };

  create_pipeline (&tex1, &pipeline1);

  cg_framebuffer_draw_rectangle (test_fb,
                                   pipeline1,
                                   -1.0f, 1.0f, /* x1/y1 */
                                   1.0f, 0.0f /* x2/y2 */);

  create_pipeline (&tex2, &pipeline2);

  cg_texture_set_region (tex2,
                           1, 1, /* width / height */
                           CG_PIXEL_FORMAT_A_8,
                           1, /* rowstride */
                           replacement_data,
                           1, 1, /* dst_x/y */
                           0, /* level */
                           NULL); /* abort on error */

  cg_framebuffer_draw_rectangle (test_fb,
                                   pipeline2,
                                   -1.0f, 0.0f, /* x1/y1 */
                                   1.0f, -1.0f /* x2/y2 */);

  cg_object_unref (tex1);
  cg_object_unref (tex2);
  cg_object_unref (pipeline1);
  cg_object_unref (pipeline2);

  /* Unmodified texture */
  test_cg_check_pixel (test_fb,
                          fb_width / 4,
                          fb_height / 8,
                          0x000000ff);
  test_cg_check_pixel (test_fb,
                          fb_width * 3 / 4,
                          fb_height / 8,
                          0x444444ff);
  test_cg_check_pixel (test_fb,
                          fb_width / 4,
                          fb_height * 3 / 8,
                          0x888888ff);
  test_cg_check_pixel (test_fb,
                          fb_width * 3 / 4,
                          fb_height * 3 / 8,
                          0xccccccff);

  /* Modified texture */
  test_cg_check_pixel (test_fb,
                          fb_width / 4,
                          fb_height * 5 / 8,
                          0x000000ff);
  test_cg_check_pixel (test_fb,
                          fb_width * 3 / 4,
                          fb_height * 5 / 8,
                          0x444444ff);
  test_cg_check_pixel (test_fb,
                          fb_width / 4,
                          fb_height * 7 / 8,
                          0x888888ff);
  test_cg_check_pixel (test_fb,
                          fb_width * 3 / 4,
                          fb_height * 7 / 8,
                          0xffffffff);

  if (test_verbose ())
    c_print ("OK\n");
}

