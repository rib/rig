#include <config.h>

#include <cglib/cglib.h>
#include <string.h>

#include "test-cg-fixtures.h"

/* Keep track of the number of textures that we've created and are
 * still alive */
static int alive_texture_mask = 0;

#define N_LAYERS 3
#define N_PIPELINES 4

#define PIPELINE_LAYER_MASK(pipeline_num)                       \
  (((1 << N_LAYERS) - 1) << (N_LAYERS * (pipeline_num) + 1))
#define LAST_PIPELINE_MASK PIPELINE_LAYER_MASK (N_PIPELINES - 1)
#define FIRST_PIPELINE_MASK PIPELINE_LAYER_MASK (0)

static void
free_texture_cb (void *user_data)
{
  int texture_num = C_POINTER_TO_INT (user_data);

  alive_texture_mask &= ~(1 << texture_num);
}

static cg_texture_t *
create_texture (void)
{
  static const uint8_t data[] =
    { 0xff, 0xff, 0xff, 0xff };
  static cg_user_data_key_t texture_data_key;
  cg_texture_2d_t *tex_2d;
  static int texture_num = 1;

  alive_texture_mask |= (1 << texture_num);

  tex_2d = cg_texture_2d_new_from_data (test_dev,
                                          1, 1, /* width / height */
                                          CG_PIXEL_FORMAT_RGBA_8888_PRE,
                                          4, /* rowstride */
                                          data,
                                          NULL);

  /* Set some user data on the texture so we can track when it has
   * been destroyed */
  cg_object_set_user_data (CG_OBJECT (tex_2d),
                             &texture_data_key,
                             C_INT_TO_POINTER (texture_num),
                             free_texture_cb);

  texture_num++;

  return tex_2d;
}

void
test_copy_replace_texture (void)
{
  cg_pipeline_t *pipelines[N_PIPELINES];
  int pipeline_num;

  /* Create a set of pipeline copies each with three of their own
   * replacement textures */
  for (pipeline_num = 0; pipeline_num < N_PIPELINES; pipeline_num++)
    {
      int layer_num;

      if (pipeline_num == 0)
        pipelines[pipeline_num] = cg_pipeline_new (test_dev);
      else
        pipelines[pipeline_num] =
          cg_pipeline_copy (pipelines[pipeline_num - 1]);

      for (layer_num = 0; layer_num < N_LAYERS; layer_num++)
        {
          cg_texture_t *tex = create_texture ();
          cg_pipeline_set_layer_texture (pipelines[pipeline_num],
                                           layer_num,
                                           tex);
          cg_object_unref (tex);
        }
    }

  /* Unref everything but the last pipeline */
  for (pipeline_num = 0; pipeline_num < N_PIPELINES - 1; pipeline_num++)
    cg_object_unref (pipelines[pipeline_num]);

  if (alive_texture_mask && test_verbose ())
    {
      int i;

      c_print ("Alive textures:");

      for (i = 0; i < N_PIPELINES * N_LAYERS; i++)
        if ((alive_texture_mask & (1 << (i + 1))))
          c_print (" %i", i);

      c_print ("\n");
    }

  /* Ideally there should only be the textures from the last pipeline
   * left alive. We also let Cogl keep the textures from the first
   * texture alive because currently the child of the third layer in
   * the first pipeline will retain its authority on the unit index
   * state so that it can set it to 2. If there are more textures then
   * it means the pipeline isn't correctly pruning redundant
   * ancestors */
  c_assert_cmpint (alive_texture_mask & ~FIRST_PIPELINE_MASK,
                   ==,
                   LAST_PIPELINE_MASK);

  /* Clean up the last pipeline */
  cg_object_unref (pipelines[N_PIPELINES - 1]);

  /* That should get rid of the last of the textures */
  c_assert_cmpint (alive_texture_mask, ==, 0);

  if (test_verbose ())
    c_print ("OK\n");
}
