#include <config.h>

#include <cglib/cglib.h>

#include <string.h>

#include "test-cg-fixtures.h"

typedef struct _TestState
{
  cg_pipeline_t *pipeline;
} TestState;

typedef struct
{
  int16_t x, y;
  float r, g, b, a;
} FloatVert;

typedef struct
{
  int16_t x, y;
  uint8_t r, g, b, a;
} ByteVert;

typedef struct
{
  int16_t x, y;
  int16_t r, g, b, a;
} ShortVert;

static void
test_float_verts (TestState *state, int offset_x, int offset_y)
{
  cg_attribute_t *attributes[2];
  cg_attribute_buffer_t *buffer;
  cg_primitive_t *primitive;

  static const FloatVert float_verts[] =
    {
      { 0, 10, /**/ 1, 0, 0, 1 },
      { 10, 10, /**/ 1, 0, 0, 1 },
      { 5, 0, /**/ 1, 0, 0, 1 },

      { 10, 10, /**/ 0, 1, 0, 1 },
      { 20, 10, /**/ 0, 1, 0, 1 },
      { 15, 0, /**/ 0, 1, 0, 1 }
    };

  buffer = cg_attribute_buffer_new (test_dev,
                                      sizeof (float_verts), float_verts);
  attributes[0] = cg_attribute_new (buffer,
                                      "cg_position_in",
                                      sizeof (FloatVert),
                                      C_STRUCT_OFFSET (FloatVert, x),
                                      2, /* n_components */
                                      CG_ATTRIBUTE_TYPE_SHORT);
  attributes[1] = cg_attribute_new (buffer,
                                      "color",
                                      sizeof (FloatVert),
                                      C_STRUCT_OFFSET (FloatVert, r),
                                      4, /* n_components */
                                      CG_ATTRIBUTE_TYPE_FLOAT);

  cg_framebuffer_push_matrix (test_fb);
  cg_framebuffer_translate (test_fb, offset_x, offset_y, 0.0f);

  primitive = cg_primitive_new_with_attributes (CG_VERTICES_MODE_TRIANGLES,
                                                  6, /* n_vertices */
                                                  attributes,
                                                  2); /* n_attributes */
  cg_primitive_draw (primitive, test_fb, state->pipeline);
  cg_object_unref (primitive);

  cg_framebuffer_pop_matrix (test_fb);

  cg_object_unref (attributes[1]);
  cg_object_unref (attributes[0]);
  cg_object_unref (buffer);

  test_cg_check_pixel (test_fb, offset_x + 5, offset_y + 5, 0xff0000ff);
  test_cg_check_pixel (test_fb, offset_x + 15, offset_y + 5, 0x00ff00ff);
}

static void
test_byte_verts (TestState *state, int offset_x, int offset_y)
{
  cg_attribute_t *attributes[2];
  cg_attribute_buffer_t *buffer, *unnorm_buffer;
  cg_primitive_t *primitive;

  static const ByteVert norm_verts[] =
    {
      { 0, 10, /**/ 255, 0, 0, 255 },
      { 10, 10, /**/ 255, 0, 0, 255 },
      { 5, 0, /**/ 255, 0, 0, 255 },

      { 10, 10, /**/ 0, 255, 0, 255 },
      { 20, 10, /**/ 0, 255, 0, 255 },
      { 15, 0, /**/ 0, 255, 0, 255 }
    };

  static const ByteVert unnorm_verts[] =
    {
      { 0, 0, /**/ 0, 0, 1, 1 },
      { 0, 0, /**/ 0, 0, 1, 1 },
      { 0, 0, /**/ 0, 0, 1, 1 },
    };

  buffer = cg_attribute_buffer_new (test_dev,
                                      sizeof (norm_verts), norm_verts);
  attributes[0] = cg_attribute_new (buffer,
                                      "cg_position_in",
                                      sizeof (ByteVert),
                                      C_STRUCT_OFFSET (ByteVert, x),
                                      2, /* n_components */
                                      CG_ATTRIBUTE_TYPE_SHORT);
  attributes[1] = cg_attribute_new (buffer,
                                      "color",
                                      sizeof (ByteVert),
                                      C_STRUCT_OFFSET (ByteVert, r),
                                      4, /* n_components */
                                      CG_ATTRIBUTE_TYPE_UNSIGNED_BYTE);
  cg_attribute_set_normalized (attributes[1], true);

  cg_framebuffer_push_matrix (test_fb);
  cg_framebuffer_translate (test_fb, offset_x, offset_y, 0.0f);

  primitive = cg_primitive_new_with_attributes (CG_VERTICES_MODE_TRIANGLES,
                                                  6, /* n_vertices */
                                                  attributes,
                                                  2); /* n_attributes */
  cg_primitive_draw (primitive, test_fb, state->pipeline);
  cg_object_unref (primitive);

  cg_object_unref (attributes[1]);

  /* Test again with unnormalized attributes */
  unnorm_buffer = cg_attribute_buffer_new (test_dev,
                                             sizeof (unnorm_verts),
                                             unnorm_verts);
  attributes[1] = cg_attribute_new (unnorm_buffer,
                                      "color",
                                      sizeof (ByteVert),
                                      C_STRUCT_OFFSET (ByteVert, r),
                                      4, /* n_components */
                                      CG_ATTRIBUTE_TYPE_BYTE);

  cg_framebuffer_translate (test_fb, 20, 0, 0);

  primitive = cg_primitive_new_with_attributes (CG_VERTICES_MODE_TRIANGLES,
                                                  3, /* n_vertices */
                                                  attributes,
                                                  2); /* n_attributes */
  cg_primitive_draw (primitive, test_fb, state->pipeline);
  cg_object_unref (primitive);

  cg_framebuffer_pop_matrix (test_fb);

  cg_object_unref (attributes[0]);
  cg_object_unref (attributes[1]);
  cg_object_unref (buffer);
  cg_object_unref (unnorm_buffer);

  test_cg_check_pixel (test_fb, offset_x + 5, offset_y + 5, 0xff0000ff);
  test_cg_check_pixel (test_fb, offset_x + 15, offset_y + 5, 0x00ff00ff);
  test_cg_check_pixel (test_fb, offset_x + 25, offset_y + 5, 0x0000ffff);
}

static void
test_short_verts (TestState *state, int offset_x, int offset_y)
{
  cg_attribute_t *attributes[2];
  cg_attribute_buffer_t *buffer;
  cg_pipeline_t *pipeline, *pipeline2;
  cg_snippet_t *snippet;
  cg_primitive_t *primitive;

  static const ShortVert short_verts[] =
    {
      { -10, -10, /**/ 0xffff, 0, 0, 0xffff },
      { -1, -10,  /**/ 0xffff, 0, 0, 0xffff },
      { -5, -1, /**/ 0xffff, 0, 0, 0xffff }
    };


  pipeline = cg_pipeline_copy (state->pipeline);

  cg_pipeline_set_color4ub (pipeline, 255, 0, 0, 255);

  buffer = cg_attribute_buffer_new (test_dev,
                                      sizeof (short_verts), short_verts);
  attributes[0] = cg_attribute_new (buffer,
                                      "cg_position_in",
                                      sizeof (ShortVert),
                                      C_STRUCT_OFFSET (ShortVert, x),
                                      2, /* n_components */
                                      CG_ATTRIBUTE_TYPE_SHORT);
  attributes[1] = cg_attribute_new (buffer,
                                      "color",
                                      sizeof (ShortVert),
                                      C_STRUCT_OFFSET (ShortVert, r),
                                      4, /* n_components */
                                      CG_ATTRIBUTE_TYPE_UNSIGNED_SHORT);
  cg_attribute_set_normalized (attributes[1], true);

  cg_framebuffer_push_matrix (test_fb);
  cg_framebuffer_translate (test_fb,
                              offset_x + 10.0f,
                              offset_y + 10.0f,
                              0.0f);

  primitive = cg_primitive_new_with_attributes (CG_VERTICES_MODE_TRIANGLES,
                                                  3, /* n_vertices */
                                                  attributes,
                                                  2); /* n_attributes */
  cg_primitive_draw (primitive, test_fb, pipeline);
  cg_object_unref (primitive);

  cg_framebuffer_pop_matrix (test_fb);

  cg_object_unref (attributes[0]);

  /* Test again treating the attribute as unsigned */
  attributes[0] = cg_attribute_new (buffer,
                                      "cg_position_in",
                                      sizeof (ShortVert),
                                      C_STRUCT_OFFSET (ShortVert, x),
                                      2, /* n_components */
                                      CG_ATTRIBUTE_TYPE_UNSIGNED_SHORT);

  /* XXX: this is a hack to force the pipeline to use the glsl backend
   * because we know it's not possible to test short vertex position
   * components with the legacy GL backend since which might otherwise
   * be used internally... */
  pipeline2 = cg_pipeline_new (test_dev);
  snippet = cg_snippet_new (CG_SNIPPET_HOOK_VERTEX,
                              "in vec4 color;",
                              "cg_color_out = vec4 (0.0, 1.0, 0.0, 1.0);");
  cg_pipeline_add_snippet (pipeline2, snippet);

  cg_framebuffer_push_matrix (test_fb);
  cg_framebuffer_translate (test_fb,
                              offset_x + 10.0f - 65525.0f,
                              offset_y - 65525,
                              0.0f);

  primitive = cg_primitive_new_with_attributes (CG_VERTICES_MODE_TRIANGLES,
                                                  3, /* n_vertices */
                                                  attributes,
                                                  1); /* n_attributes */
  cg_primitive_draw (primitive, test_fb, pipeline2);
  cg_object_unref (primitive);

  cg_framebuffer_pop_matrix (test_fb);

  cg_object_unref (attributes[0]);

  cg_object_unref (pipeline2);
  cg_object_unref (pipeline);
  cg_object_unref (buffer);

  test_cg_check_pixel (test_fb, offset_x + 5, offset_y + 5, 0xff0000ff);
  test_cg_check_pixel (test_fb, offset_x + 15, offset_y + 5, 0x00ff00ff);
}

static void
paint (TestState *state)
{
  cg_framebuffer_clear4f (test_fb, CG_BUFFER_BIT_COLOR, 0, 0, 0, 1);

  test_float_verts (state, 0, 0);
  test_byte_verts (state, 0, 10);
  test_short_verts (state, 0, 20);
}

void
test_custom_attributes (void)
{
  cg_snippet_t *snippet;
  TestState state;

  cg_framebuffer_orthographic (test_fb,
                                 0, 0,
                                 cg_framebuffer_get_width (test_fb),
                                 cg_framebuffer_get_height (test_fb),
                                 -1,
                                 100);

  state.pipeline = cg_pipeline_new (test_dev);
  snippet = cg_snippet_new (CG_SNIPPET_HOOK_VERTEX,
                              "in vec4 color;",
                              "cg_color_out = color;");
  cg_pipeline_add_snippet (state.pipeline, snippet);

  paint (&state);

  cg_object_unref (state.pipeline);
  cg_object_unref (snippet);

  if (test_verbose ())
    c_print ("OK\n");
}
