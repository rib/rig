#include <config.h>

#include <cglib/cglib.h>

#include "test-cg-fixtures.h"

#define POINT_SIZE 8

static const cg_vertex_p2t2_t
point =
  {
    POINT_SIZE, POINT_SIZE,
    0.0f, 0.0f
  };

static const uint8_t
tex_data[3 * 2 * 2] =
  {
    0x00, 0x00, 0xff, 0x00, 0xff, 0x00,
    0x00, 0xff, 0xff, 0xff, 0x00, 0x00
  };

static void
do_test (bool check_orientation,
         bool use_glsl)
{
  int fb_width = cg_framebuffer_get_width (test_fb);
  int fb_height = cg_framebuffer_get_height (test_fb);
  cg_primitive_t *prim;
  cg_error_t *error = NULL;
  cg_texture_2d_t *tex_2d;
  cg_pipeline_t *pipeline, *solid_pipeline;
  int tex_height;

  test_allow_failure();

  cg_framebuffer_orthographic (test_fb,
                                 0, 0, /* x_1, y_1 */
                                 fb_width, /* x_2 */
                                 fb_height /* y_2 */,
                                 -1, 100 /* near/far */);

  cg_framebuffer_clear4f (test_fb,
                            CG_BUFFER_BIT_COLOR,
                            1.0f, 1.0f, 1.0f, 1.0f);

  /* If we're not checking the orientation of the point sprite then
   * we'll set the height of the texture to 1 so that the vertical
   * orientation does not matter */
  if (check_orientation)
    tex_height = 2;
  else
    tex_height = 1;

  tex_2d = cg_texture_2d_new_from_data (test_dev,
                                          2, tex_height, /* width/height */
                                          CG_PIXEL_FORMAT_RGB_888,
                                          6, /* row stride */
                                          tex_data,
                                          &error);
  c_assert (tex_2d != NULL);
  c_assert (error == NULL);

  pipeline = cg_pipeline_new (test_dev);
  cg_pipeline_set_layer_texture (pipeline, 0, tex_2d);

  cg_pipeline_set_layer_filters (pipeline,
                                   0, /* layer_index */
                                   CG_PIPELINE_FILTER_NEAREST,
                                   CG_PIPELINE_FILTER_NEAREST);
  cg_pipeline_set_point_size (pipeline, POINT_SIZE);

  /* If we're using GLSL then we don't need to enable point sprite
   * coords and we can just directly reference cg_point_coord in the
   * snippet */
  if (use_glsl)
    {
      cg_snippet_t *snippet =
        cg_snippet_new (CG_SNIPPET_HOOK_TEXTURE_LOOKUP,
                          NULL, /* declarations */
                          NULL /* post */);
      static const char source[] =
        "  cg_texel = texture2D (cg_sampler, cg_point_coord);\n";

      cg_snippet_set_replace (snippet, source);

      /* Keep a reference to the original pipeline because there is no
       * way to remove a snippet in order to recreate the solid
       * pipeline */
      solid_pipeline = cg_pipeline_copy (pipeline);

      cg_pipeline_add_layer_snippet (pipeline, 0, snippet);

      cg_object_unref (snippet);
    }
  else
    {
      bool res =
        cg_pipeline_set_layer_point_sprite_coords_enabled (pipeline,
                                                             /* layer_index */
                                                             0,
                                                             /* enable */
                                                             true,
                                                             &error);
      c_assert (res == true);
      c_assert (error == NULL);

      solid_pipeline = cg_pipeline_copy (pipeline);

      res =
        cg_pipeline_set_layer_point_sprite_coords_enabled (solid_pipeline,
                                                             /* layer_index */
                                                             0,
                                                             /* enable */
                                                             false,
                                                             &error);

      c_assert (res == true);
      c_assert (error == NULL);
    }

  prim = cg_primitive_new_p2t2 (test_dev,
                                  CG_VERTICES_MODE_POINTS,
                                  1, /* n_vertices */
                                  &point);

  cg_primitive_draw (prim, test_fb, pipeline);

  /* Render the primitive again without point sprites to make sure
     disabling it works */

  cg_framebuffer_push_matrix (test_fb);
  cg_framebuffer_translate (test_fb,
                              POINT_SIZE * 2, /* x */
                              0.0f, /* y */
                              0.0f /* z */);
  cg_primitive_draw (prim, test_fb, solid_pipeline);
  cg_framebuffer_pop_matrix (test_fb);

  cg_object_unref (prim);
  cg_object_unref (solid_pipeline);
  cg_object_unref (pipeline);
  cg_object_unref (tex_2d);

  test_cg_check_pixel (test_fb,
                          POINT_SIZE - POINT_SIZE / 4,
                          POINT_SIZE - POINT_SIZE / 4,
                          0x0000ffff);
  test_cg_check_pixel (test_fb,
                          POINT_SIZE + POINT_SIZE / 4,
                          POINT_SIZE - POINT_SIZE / 4,
                          0x00ff00ff);
  test_cg_check_pixel (test_fb,
                          POINT_SIZE - POINT_SIZE / 4,
                          POINT_SIZE + POINT_SIZE / 4,
                          check_orientation ?
                          0x00ffffff :
                          0x0000ffff);
  test_cg_check_pixel (test_fb,
                          POINT_SIZE + POINT_SIZE / 4,
                          POINT_SIZE + POINT_SIZE / 4,
                          check_orientation ?
                          0xff0000ff :
                          0x00ff00ff);

  /* When rendering without the point sprites all of the texture
     coordinates should be 0,0 so it should get the top-left texel
     which is blue */
  test_cg_check_region (test_fb,
                           POINT_SIZE * 3 - POINT_SIZE / 2 + 1,
                           POINT_SIZE - POINT_SIZE / 2 + 1,
                           POINT_SIZE - 2, POINT_SIZE - 2,
                           0x0000ffff);

  if (test_verbose ())
    c_print ("OK\n");
}

void
test_point_sprite (void)
{
  do_test (false /* don't check orientation */,
           false /* don't use GLSL */);
}

void
test_point_sprite_orientation (void)
{
  do_test (true /* check orientation */,
           false /* don't use GLSL */);
}

void
test_point_sprite_glsl (void)
{
  do_test (false /* don't check orientation */,
           true /* use GLSL */);
}
