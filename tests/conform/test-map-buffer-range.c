#include <config.h>

#include <cglib/cglib.h>

#include <string.h>

#include "test-cg-fixtures.h"

static uint8_t
tex_data[2 * 2 * 4] =
  {
    0xff, 0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
    0x00, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff
  };

/* Vertex data for a quad with all of the texture coordinates set to
 * the top left (red) pixel */
static cg_vertex_p2t2_t
vertex_data[4] =
  {
    { -1, -1, 0, 0 },
    { 1, -1, 0, 0 },
    { -1, 1, 0, 0 },
    { 1, 1, 0, 0 }
  };

void
test_map_buffer_range (void)
{
  cg_texture_2d_t *tex;
  cg_pipeline_t *pipeline;
  int fb_width, fb_height;
  cg_attribute_buffer_t *buffer;
  cg_vertex_p2t2_t *data;
  cg_attribute_t *pos_attribute;
  cg_attribute_t *tex_coord_attribute;
  cg_primitive_t *primitive;

  tex = cg_texture_2d_new_from_data (test_dev,
                                       2, 2, /* width/height */
                                       CG_PIXEL_FORMAT_RGBA_8888_PRE,
                                       2 * 4, /* rowstride */
                                       tex_data,
                                       NULL /* error */);

  pipeline = cg_pipeline_new (test_dev);

  cg_pipeline_set_layer_texture (pipeline, 0, tex);
  cg_pipeline_set_layer_filters (pipeline,
                                   0, /* layer */
                                   CG_PIPELINE_FILTER_NEAREST,
                                   CG_PIPELINE_FILTER_NEAREST);
  cg_pipeline_set_layer_wrap_mode (pipeline,
                                     0, /* layer */
                                     CG_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);

  fb_width = cg_framebuffer_get_width (test_fb);
  fb_height = cg_framebuffer_get_height (test_fb);

  buffer = cg_attribute_buffer_new (test_dev,
                                      sizeof (vertex_data),
                                      vertex_data);

  /* Replace the texture coordinates of the third vertex with the
   * coordinates for a green texel */
  data = cg_buffer_map_range (buffer,
                                sizeof (vertex_data[0]) * 2,
                                sizeof (vertex_data[0]),
                                CG_BUFFER_ACCESS_WRITE,
                                CG_BUFFER_MAP_HINT_DISCARD_RANGE,
                                NULL); /* don't catch errors */
  c_assert (data != NULL);

  data->x = vertex_data[2].x;
  data->y = vertex_data[2].y;
  data->s = 1.0f;
  data->t = 0.0f;

  cg_buffer_unmap (buffer);

  pos_attribute =
    cg_attribute_new (buffer,
                        "cg_position_in",
                        sizeof (vertex_data[0]),
                        offsetof (cg_vertex_p2t2_t, x),
                        2, /* n_components */
                        CG_ATTRIBUTE_TYPE_FLOAT);
  tex_coord_attribute =
    cg_attribute_new (buffer,
                        "cg_tex_coord_in",
                        sizeof (vertex_data[0]),
                        offsetof (cg_vertex_p2t2_t, s),
                        2, /* n_components */
                        CG_ATTRIBUTE_TYPE_FLOAT);

  cg_framebuffer_clear4f (test_fb,
                            CG_BUFFER_BIT_COLOR,
                            0, 0, 0, 1);

  primitive =
    cg_primitive_new (CG_VERTICES_MODE_TRIANGLE_STRIP,
                        4, /* n_vertices */
                        pos_attribute,
                        tex_coord_attribute,
                        NULL);
  cg_primitive_draw (primitive, test_fb, pipeline);
  cg_object_unref (primitive);

  /* Top left pixel should be the one that is replaced to be green */
  test_cg_check_pixel (test_fb, 1, 1, 0x00ff00ff);
  /* The other three corners should be left as red */
  test_cg_check_pixel (test_fb, fb_width - 2, 1, 0xff0000ff);
  test_cg_check_pixel (test_fb, 1, fb_height - 2, 0xff0000ff);
  test_cg_check_pixel (test_fb, fb_width - 2, fb_height - 2, 0xff0000ff);

  cg_object_unref (buffer);
  cg_object_unref (pos_attribute);
  cg_object_unref (tex_coord_attribute);

  cg_object_unref (pipeline);
  cg_object_unref (tex);

  if (test_verbose ())
    c_print ("OK\n");
}
