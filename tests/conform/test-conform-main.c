#include "config.h"

#include <cglib/cglib.h>

#include <clib.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include <test-fixtures/test-fixtures.h>
#include <test-fixtures/test-cg-fixtures.h>

/* A bit of sugar for adding new conformance tests */
#define ADD_CG_TEST(FUNC, REQUIREMENTS)                                 \
  C_STMT_START {                                                        \
    extern void FUNC (void);                                            \
    if (strcmp (#FUNC, argv[1]) == 0) {                                 \
        test_cg_init();                                                 \
        if (test_cg_check_requirements(REQUIREMENTS))                   \
            FUNC();                                                     \
        else                                                            \
            exit(1);                                                    \
        test_cg_fini();                                                 \
        exit(0);                                                        \
    }                                                                   \
  } C_STMT_END

int
main (int argc, char **argv)
{
  int i;

  if (argc != 2) {
      c_printerr ("usage %s TEST\n", argv[0]);
      exit (1);
  }

  /* Just for convenience in case people try passing the wrapper
   * filenames for the UNIT_TEST argument we normalize '-' characters
   * to '_' characters... */
  for (i = 0; argv[1][i]; i++) {
      if (argv[1][i] == '-')
          argv[1][i] = '_';
  }

  /* This file is run through a sed script during the make step so the
   * lines containing the tests need to be formatted on a single line
   * each.
   */

  ADD_CG_TEST(test_blend_strings, 0);
  ADD_CG_TEST(test_blend, 0);
  ADD_CG_TEST(test_premult, 0);
#ifdef CG_HAS_CG_PATH_SUPPORT
  ADD_CG_TEST(test_path, 0);
  ADD_CG_TEST(test_path_clip, 0);
#endif
  ADD_CG_TEST(test_depth_test, 0);
  ADD_CG_TEST(test_color_mask, 0);
  ADD_CG_TEST(test_backface_culling, 0);
  ADD_CG_TEST(test_layer_remove, 0);

  ADD_CG_TEST(test_sparse_pipeline, 0);

  ADD_CG_TEST(test_npot_texture, 0);
  ADD_CG_TEST(test_sub_texture, 0);
  ADD_CG_TEST(test_pixel_buffer_map, 0);
  ADD_CG_TEST(test_pixel_buffer_set_data, 0);
  ADD_CG_TEST(test_pixel_buffer_sub_region, 0);
  ADD_CG_TEST(test_texture_3d, TEST_CG_REQUIREMENT_TEXTURE_3D);
  ADD_CG_TEST(test_wrap_modes, 0);
  ADD_CG_TEST(test_texture_get_set_data, 0);
  /* This test won't work on GLES because that doesn't support setting
   * the maximum texture level. */
  ADD_CG_TEST(test_texture_mipmap_get_set, TEST_CG_REQUIREMENT_GL);
  ADD_CG_TEST(test_atlas_migration, 0);
  ADD_CG_TEST(test_read_texture_formats, 0);
  ADD_CG_TEST(test_write_texture_formats, 0);
  ADD_CG_TEST(test_alpha_textures, 0);

  ADD_CG_TEST(test_primitive, 0);

  ADD_CG_TEST(test_just_vertex_shader, 0);
  ADD_CG_TEST(test_pipeline_uniforms, 0);
  ADD_CG_TEST(test_snippets, 0);
  ADD_CG_TEST(test_custom_attributes, 0);

  ADD_CG_TEST(test_offscreen, 0);
  ADD_CG_TEST(test_framebuffer_get_bits,
           TEST_CG_REQUIREMENT_OFFSCREEN | TEST_CG_REQUIREMENT_GL);

  ADD_CG_TEST(test_point_size, 0);
  ADD_CG_TEST(test_point_size_attribute,
           TEST_CG_REQUIREMENT_PER_VERTEX_POINT_SIZE);
  ADD_CG_TEST(test_point_size_attribute_snippet,
           TEST_CG_REQUIREMENT_PER_VERTEX_POINT_SIZE);
  ADD_CG_TEST(test_point_sprite,
           TEST_CG_REQUIREMENT_POINT_SPRITE);
  ADD_CG_TEST(test_point_sprite_orientation,
           TEST_CG_REQUIREMENT_POINT_SPRITE);
  ADD_CG_TEST(test_point_sprite_glsl,
           TEST_CG_REQUIREMENT_POINT_SPRITE);

  ADD_CG_TEST(test_version, 0);

  ADD_CG_TEST(test_alpha_test, 0);

  ADD_CG_TEST(test_map_buffer_range, TEST_CG_REQUIREMENT_MAP_WRITE);

  ADD_CG_TEST(test_primitive_and_journal, 0);

  ADD_CG_TEST(test_copy_replace_texture, 0);

  ADD_CG_TEST(test_pipeline_cache_unrefs_texture, 0);
  ADD_CG_TEST(test_pipeline_shader_state, 0);

  ADD_CG_TEST(test_gles2_context, TEST_CG_REQUIREMENT_GLES2_CONTEXT);
  ADD_CG_TEST(test_gles2_context_fbo, TEST_CG_REQUIREMENT_GLES2_CONTEXT);
  ADD_CG_TEST(test_gles2_context_copy_tex_image,
           TEST_CG_REQUIREMENT_GLES2_CONTEXT);

  ADD_CG_TEST(test_euler_quaternion, 0);
  ADD_CG_TEST(test_color_hsl, 0);

#if defined(CG_HAS_GLIB_SUPPORT)
  ADD_CG_TEST(test_fence, TEST_CG_REQUIREMENT_FENCE);
#endif

  ADD_CG_TEST(test_texture_no_allocate, 0);

  ADD_CG_TEST(test_texture_rg, TEST_CG_REQUIREMENT_TEXTURE_RG);

  c_printerr("Unknown test name \"%s\"\n", argv[1]);

  return 1;
}
