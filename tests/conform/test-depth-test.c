#include <config.h>

#include <cglib/cglib.h>

#include <string.h>

#include "test-cg-fixtures.h"

#define QUAD_WIDTH 20

#define RED 0
#define GREEN 1
#define BLUE 2
#define ALPHA 3

#define MASK_RED(COLOR)   ((COLOR & 0xff000000) >> 24)
#define MASK_GREEN(COLOR) ((COLOR & 0xff0000) >> 16)
#define MASK_BLUE(COLOR)  ((COLOR & 0xff00) >> 8)
#define MASK_ALPHA(COLOR) (COLOR & 0xff)

typedef struct _TestState
{
  int padding;
} TestState;

typedef struct
{
  uint32_t               color;
  float                 depth;
  bool              test_enable;
  cg_depth_test_function_t test_function;
  bool              write_enable;
  bool              fb_write_enable;
  float                 range_near;
  float                 range_far;
} TestDepthState;

static bool
draw_rectangle (TestState *state,
                int x,
                int y,
                TestDepthState *rect_state)
{
  uint8_t Cr = MASK_RED (rect_state->color);
  uint8_t Cg = MASK_GREEN (rect_state->color);
  uint8_t Cb = MASK_BLUE (rect_state->color);
  uint8_t Ca = MASK_ALPHA (rect_state->color);
  cg_pipeline_t *pipeline;
  cg_depth_state_t depth_state;

  cg_depth_state_init (&depth_state);
  cg_depth_state_set_test_enabled (&depth_state, rect_state->test_enable);
  cg_depth_state_set_test_function (&depth_state, rect_state->test_function);
  cg_depth_state_set_write_enabled (&depth_state, rect_state->write_enable);
  cg_depth_state_set_range (&depth_state,
                              rect_state->range_near,
                              rect_state->range_far);

  pipeline = cg_pipeline_new (test_dev);
  if (!cg_pipeline_set_depth_state (pipeline, &depth_state, NULL))
    {
      cg_object_unref (pipeline);
      return false;
    }

  cg_pipeline_set_color4ub (pipeline, Cr, Cg, Cb, Ca);

  cg_framebuffer_set_depth_write_enabled (test_fb, rect_state->fb_write_enable);
  cg_framebuffer_push_matrix (test_fb);
  cg_framebuffer_translate (test_fb, 0, 0, rect_state->depth);
  cg_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   x * QUAD_WIDTH,
                                   y * QUAD_WIDTH,
                                   x * QUAD_WIDTH + QUAD_WIDTH,
                                   y * QUAD_WIDTH + QUAD_WIDTH);
  cg_framebuffer_pop_matrix (test_fb);

  cg_object_unref (pipeline);

  return true;
}

static void
test_depth (TestState *state,
            int x,
            int y,
            TestDepthState *rect0_state,
            TestDepthState *rect1_state,
            TestDepthState *rect2_state,
            uint32_t expected_result)
{
  bool missing_feature = false;

  if (rect0_state)
    missing_feature |= !draw_rectangle (state, x, y, rect0_state);
  if (rect1_state)
    missing_feature |= !draw_rectangle (state, x, y, rect1_state);
  if (rect2_state)
    missing_feature |= !draw_rectangle (state, x, y, rect2_state);

  /* We don't consider it an error that we can't test something
   * the driver doesn't support. */
  if (missing_feature)
    return;

  test_cg_check_pixel (test_fb,
                          x * QUAD_WIDTH + (QUAD_WIDTH / 2),
                          y * QUAD_WIDTH + (QUAD_WIDTH / 2),
                          expected_result);
}

static void
paint (TestState *state)
{
  /* Sanity check a few of the different depth test functions
   * and that depth writing can be disabled... */

  {
    /* Closest */
    TestDepthState rect0_state = {
      0xff0000ff, /* rgba color */
      -10, /* depth */
      false, /* depth test enable */
      CG_DEPTH_TEST_FUNCTION_ALWAYS,
      true, /* depth write enable */
      true, /* FB depth write enable */
      0, 1 /* depth range */
    };
    /* Furthest */
    TestDepthState rect1_state = {
      0x00ff00ff, /* rgba color */
      -70, /* depth */
      true, /* depth test enable */
      CG_DEPTH_TEST_FUNCTION_ALWAYS,
      true, /* depth write enable */
      true, /* FB depth write enable */
      0, 1 /* depth range */
    };
    /* In the middle */
    TestDepthState rect2_state = {
      0x0000ffff, /* rgba color */
      -20, /* depth */
      true, /* depth test enable */
      CG_DEPTH_TEST_FUNCTION_NEVER,
      true, /* depth write enable */
      true, /* FB depth write enable */
      0, 1 /* depth range */
    };

    test_depth (state, 0, 0, /* position */
                &rect0_state, &rect1_state, &rect2_state,
                0x00ff00ff); /* expected */

    rect2_state.test_function = CG_DEPTH_TEST_FUNCTION_ALWAYS;
    test_depth (state, 1, 0, /* position */
                &rect0_state, &rect1_state, &rect2_state,
                0x0000ffff); /* expected */

    rect2_state.test_function = CG_DEPTH_TEST_FUNCTION_LESS;
    test_depth (state, 2, 0, /* position */
                &rect0_state, &rect1_state, &rect2_state,
                0x0000ffff); /* expected */

    rect2_state.test_function = CG_DEPTH_TEST_FUNCTION_GREATER;
    test_depth (state, 3, 0, /* position */
                &rect0_state, &rect1_state, &rect2_state,
                0x00ff00ff); /* expected */

    rect0_state.test_enable = true;
    rect1_state.write_enable = false;
    test_depth (state, 4, 0, /* position */
                &rect0_state, &rect1_state, &rect2_state,
                0x0000ffff); /* expected */

    rect1_state.write_enable = true;
    rect1_state.fb_write_enable = false;
    test_depth (state, 4, 0, /* position */
                &rect0_state, &rect1_state, &rect2_state,
                0x0000ffff); /* expected */

    /* Re-enable FB depth writing to verify state flush */
    rect1_state.write_enable = true;
    rect1_state.fb_write_enable = true;
    test_depth (state, 4, 0, /* position */
                &rect0_state, &rect1_state, &rect2_state,
                0x00ff00ff); /* expected */
  }

  /* Check that the depth buffer values can be mapped into different
   * ranges... */

  {
    /* Closest by depth, furthest by depth range */
    TestDepthState rect0_state = {
      0xff0000ff, /* rgba color */
      -10, /* depth */
      true, /* depth test enable */
      CG_DEPTH_TEST_FUNCTION_ALWAYS,
      true, /* depth write enable */
      true, /* FB depth write enable */
      0.5, 1 /* depth range */
    };
    /* Furthest by depth, nearest by depth range */
    TestDepthState rect1_state = {
      0x00ff00ff, /* rgba color */
      -70, /* depth */
      true, /* depth test enable */
      CG_DEPTH_TEST_FUNCTION_GREATER,
      true, /* depth write enable */
      true, /* FB depth write enable */
      0, 0.5 /* depth range */
    };

    test_depth (state, 0, 1, /* position */
                &rect0_state, &rect1_state, NULL,
                0xff0000ff); /* expected */
  }
}

void
test_depth_test (void)
{
  TestState state;

  cg_framebuffer_orthographic (test_fb, 0, 0,
                                 cg_framebuffer_get_width (test_fb),
                                 cg_framebuffer_get_height (test_fb),
                                 -1,
                                 100);

  paint (&state);

  if (test_verbose ())
    c_print ("OK\n");
}

