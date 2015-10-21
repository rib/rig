#include <config.h>

#include <cglib/cglib.h>

#include "test-cg-fixtures.h"

/* This test assumes the GL driver supports point sizes up to 16
   pixels. Cogl should probably have some way of querying the size so
   we start from that instead */
#define MAX_POINT_SIZE 16
#define MIN_POINT_SIZE 4
#define N_POINTS (MAX_POINT_SIZE - MIN_POINT_SIZE + 1)
/* The size of the area that we'll paint each point in */
#define POINT_BOX_SIZE (MAX_POINT_SIZE * 2)

typedef struct
{
  float x, y;
  float point_size;
} PointVertex;

static int
calc_coord_offset (int pos, int pos_index, int point_size)
{
  switch (pos_index)
    {
    case 0: return pos - point_size / 2 - 2;
    case 1: return pos - point_size / 2 + 2;
    case 2: return pos + point_size / 2 - 2;
    case 3: return pos + point_size / 2 + 2;
    }

  c_assert_not_reached ();
}

static void
verify_point_size (cg_framebuffer_t *test_fb,
                   int x_pos,
                   int y_pos,
                   int point_size)
{
  int y, x;

  for (y = 0; y < 4; y++)
    for (x = 0; x < 4; x++)
      {
        bool in_point = x >= 1 && x <= 2 && y >= 1 && y <= 2;
        uint32_t expected_pixel = in_point ? 0x00ff00ff : 0xff0000ff;

        test_cg_check_pixel (test_fb,
                                calc_coord_offset (x_pos, x, point_size),
                                calc_coord_offset (y_pos, y, point_size),
                                expected_pixel);
      }
}

static cg_primitive_t *
create_primitive (const char *attribute_name)
{
  PointVertex vertices[N_POINTS];
  cg_attribute_buffer_t *buffer;
  cg_attribute_t *attributes[2];
  cg_primitive_t *prim;
  int i;

  for (i = 0; i < N_POINTS; i++)
    {
      vertices[i].x = i * POINT_BOX_SIZE + POINT_BOX_SIZE / 2;
      vertices[i].y = POINT_BOX_SIZE / 2;
      vertices[i].point_size = MAX_POINT_SIZE - i;
    }

  buffer = cg_attribute_buffer_new (test_dev,
                                      sizeof (vertices),
                                      vertices);

  attributes[0] = cg_attribute_new (buffer,
                                      "cg_position_in",
                                      sizeof (PointVertex),
                                      C_STRUCT_OFFSET (PointVertex, x),
                                      2, /* n_components */
                                      CG_ATTRIBUTE_TYPE_FLOAT);
  attributes[1] = cg_attribute_new (buffer,
                                      attribute_name,
                                      sizeof (PointVertex),
                                      C_STRUCT_OFFSET (PointVertex, point_size),
                                      1, /* n_components */
                                      CG_ATTRIBUTE_TYPE_FLOAT);

  prim = cg_primitive_new_with_attributes (CG_VERTICES_MODE_POINTS,
                                             N_POINTS,
                                             attributes,
                                             2 /* n_attributes */);

  for (i = 0; i < 2; i++)
    cg_object_unref (attributes[i]);

  return prim;
}

static void
do_test (const char *attribute_name,
         void (* pipeline_setup_func) (cg_pipeline_t *pipeline))
{
  int fb_width = cg_framebuffer_get_width (test_fb);
  int fb_height = cg_framebuffer_get_height (test_fb);
  cg_primitive_t *primitive;
  cg_pipeline_t *pipeline;
  int i;

  cg_framebuffer_orthographic (test_fb,
                                 0, 0, /* x_1, y_1 */
                                 fb_width, /* x_2 */
                                 fb_height /* y_2 */,
                                 -1, 100 /* near/far */);

  cg_framebuffer_clear4f (test_fb,
                            CG_BUFFER_BIT_COLOR,
                            1.0f, 0.0f, 0.0f, 1.0f);

  primitive = create_primitive (attribute_name);
  pipeline = cg_pipeline_new (test_dev);
  cg_pipeline_set_color4ub (pipeline, 0x00, 0xff, 0x00, 0xff);
  cg_pipeline_set_per_vertex_point_size (pipeline, true, NULL);
  if (pipeline_setup_func)
    pipeline_setup_func (pipeline);
  cg_primitive_draw (primitive, test_fb, pipeline);
  cg_object_unref (pipeline);
  cg_object_unref (primitive);

  /* Verify all of the points where drawn at the right size */
  for (i = 0; i < N_POINTS; i++)
    verify_point_size (test_fb,
                       i * POINT_BOX_SIZE + POINT_BOX_SIZE / 2, /* x */
                       POINT_BOX_SIZE / 2, /* y */
                       MAX_POINT_SIZE - i /* point size */);

  if (test_verbose ())
    c_print ("OK\n");
}

void
test_point_size_attribute (void)
{
  do_test ("cg_point_size_in", NULL);
}

static void
setup_snippet (cg_pipeline_t *pipeline)
{
  cg_snippet_t *snippet;

  snippet = cg_snippet_new (CG_SNIPPET_HOOK_POINT_SIZE,
                              "attribute float "
                              "my_super_duper_point_size_attrib;\n",
                              NULL);
  cg_snippet_set_replace (snippet,
                            "cg_point_size_out = "
                            "my_super_duper_point_size_attrib;\n");
  cg_pipeline_add_snippet (pipeline, snippet);
  cg_object_unref (snippet);
}

void
test_point_size_attribute_snippet (void)
{
  do_test ("my_super_duper_point_size_attrib", setup_snippet);
}
