#include <config.h>

#include <cglib/cglib.h>

#include <string.h>

#include "test-cg-fixtures.h"

/* Size the texture so that it is just off a power of two to encourage
   it so use software tiling when NPOTs aren't available */
#define TEXTURE_SIZE        257

/* Amount of pixels to skip off the top, bottom, left and right of the
   texture when reading back the stage */
#define TEST_INSET          2

/* Size to actually render the texture at */
#define TEXTURE_RENDER_SIZE 8

typedef struct _TestState
{
  cg_texture_t *texture;
  cg_framebuffer_t *offscreen;
  cg_texture_t *offscreen_tex;
  int width, height;
} TestState;

static void
validate_part (cg_framebuffer_t *framebuffer,
               int xnum, int ynum, bool shown)
{
  test_cg_check_region (framebuffer,
                           xnum * TEXTURE_RENDER_SIZE + TEST_INSET,
                           ynum * TEXTURE_RENDER_SIZE + TEST_INSET,
                           TEXTURE_RENDER_SIZE - TEST_INSET * 2,
                           TEXTURE_RENDER_SIZE - TEST_INSET * 2,
                           shown ? 0xff0000ff : 0x000000ff);
}

/* We draw everything 8 times. The draw number is used as a bitmask
   to test all of the combinations of both winding orders and all four
   culling modes */

#define FRONT_WINDING(draw_num)    ((draw_num) & 0x01)
#define CULL_FACE_MODE(draw_num)   (((draw_num) & 0x06) >> 1)

static void
paint_test_backface_culling (TestState *state,
                             cg_framebuffer_t *framebuffer)
{
  int draw_num;
  cg_pipeline_t *base_pipeline = cg_pipeline_new (test_dev);

  cg_framebuffer_orthographic (framebuffer,
                                 0, 0,
                                 state->width,
                                 state->height,
                                 -1,
                                 100);

  cg_framebuffer_clear4f (framebuffer,
                            CG_BUFFER_BIT_COLOR | CG_BUFFER_BIT_STENCIL,
                            0, 0, 0, 1);

  cg_pipeline_set_layer_texture (base_pipeline, 0, state->texture);

  cg_pipeline_set_layer_filters (base_pipeline, 0,
                                   CG_PIPELINE_FILTER_NEAREST,
                                   CG_PIPELINE_FILTER_NEAREST);

  /* Render the scene eight times to test all of the combinations of
     cull face mode and winding orders */
  for (draw_num = 0; draw_num < 8; draw_num++)
    {
      float x1 = 0, x2, y1 = 0, y2 = (float)(TEXTURE_RENDER_SIZE);
      cg_pipeline_t *pipeline;

      cg_framebuffer_push_matrix (framebuffer);
      cg_framebuffer_translate (framebuffer,
                                  0, TEXTURE_RENDER_SIZE * draw_num, 0);

      pipeline = cg_pipeline_copy (base_pipeline);

      cg_pipeline_set_front_face_winding (pipeline, FRONT_WINDING (draw_num));
      cg_pipeline_set_cull_face_mode (pipeline, CULL_FACE_MODE (draw_num));

      x2 = x1 + (float)(TEXTURE_RENDER_SIZE);

      /* Draw a front-facing texture */
      cg_framebuffer_draw_rectangle (framebuffer, pipeline, x1, y1, x2, y2);

      x1 = x2;
      x2 = x1 + (float)(TEXTURE_RENDER_SIZE);

      /* Draw a front-facing texture with flipped texcoords */
      cg_framebuffer_draw_textured_rectangle (framebuffer,
                                                pipeline,
                                                x1, y1, x2, y2,
                                                1.0, 0.0, 0.0, 1.0);

      x1 = x2;
      x2 = x1 + (float)(TEXTURE_RENDER_SIZE);

      /* Draw a back-facing texture */
      cg_framebuffer_draw_rectangle (framebuffer, pipeline, x2, y1, x1, y2);

      cg_framebuffer_pop_matrix (framebuffer);

      cg_object_unref (pipeline);
    }

  cg_object_unref (base_pipeline);
}

static void
validate_result (cg_framebuffer_t *framebuffer, int y_offset)
{
  int draw_num;

  for (draw_num = 0; draw_num < 8; draw_num++)
    {
      bool cull_front, cull_back;
      cg_pipeline_cull_face_mode_t cull_mode;

      cull_mode = CULL_FACE_MODE (draw_num);

      switch (cull_mode)
        {
        case CG_PIPELINE_CULL_FACE_MODE_NONE:
          cull_front = false;
          cull_back = false;
          break;

        case CG_PIPELINE_CULL_FACE_MODE_FRONT:
          cull_front = true;
          cull_back = false;
          break;

        case CG_PIPELINE_CULL_FACE_MODE_BACK:
          cull_front = false;
          cull_back = true;
          break;

        case CG_PIPELINE_CULL_FACE_MODE_BOTH:
          cull_front = true;
          cull_back = true;
          break;
        }

      if (FRONT_WINDING (draw_num) == CG_WINDING_CLOCKWISE)
        {
          bool tmp = cull_front;
          cull_front = cull_back;
          cull_back = tmp;
        }

      /* Front-facing texture */
      validate_part (framebuffer,
                     0, y_offset + draw_num, !cull_front);
      /* Front-facing texture with flipped tex coords */
      validate_part (framebuffer,
                     1, y_offset + draw_num, !cull_front);
      /* Back-facing texture */
      validate_part (framebuffer,
                     2, y_offset + draw_num, !cull_back);
    }
}

static void
paint (TestState *state)
{
  cg_pipeline_t *pipeline;

  paint_test_backface_culling (state, test_fb);

  /*
   * Now repeat the test but rendered to an offscreen
   * framebuffer. Note that by default the conformance tests are
   * always run to an offscreen buffer but we might as well have this
   * check anyway in case it is being run with CG_TEST_ONSCREEN=1
   */
  paint_test_backface_culling (state, state->offscreen);

  /* Copy the result of the offscreen rendering for validation and
   * also so we can have visual feedback. */
  pipeline = cg_pipeline_new (test_dev);
  cg_pipeline_set_layer_texture (pipeline, 0, state->offscreen_tex);
  cg_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   0, TEXTURE_RENDER_SIZE * 16,
                                   state->width,
                                   state->height + TEXTURE_RENDER_SIZE * 16);
  cg_object_unref (pipeline);

  validate_result (test_fb, 0);
  validate_result (test_fb, 16);
}

static cg_texture_t *
make_texture (void)
{
  uint8_t *tex_data, *p;
  cg_texture_t *tex;

  tex_data = c_malloc (TEXTURE_SIZE * TEXTURE_SIZE * 4);

  for (p = tex_data + TEXTURE_SIZE * TEXTURE_SIZE * 4; p > tex_data;)
    {
      *(--p) = 255;
      *(--p) = 0;
      *(--p) = 0;
      *(--p) = 255;
    }

  tex = test_cg_texture_new_from_data (test_dev,
                                          TEXTURE_SIZE,
                                          TEXTURE_SIZE,
                                          TEST_CG_TEXTURE_NO_ATLAS,
                                          CG_PIXEL_FORMAT_RGBA_8888,
                                          TEXTURE_SIZE * 4,
                                          tex_data);

  c_free (tex_data);

  return tex;
}

void
test_backface_culling (void)
{
  TestState state;
  cg_texture_t *tex;

  state.width = cg_framebuffer_get_width (test_fb);
  state.height = cg_framebuffer_get_height (test_fb);

  state.offscreen = NULL;

  state.texture = make_texture ();

  tex = test_cg_texture_new_with_size (test_dev,
                                          state.width, state.height,
                                          TEST_CG_TEXTURE_NO_SLICING,
                                          CG_TEXTURE_COMPONENTS_RGBA);
  state.offscreen = cg_offscreen_new_with_texture (tex);
  state.offscreen_tex = tex;

  paint (&state);

  cg_object_unref (state.offscreen);
  cg_object_unref (state.offscreen_tex);
  cg_object_unref (state.texture);

  if (test_verbose ())
    c_print ("OK\n");
}

