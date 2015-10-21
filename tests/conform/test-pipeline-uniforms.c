#include <config.h>

#include <cglib/cglib.h>

#include <string.h>

#include "test-cg-fixtures.h"

#define LONG_ARRAY_SIZE 128

typedef struct _TestState
{
  cg_pipeline_t *pipeline_red;
  cg_pipeline_t *pipeline_green;
  cg_pipeline_t *pipeline_blue;

  cg_pipeline_t *matrix_pipeline;
  cg_pipeline_t *vector_pipeline;
  cg_pipeline_t *int_pipeline;

  cg_pipeline_t *long_pipeline;
  int long_uniform_locations[LONG_ARRAY_SIZE];
} TestState;

static const char *
color_declarations = "uniform float red, green, blue;\n";

static const char *
color_fragment_source = "  cg_color_out = vec4 (red, green, blue, 1.0);\n";

static const char *
matrix_declarations = "uniform mat4 matrix_array[4];\n";

static const char *
matrix_fragment_source =
  "  vec4 color = vec4 (0.0, 0.0, 0.0, 1.0);\n"
  "  int i;\n"
  "\n"
  "  for (i = 0; i < 4; i++)\n"
  "    color = matrix_array[i] * color;\n"
  "\n"
  "  cg_color_out = color;\n";

static const char *
vector_declarations =
  "uniform vec4 vector_array[2];\n"
  "uniform vec3 short_vector;\n";

static const char *
vector_fragment_source =
  "  cg_color_out = (vector_array[0] +\n"
  "                    vector_array[1] +\n"
  "                    vec4 (short_vector, 1.0));\n";

static const char *
int_declarations =
  "uniform ivec4 vector_array[2];\n"
  "uniform int single_value;\n";

static const char *
int_fragment_source =
  "  cg_color_out = (vec4 (vector_array[0]) +\n"
  "                    vec4 (vector_array[1]) +\n"
  "                    vec4 (float (single_value), 0.0, 0.0, 255.0)) / 255.0;\n";

static const char *
long_declarations =
  "uniform int long_array[" C_STRINGIFY (LONG_ARRAY_SIZE) "];\n"
  "const int last_index = " C_STRINGIFY (LONG_ARRAY_SIZE) " - 1;\n";

static const char *
long_fragment_source =
  "  cg_color_out = vec4 (float (long_array[last_index]), 0.0, 0.0, 1.0);\n";

static cg_pipeline_t *
create_pipeline_for_shader (TestState *state,
                            const char *declarations,
                            const char *fragment_source)
{
  cg_pipeline_t *pipeline = cg_pipeline_new (test_dev);
  cg_snippet_t *snippet = cg_snippet_new (CG_SNIPPET_HOOK_FRAGMENT,
                                           declarations,
                                           NULL);
  cg_snippet_set_replace (snippet, fragment_source);

  cg_pipeline_add_snippet (pipeline, snippet);
  cg_object_unref (snippet);

  return pipeline;
}

static void
init_state (TestState *state)
{
  int uniform_location;

  state->pipeline_red = create_pipeline_for_shader (state,
                                                    color_declarations,
                                                    color_fragment_source);

  uniform_location =
    cg_pipeline_get_uniform_location (state->pipeline_red, "red");
  cg_pipeline_set_uniform_1f (state->pipeline_red, uniform_location, 1.0f);
  uniform_location =
    cg_pipeline_get_uniform_location (state->pipeline_red, "green");
  cg_pipeline_set_uniform_1f (state->pipeline_red, uniform_location, 0.0f);
  uniform_location =
    cg_pipeline_get_uniform_location (state->pipeline_red, "blue");
  cg_pipeline_set_uniform_1f (state->pipeline_red, uniform_location, 0.0f);

  state->pipeline_green = cg_pipeline_copy (state->pipeline_red);
  uniform_location =
    cg_pipeline_get_uniform_location (state->pipeline_green, "green");
  cg_pipeline_set_uniform_1f (state->pipeline_green, uniform_location, 1.0f);

  state->pipeline_blue = cg_pipeline_copy (state->pipeline_red);
  uniform_location =
    cg_pipeline_get_uniform_location (state->pipeline_blue, "blue");
  cg_pipeline_set_uniform_1f (state->pipeline_blue, uniform_location, 1.0f);

  state->matrix_pipeline = create_pipeline_for_shader (state,
                                                       matrix_declarations,
                                                       matrix_fragment_source);
  state->vector_pipeline = create_pipeline_for_shader (state,
                                                       vector_declarations,
                                                       vector_fragment_source);
  state->int_pipeline = create_pipeline_for_shader (state,
                                                    int_declarations,
                                                    int_fragment_source);

  state->long_pipeline = NULL;
}

static void
init_long_pipeline_state (TestState *state)
{
  int i;

  state->long_pipeline = create_pipeline_for_shader (state,
                                                     long_declarations,
                                                     long_fragment_source);

  /* This tries to lookup a large number of uniform names to make sure
     that the bitmask of overriden uniforms flows over the size of a
     single long so that it has to resort to allocating it */
  for (i = 0; i < LONG_ARRAY_SIZE; i++)
    {
      char *uniform_name = c_strdup_printf ("long_array[%i]", i);
      state->long_uniform_locations[i] =
        cg_pipeline_get_uniform_location (state->long_pipeline,
                                            uniform_name);
      c_free (uniform_name);
    }
}

static void
destroy_state (TestState *state)
{
  cg_object_unref (state->pipeline_red);
  cg_object_unref (state->pipeline_green);
  cg_object_unref (state->pipeline_blue);
  cg_object_unref (state->matrix_pipeline);
  cg_object_unref (state->vector_pipeline);
  cg_object_unref (state->int_pipeline);

  if (state->long_pipeline)
    cg_object_unref (state->long_pipeline);
}

static void
paint_pipeline (cg_pipeline_t *pipeline, int pos)
{
  cg_framebuffer_draw_rectangle (test_fb, pipeline,
                                   pos * 10, 0, pos * 10 + 10, 10);
}

static void
paint_color_pipelines (TestState *state)
{
  cg_pipeline_t *temp_pipeline;
  int uniform_location;
  int i;

  /* Paint with the first pipeline that sets the uniforms to bright
     red */
  paint_pipeline (state->pipeline_red, 0);

  /* Paint with the two other pipelines. These inherit from the red
     pipeline and only override one other component. The values for
     the two other components should be inherited from the red
     pipeline. */
  paint_pipeline (state->pipeline_green, 1);
  paint_pipeline (state->pipeline_blue, 2);

  /* Try modifying a single pipeline for multiple rectangles */
  temp_pipeline = cg_pipeline_copy (state->pipeline_green);
  uniform_location = cg_pipeline_get_uniform_location (temp_pipeline,
                                                         "green");

  for (i = 0; i <= 8; i++)
    {
      cg_pipeline_set_uniform_1f (temp_pipeline, uniform_location,
                                    i / 8.0f);
      paint_pipeline (temp_pipeline, i + 3);
    }

  cg_object_unref (temp_pipeline);
}

static void
paint_matrix_pipeline (cg_pipeline_t *pipeline)
{
  c_matrix_t matrices[4];
  float matrix_floats[16 * 4];
  int uniform_location;
  int i;

  for (i = 0; i < 4; i++)
    c_matrix_init_identity (matrices + i);

  /* Use the first matrix to make the color red */
  c_matrix_translate (matrices + 0, 1.0f, 0.0f, 0.0f);

  /* Rotate the vertex so that it ends up green */
  c_matrix_rotate (matrices + 1, 90.0f, 0.0f, 0.0f, 1.0f);

  /* Scale the vertex so it ends up halved */
  c_matrix_scale (matrices + 2, 0.5f, 0.5f, 0.5f);

  /* Add a blue component in the final matrix. The final matrix is
     uploaded as transposed so we need to transpose first to cancel
     that out */
  c_matrix_translate (matrices + 3, 0.0f, 0.0f, 1.0f);
  c_matrix_transpose (matrices + 3);

  for (i = 0; i < 4; i++)
    memcpy (matrix_floats + i * 16,
            c_matrix_get_array (matrices + i),
            sizeof (float) * 16);

  /* Set the first three matrices as transposed */
  uniform_location =
    cg_pipeline_get_uniform_location (pipeline, "matrix_array");
  cg_pipeline_set_uniform_matrix (pipeline,
                                    uniform_location,
                                    4, /* dimensions */
                                    3, /* count */
                                    false, /* not transposed */
                                    matrix_floats);

  /* Set the last matrix as untransposed */
  uniform_location =
    cg_pipeline_get_uniform_location (pipeline, "matrix_array[3]");
  cg_pipeline_set_uniform_matrix (pipeline,
                                    uniform_location,
                                    4, /* dimensions */
                                    1, /* count */
                                    true, /* transposed */
                                    matrix_floats + 16 * 3);

  paint_pipeline (pipeline, 12);
}

static void
paint_vector_pipeline (cg_pipeline_t *pipeline)
{
  float vector_array_values[] = { 1.0f, 0.0f, 0.0f, 0.0f,
                                  0.0f, 1.0f, 0.0f, 0.0f };
  float short_vector_values[] = { 0.0f, 0.0f, 1.0f };
  int uniform_location;

  uniform_location =
    cg_pipeline_get_uniform_location (pipeline, "vector_array");
  cg_pipeline_set_uniform_float (pipeline,
                                   uniform_location,
                                   4, /* n_components */
                                   2, /* count */
                                   vector_array_values);

  uniform_location =
    cg_pipeline_get_uniform_location (pipeline, "short_vector");
  cg_pipeline_set_uniform_float (pipeline,
                                   uniform_location,
                                   3, /* n_components */
                                   1, /* count */
                                   short_vector_values);

  paint_pipeline (pipeline, 13);
}

static void
paint_int_pipeline (cg_pipeline_t *pipeline)
{
  int vector_array_values[] = { 0x00, 0x00, 0xff, 0x00,
                                0x00, 0xff, 0x00, 0x00 };
  int single_value = 0x80;
  int uniform_location;

  uniform_location =
    cg_pipeline_get_uniform_location (pipeline, "vector_array");
  cg_pipeline_set_uniform_int (pipeline,
                                 uniform_location,
                                 4, /* n_components */
                                 2, /* count */
                                 vector_array_values);

  uniform_location =
    cg_pipeline_get_uniform_location (pipeline, "single_value");
  cg_pipeline_set_uniform_1i (pipeline,
                                uniform_location,
                                single_value);

  paint_pipeline (pipeline, 14);
}

static void
paint_long_pipeline (TestState *state)
{
  int i;

  for (i = 0; i < LONG_ARRAY_SIZE; i++)
    {
      int location = state->long_uniform_locations[i];

      cg_pipeline_set_uniform_1i (state->long_pipeline,
                                    location,
                                    i == LONG_ARRAY_SIZE - 1);
    }

  paint_pipeline (state->long_pipeline, 15);
}

static void
paint (TestState *state)
{
  cg_framebuffer_clear4f (test_fb, CG_BUFFER_BIT_COLOR, 0, 0, 0, 1);

  paint_color_pipelines (state);
  paint_matrix_pipeline (state->matrix_pipeline);
  paint_vector_pipeline (state->vector_pipeline);
  paint_int_pipeline (state->int_pipeline);
}

static void
check_pos (int pos, uint32_t color)
{
  test_cg_check_pixel (test_fb, pos * 10 + 5, 5, color);
}

static void
validate_result (void)
{
  int i;

  check_pos (0, 0xff0000ff);
  check_pos (1, 0xffff00ff);
  check_pos (2, 0xff00ffff);

  for (i = 0; i <= 8; i++)
    {
      int green_value = i / 8.0f * 255.0f + 0.5f;
      check_pos (i + 3, 0xff0000ff + (green_value << 16));
    }

  check_pos (12, 0x0080ffff);
  check_pos (13, 0xffffffff);
  check_pos (14, 0x80ffffff);
}

static void
validate_long_pipeline_result (void)
{
  check_pos (15, 0xff0000ff);
}

void
test_pipeline_uniforms (void)
{
  TestState state;

  init_state (&state);

  cg_framebuffer_orthographic (test_fb,
                                 0, 0,
                                 cg_framebuffer_get_width (test_fb),
                                 cg_framebuffer_get_height (test_fb),
                                 -1,
                                 100);

  paint (&state);
  validate_result ();

  /* Try the test again after querying the location of a large
     number of uniforms. This should verify that the bitmasks
     still work even if they have to allocate a separate array to
     store the bits */

  init_long_pipeline_state (&state);
  paint (&state);
  paint_long_pipeline (&state);
  validate_result ();
  validate_long_pipeline_result ();

  destroy_state (&state);

  if (test_verbose ())
    c_print ("OK\n");
}
