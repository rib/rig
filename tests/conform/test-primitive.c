#include <config.h>

#include <cglib/cglib.h>
#include <string.h>
#include <stdlib.h>

#include "test-cg-fixtures.h"

typedef struct _TestState
{
  int fb_width;
  int fb_height;
} TestState;

#define PRIM_COLOR 0xff00ffff
#define TEX_COLOR 0x0000ffff

#define N_ATTRIBS 8

typedef cg_primitive_t * (* TestPrimFunc) (cg_device_t *dev, uint32_t *expected_color);

static cg_primitive_t *
test_prim_p2 (cg_device_t *dev, uint32_t *expected_color)
{
  static const cg_vertex_p2_t verts[] =
    { { 0, 0 }, { 0, 10 }, { 10, 0 } };

  return cg_primitive_new_p2 (dev,
                                CG_VERTICES_MODE_TRIANGLES,
                                3, /* n_vertices */
                                verts);
}

static cg_primitive_t *
test_prim_p3 (cg_device_t *dev, uint32_t *expected_color)
{
  static const cg_vertex_p3_t verts[] =
    { { 0, 0, 0 }, { 0, 10, 0 }, { 10, 0, 0 } };

  return cg_primitive_new_p3 (dev,
                                CG_VERTICES_MODE_TRIANGLES,
                                3, /* n_vertices */
                                verts);
}

static cg_primitive_t *
test_prim_p2c4 (cg_device_t *dev, uint32_t *expected_color)
{
  static const cg_vertex_p2c4_t verts[] =
    { { 0, 0, 255, 255, 0, 255 },
      { 0, 10, 255, 255, 0, 255 },
      { 10, 0, 255, 255, 0, 255 } };

  *expected_color = 0xffff00ff;

  return cg_primitive_new_p2c4 (dev,
                                  CG_VERTICES_MODE_TRIANGLES,
                                  3, /* n_vertices */
                                  verts);
}

static cg_primitive_t *
test_prim_p3c4 (cg_device_t *dev, uint32_t *expected_color)
{
  static const cg_vertex_p3c4_t verts[] =
    { { 0, 0, 0, 255, 255, 0, 255 },
      { 0, 10, 0, 255, 255, 0, 255 },
      { 10, 0, 0, 255, 255, 0, 255 } };

  *expected_color = 0xffff00ff;

  return cg_primitive_new_p3c4 (dev,
                                  CG_VERTICES_MODE_TRIANGLES,
                                  3, /* n_vertices */
                                  verts);
}

static cg_primitive_t *
test_prim_p2t2 (cg_device_t *dev, uint32_t *expected_color)
{
  static const cg_vertex_p2t2_t verts[] =
    { { 0, 0, 1, 0 },
      { 0, 10, 1, 0 },
      { 10, 0, 1, 0 } };

  *expected_color = TEX_COLOR;

  return cg_primitive_new_p2t2 (dev,
                                  CG_VERTICES_MODE_TRIANGLES,
                                  3, /* n_vertices */
                                  verts);
}

static cg_primitive_t *
test_prim_p3t2 (cg_device_t *dev, uint32_t *expected_color)
{
  static const cg_vertex_p3t2_t verts[] =
    { { 0, 0, 0, 1, 0 },
      { 0, 10, 0, 1, 0 },
      { 10, 0, 0, 1, 0 } };

  *expected_color = TEX_COLOR;

  return cg_primitive_new_p3t2 (dev,
                                  CG_VERTICES_MODE_TRIANGLES,
                                  3, /* n_vertices */
                                  verts);
}

static cg_primitive_t *
test_prim_p2t2c4 (cg_device_t *dev, uint32_t *expected_color)
{
  static const cg_vertex_p2t2c4_t verts[] =
    { { 0, 0, 1, 0, 0xff, 0xff, 0xf0, 0xff },
      { 0, 10, 1, 0, 0xff, 0xff, 0xf0, 0xff },
      { 10, 0, 1, 0, 0xff, 0xff, 0xf0, 0xff } };

  /* The blue component of the texture color should be replaced with 0xf0 */
  *expected_color = (TEX_COLOR & 0xffff00ff) | 0x0000f000;

  return cg_primitive_new_p2t2c4 (dev,
                                    CG_VERTICES_MODE_TRIANGLES,
                                    3, /* n_vertices */
                                    verts);
}

static cg_primitive_t *
test_prim_p3t2c4 (cg_device_t *dev, uint32_t *expected_color)
{
  static const cg_vertex_p3t2c4_t verts[] =
    { { 0, 0, 0, 1, 0, 0xff, 0xff, 0xf0, 0xff },
      { 0, 10, 0, 1, 0, 0xff, 0xff, 0xf0, 0xff },
      { 10, 0, 0, 1, 0, 0xff, 0xff, 0xf0, 0xff } };

  /* The blue component of the texture color should be replaced with 0xf0 */
  *expected_color = (TEX_COLOR & 0xffff00ff) | 0x0000f000;

  return cg_primitive_new_p3t2c4 (dev,
                                    CG_VERTICES_MODE_TRIANGLES,
                                    3, /* n_vertices */
                                    verts);
}

static const TestPrimFunc
test_prim_funcs[] =
  {
    test_prim_p2,
    test_prim_p3,
    test_prim_p2c4,
    test_prim_p3c4,
    test_prim_p2t2,
    test_prim_p3t2,
    test_prim_p2t2c4,
    test_prim_p3t2c4
  };

static void
test_paint (TestState *state)
{
  cg_pipeline_t *pipeline;
  cg_texture_t *tex;
  uint8_t tex_data[6];
  int i;

  /* Create a two pixel texture. The first pixel is white and the
     second pixel is tex_color. The assumption is that if no texture
     coordinates are specified then it will default to 0,0 and get
     white */
  tex_data[0] = 255;
  tex_data[1] = 255;
  tex_data[2] = 255;
  tex_data[3] = (TEX_COLOR >> 24) & 0xff;
  tex_data[4] = (TEX_COLOR >> 16) & 0xff;
  tex_data[5] = (TEX_COLOR >> 8) & 0xff;
  tex = test_cg_texture_new_from_data (test_dev,
                                          2, 1, /* size */
                                          TEST_CG_TEXTURE_NO_ATLAS,
                                          CG_PIXEL_FORMAT_RGB_888,
                                          6, /* rowstride */
                                          tex_data);
  pipeline = cg_pipeline_new (test_dev);
  cg_pipeline_set_color4ub (pipeline,
                              (PRIM_COLOR >> 24) & 0xff,
                              (PRIM_COLOR >> 16) & 0xff,
                              (PRIM_COLOR >> 8) & 0xff,
                              (PRIM_COLOR >> 0) & 0xff);
  cg_pipeline_set_layer_texture (pipeline, 0, tex);
  cg_object_unref (tex);

  for (i = 0; i < C_N_ELEMENTS (test_prim_funcs); i++)
    {
      cg_primitive_t *prim;
      uint32_t expected_color = PRIM_COLOR;

      prim = test_prim_funcs[i] (test_dev, &expected_color);

      cg_framebuffer_push_matrix (test_fb);
      cg_framebuffer_translate (test_fb, i * 10, 0, 0);
      cg_primitive_draw (prim, test_fb, pipeline);
      cg_framebuffer_pop_matrix (test_fb);

      test_cg_check_pixel (test_fb, i * 10 + 2, 2, expected_color);

      cg_object_unref (prim);
    }

  cg_object_unref (pipeline);
}

static bool
get_attributes_cb (cg_primitive_t *prim,
                   cg_attribute_t *attrib,
                   void *user_data)
{
  cg_attribute_t ***p = user_data;
  *((* p)++) = attrib;
  return true;
}

static int
compare_pointers (const void *a, const void *b)
{
  cg_attribute_t *pa = *(cg_attribute_t **) a;
  cg_attribute_t *pb = *(cg_attribute_t **) b;

  if (pa < pb)
    return -1;
  else if (pa > pb)
    return 1;
  else
    return 0;
}

static void
test_copy (TestState *state)
{
  static const uint16_t indices_data[2] = { 1, 2 };
  cg_attribute_buffer_t *buffer =
    cg_attribute_buffer_new_with_size (test_dev, 100);
  cg_attribute_t *attributes[N_ATTRIBS];
  cg_attribute_t *attributes_a[N_ATTRIBS], *attributes_b[N_ATTRIBS];
  cg_attribute_t **p;
  cg_primitive_t *prim_a, *prim_b;
  cg_indices_t *indices;
  int i;

  for (i = 0; i < N_ATTRIBS; i++)
    {
      char *name = c_strdup_printf ("foo_%i", i);
      attributes[i] = cg_attribute_new (buffer,
                                          name,
                                          16, /* stride */
                                          16, /* offset */
                                          2, /* components */
                                          CG_ATTRIBUTE_TYPE_FLOAT);
      c_free (name);
    }

  prim_a = cg_primitive_new_with_attributes (CG_VERTICES_MODE_TRIANGLES,
                                               8, /* n_vertices */
                                               attributes,
                                               N_ATTRIBS);

  indices = cg_indices_new (test_dev,
                              CG_INDICES_TYPE_UNSIGNED_SHORT,
                              indices_data,
                              2 /* n_indices */);

  cg_primitive_set_first_vertex (prim_a, 12);
  cg_primitive_set_indices (prim_a, indices, 2);

  prim_b = cg_primitive_copy (prim_a);

  p = attributes_a;
  cg_primitive_foreach_attribute (prim_a,
                                    get_attributes_cb,
                                    &p);
  c_assert_cmpint (p - attributes_a, ==, N_ATTRIBS);

  p = attributes_b;
  cg_primitive_foreach_attribute (prim_b,
                                    get_attributes_cb,
                                    &p);
  c_assert_cmpint (p - attributes_b, ==, N_ATTRIBS);

  qsort (attributes_a, N_ATTRIBS, sizeof (cg_attribute_t *), compare_pointers);
  qsort (attributes_b, N_ATTRIBS, sizeof (cg_attribute_t *), compare_pointers);

  c_assert (memcmp (attributes_a, attributes_b, sizeof (attributes_a)) == 0);

  c_assert_cmpint (cg_primitive_get_first_vertex (prim_a),
                   ==,
                   cg_primitive_get_first_vertex (prim_b));

  c_assert_cmpint (cg_primitive_get_n_vertices (prim_a),
                   ==,
                   cg_primitive_get_n_vertices (prim_b));

  c_assert_cmpint (cg_primitive_get_mode (prim_a),
                   ==,
                   cg_primitive_get_mode (prim_b));

  c_assert (cg_primitive_get_indices (prim_a) ==
            cg_primitive_get_indices (prim_b));

  cg_object_unref (prim_a);
  cg_object_unref (prim_b);
  cg_object_unref (indices);

  for (i = 0; i < N_ATTRIBS; i++)
    cg_object_unref (attributes[i]);

  cg_object_unref (buffer);
}

void
test_primitive (void)
{
  TestState state;

  state.fb_width = cg_framebuffer_get_width (test_fb);
  state.fb_height = cg_framebuffer_get_height (test_fb);

  cg_framebuffer_orthographic (test_fb,
                                 0, 0,
                                 state.fb_width,
                                 state.fb_height,
                                 -1,
                                 100);

  test_paint (&state);
  test_copy (&state);

  if (test_verbose ())
    c_print ("OK\n");
}
