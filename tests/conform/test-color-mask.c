#include <config.h>

#include <cglib/cglib.h>

#include "test-cg-fixtures.h"

#define TEX_SIZE 128

#define NUM_FBOS 3

typedef struct _TestState
{
  int width;
  int height;

  cg_texture_t *tex[NUM_FBOS];
  cg_framebuffer_t *fbo[NUM_FBOS];
} TestState;

static void
paint (TestState *state)
{
  cg_pipeline_t *white = cg_pipeline_new (test_dev);
  int i;

  cg_pipeline_set_color4f (white, 1, 1, 1, 1);


  cg_framebuffer_draw_rectangle (state->fbo[0],
                                   white,
                                   -1.0, -1.0, 1.0, 1.0);

  cg_framebuffer_draw_rectangle (state->fbo[1],
                                   white,
                                   -1.0, -1.0, 1.0, 1.0);

  cg_framebuffer_draw_rectangle (state->fbo[2],
                                   white,
                                   -1.0, -1.0, 1.0, 1.0);

  cg_object_unref (white);

  cg_framebuffer_clear4f (test_fb,
                            CG_BUFFER_BIT_COLOR | CG_BUFFER_BIT_DEPTH,
                            0.5, 0.5, 0.5, 1.0);

  /* Render all of the textures to the screen */
  for (i = 0; i < NUM_FBOS; i++)
    {
      cg_pipeline_t *pipeline = cg_pipeline_new (test_dev);
      cg_pipeline_set_layer_texture (pipeline, 0, state->tex[i]);
      cg_framebuffer_draw_rectangle (test_fb, pipeline,
                                       2.0f / NUM_FBOS * i - 1.0f, -1.0f,
                                       2.0f / NUM_FBOS * (i + 1) - 1.0f, 1.0f);
      cg_object_unref (pipeline);
    }

  /* Verify all of the fbos drew the right color */
  for (i = 0; i < NUM_FBOS; i++)
    {
      uint8_t expected_colors[NUM_FBOS][4] =
        { { 0xff, 0x00, 0x00, 0xff },
          { 0x00, 0xff, 0x00, 0xff },
          { 0x00, 0x00, 0xff, 0xff } };

      test_cg_check_pixel_rgb (test_fb,
                                  state->width * (i + 0.5f) / NUM_FBOS,
                                  state->height / 2,
                                  expected_colors[i][0],
                                  expected_colors[i][1],
                                  expected_colors[i][2]);
    }
}

void
test_color_mask (void)
{
  TestState state;
  int i;

  state.width = cg_framebuffer_get_width (test_fb);
  state.height = cg_framebuffer_get_height (test_fb);

  for (i = 0; i < NUM_FBOS; i++)
    {
      state.tex[i] = test_cg_texture_new_with_size (test_dev, 128, 128,
                                                 TEST_CG_TEXTURE_NO_ATLAS,
                                                 CG_TEXTURE_COMPONENTS_RGB);


      state.fbo[i] = cg_offscreen_new_with_texture (state.tex[i]);

      /* Clear the texture color bits */
      cg_framebuffer_clear4f (state.fbo[i],
                                CG_BUFFER_BIT_COLOR, 0, 0, 0, 1);

      cg_framebuffer_set_color_mask (state.fbo[i],
                                       i == 0 ? CG_COLOR_MASK_RED :
                                       i == 1 ? CG_COLOR_MASK_GREEN :
                                       CG_COLOR_MASK_BLUE);
    }

  paint (&state);

  if (test_verbose ())
    c_print ("OK\n");
}

