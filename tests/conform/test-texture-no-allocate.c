#include <config.h>

#include <cglib/cglib.h>

#include "test-cg-fixtures.h"

/* Tests that the various texture types can be freed without being
 * allocated */

/* Texture size that is probably to big to fit within the texture
 * limits */
#define BIG_TEX_WIDTH 16384
#define BIG_TEX_HEIGHT 128

void
test_texture_no_allocate (void)
{
  uint8_t *tex_data;
  cg_texture_t *texture;
  cg_texture_2d_t *texture_2d;
  cg_error_t *error = NULL;

  tex_data = c_malloc (BIG_TEX_WIDTH * BIG_TEX_HEIGHT * 4);

  /* NB: if we make the atlas and sliced texture APIs public then this
   * could changed to explicitly use that instead of the magic texture
   * API */

  /* Try to create an atlas texture that is too big so it will
   * internally be freed without allocating */
  texture =
    cg_atlas_texture_new_from_data (test_dev,
                                      BIG_TEX_WIDTH,
                                      BIG_TEX_HEIGHT,
                                      /* format */
                                      CG_PIXEL_FORMAT_RGBA_8888_PRE,
                                      /* rowstride */
                                      BIG_TEX_WIDTH * 4,
                                      tex_data,
                                      &error);

  c_free (tex_data);

  /* It's ok if this causes an error, we just don't want it to
   * crash */

  if (texture == NULL)
    cg_error_free (error);
  else
    cg_object_unref (texture);

  /* Try to create a sliced texture without allocating it */
  texture =
    cg_texture_2d_sliced_new_with_size (test_dev,
                                          BIG_TEX_WIDTH,
                                          BIG_TEX_HEIGHT,
                                          CG_TEXTURE_MAX_WASTE);
  cg_object_unref (texture);

  /* 2D texture */
  texture_2d = cg_texture_2d_new_with_size (test_dev,
                                              64, 64);
  cg_object_unref (texture_2d);

  /* 3D texture */
  if (cg_has_feature (test_dev, CG_FEATURE_ID_TEXTURE_3D))
    {
      cg_texture_3d_t *texture_3d =
        cg_texture_3d_new_with_size (test_dev,
                                       64, 64, 64);
      cg_object_unref (texture_3d);
    }
}
