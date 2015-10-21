#include <config.h>

#include <cglib/cglib.h>

#include <string.h>

#include "test-cg-fixtures.h"

void
test_pipeline_shader_state (void)
{
  cg_offscreen_t *offscreen;
  cg_framebuffer_t *fb;
  cg_pipeline_t *base_pipeline;
  cg_pipeline_t *draw_pipeline;
  cg_texture_2d_t *tex;
  cg_snippet_t *snippet;

  float width = cg_framebuffer_get_width (test_fb);
  float height = cg_framebuffer_get_height (test_fb);

  cg_framebuffer_orthographic (test_fb,
                                 0, 0, width, height,
                                 -1,
                                 100);

  tex = cg_texture_2d_new_with_size (test_dev, 128, 128);
  offscreen = cg_offscreen_new_with_texture (tex);
  fb = offscreen;
  cg_framebuffer_clear4f (fb, CG_BUFFER_BIT_COLOR, 0, 0, 0, 1);
  cg_object_unref (offscreen);

  cg_framebuffer_clear4f (test_fb, CG_BUFFER_BIT_COLOR, 1, 1, 0, 1);


  /* Setup a template pipeline... */

  base_pipeline = cg_pipeline_new (test_dev);
  cg_pipeline_set_layer_texture (base_pipeline, 1, tex);
  cg_pipeline_set_color4f (base_pipeline, 1, 0, 0, 1);


  /* Derive a pipeline from the template, making a change that affects
   * fragment processing but making sure not to affect vertex
   * processing... */

  draw_pipeline = cg_pipeline_copy (base_pipeline);
  snippet = cg_snippet_new (CG_SNIPPET_HOOK_FRAGMENT,
                              NULL, /* declarations */
                              "cg_color_out = vec4 (0.0, 1.0, 0.1, 1.1);");
  cg_pipeline_add_snippet (draw_pipeline, snippet);
  cg_object_unref (snippet);

  cg_framebuffer_draw_rectangle (test_fb, draw_pipeline,
                                   0, 0, width, height);

  cg_object_unref (draw_pipeline);

  cg_framebuffer_finish (test_fb);


  /* At this point we should have provoked cogl to cache some vertex
   * shader state for the draw_pipeline with the base_pipeline because
   * none of the changes made to the draw_pipeline affected vertex
   * processing. (NB: cogl will cache shader state with the oldest
   * ancestor that the state is still valid for to maximize the chance
   * that it can be used with other derived pipelines)
   *
   * Now we make a change to the base_pipeline to make sure that this
   * cached vertex shader gets invalidated.
   */

  cg_pipeline_set_layer_texture (base_pipeline, 0, tex);


  /* Now we derive another pipeline from base_pipeline to verify that
   * it doesn't end up re-using the old cached state
   */

  draw_pipeline = cg_pipeline_copy (base_pipeline);
  snippet = cg_snippet_new (CG_SNIPPET_HOOK_FRAGMENT,
                              NULL, /* declarations */
                              "cg_color_out = vec4 (0.0, 0.0, 1.1, 1.1);");
  cg_pipeline_add_snippet (draw_pipeline, snippet);
  cg_object_unref (snippet);

  cg_framebuffer_draw_rectangle (test_fb, draw_pipeline,
                                   0, 0, width, height);

  cg_object_unref (draw_pipeline);


  test_cg_check_region (test_fb, 0, 0, width, height,
                           0x0000ffff);
}
