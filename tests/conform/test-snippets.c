#include <config.h>

#include <cglib/cglib.h>

#include <string.h>

#include "test-cg-fixtures.h"

typedef struct _TestState {
    int fb_width, fb_height;
} TestState;

typedef void(* snippet_test_func_t)(TestState *state);

static cg_pipeline_t *
create_texture_pipeline(TestState *state)
{
    cg_pipeline_t *pipeline;
    cg_texture_t *tex;
    static const uint8_t tex_data[] = {
        0xff, 0x00, 0x00, 0xff, /* red */  0x00, 0xff, 0x00, 0xff, /* green */
        0x00, 0x00, 0xff, 0xff, /* blue */ 0xff, 0xff, 0x00, 0xff, /* yellow */
    };

    tex = test_cg_texture_new_from_data(test_dev,
                                        2, 2, /* width/height */
                                        TEST_CG_TEXTURE_NO_ATLAS,
                                        CG_PIXEL_FORMAT_RGBA_8888_PRE,
                                        8, /* rowstride */
                                        tex_data);

    pipeline = cg_pipeline_new(test_dev);

    cg_pipeline_set_layer_texture(pipeline, 0, tex);

    cg_pipeline_set_layer_filters(pipeline, 0,
                                  CG_PIPELINE_FILTER_NEAREST,
                                  CG_PIPELINE_FILTER_NEAREST);

    cg_object_unref(tex);

    return pipeline;
}

static void
simple_fragment_snippet(TestState *state)
{
    cg_pipeline_t *pipeline;
    cg_snippet_t *snippet;

    /* Simple fragment snippet */
    pipeline = cg_pipeline_new(test_dev);

    cg_pipeline_set_color4ub(pipeline, 255, 0, 0, 255);

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT,
                             NULL, /* declarations */
                             "cg_color_out.g += 1.0;");
    cg_pipeline_add_snippet(pipeline, snippet);
    cg_object_unref(snippet);

    cg_framebuffer_draw_rectangle(test_fb, pipeline, 0, 0, 10, 10);

    cg_object_unref(pipeline);

    test_cg_check_pixel(test_fb, 5, 5, 0xffff00ff);
}

static void
simple_vertex_snippet(TestState *state)
{
    cg_pipeline_t *pipeline;
    cg_snippet_t *snippet;

    /* Simple vertex snippet */
    pipeline = cg_pipeline_new(test_dev);

    cg_pipeline_set_color4ub(pipeline, 255, 0, 0, 255);

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_VERTEX,
                             NULL,
                             "cg_color_out.b += 1.0;");
    cg_pipeline_add_snippet(pipeline, snippet);
    cg_object_unref(snippet);

    cg_framebuffer_draw_rectangle(test_fb, pipeline, 10, 0, 20, 10);

    cg_object_unref(pipeline);

    test_cg_check_pixel(test_fb, 15, 5, 0xff00ffff);
}

static void
shared_uniform(TestState *state)
{
    cg_pipeline_t *pipeline;
    cg_snippet_t *snippet;
    int location;

    /* Snippets sharing a uniform across the vertex and fragment
       hooks */
    pipeline = cg_pipeline_new(test_dev);

    location = cg_pipeline_get_uniform_location(pipeline, "a_value");
    cg_pipeline_set_uniform_1f(pipeline, location, 0.25f);

    cg_pipeline_set_color4ub(pipeline, 255, 0, 0, 255);

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_VERTEX,
                             "uniform float a_value;",
                             "cg_color_out.b += a_value;");
    cg_pipeline_add_snippet(pipeline, snippet);
    cg_object_unref(snippet);
    snippet = cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT,
                             "uniform float a_value;",
                             "cg_color_out.b += a_value;");
    cg_pipeline_add_snippet(pipeline, snippet);
    cg_object_unref(snippet);

    cg_framebuffer_draw_rectangle(test_fb,
                                  pipeline,
                                  20, 0, 30, 10);

    cg_object_unref(pipeline);

    test_cg_check_pixel(test_fb, 25, 5, 0xff0080ff);
}

static void
lots_snippets(TestState *state)
{
    cg_pipeline_t *pipeline;
    cg_snippet_t *snippet;
    int location;
    int i;

    /* Lots of snippets on one pipeline */
    pipeline = cg_pipeline_new(test_dev);

    cg_pipeline_set_color4ub(pipeline, 0, 0, 0, 255);

    for(i = 0; i < 3; i++) {
        char letter = 'x' + i;
        char *uniform_name = c_strdup_printf("%c_value", letter);
        char *declarations = c_strdup_printf("uniform float %s;\n",
                                             uniform_name);
        char *code = c_strdup_printf("cg_color_out.%c = %s;\n",
                                     letter,
                                     uniform_name);

        location = cg_pipeline_get_uniform_location(pipeline, uniform_name);
        cg_pipeline_set_uniform_1f(pipeline, location,(i + 1) * 0.1f);

        snippet = cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT,
                                 declarations,
                                 code);
        cg_pipeline_add_snippet(pipeline, snippet);
        cg_object_unref(snippet);

        c_free(code);
        c_free(uniform_name);
        c_free(declarations);
    }

    cg_framebuffer_draw_rectangle(test_fb, pipeline, 30, 0, 40, 10);

    cg_object_unref(pipeline);

    test_cg_check_pixel(test_fb, 35, 5, 0x19334cff);
}

static void
shared_variable_pre_post(TestState *state)
{
    cg_pipeline_t *pipeline;
    cg_snippet_t *snippet;

    /* Test that the pre string can declare variables used by the post
       string */
    pipeline = cg_pipeline_new(test_dev);

    cg_pipeline_set_color4ub(pipeline, 255, 255, 255, 255);

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT,
                             NULL, /* declarations */
                             "cg_color_out = redvec;");
    cg_snippet_set_pre(snippet, "vec4 redvec = vec4(1.0, 0.0, 0.0, 1.0);");
    cg_pipeline_add_snippet(pipeline, snippet);
    cg_object_unref(snippet);

    cg_framebuffer_draw_rectangle(test_fb, pipeline, 40, 0, 50, 10);

    cg_object_unref(pipeline);

    test_cg_check_pixel(test_fb, 45, 5, 0xff0000ff);
}

static void
test_pipeline_caching(TestState *state)
{
    cg_pipeline_t *pipeline;
    cg_snippet_t *snippet;

    /* Check that the pipeline caching works when unrelated pipelines
       share snippets state. It's too hard to actually assert this in
       the conformance test but at least it should be possible to see by
       setting CG_DEBUG=show-source to check whether this shader gets
       generated twice */
    snippet = cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT,
                             "/* This comment should only be seen ONCE\n"
                             "   when CG_DEBUG=show-source is true\n"
                             "   even though it is used in two different\n"
                             "   unrelated pipelines */",
                             "cg_color_out = vec4(0.0, 1.0, 0.0, 1.0);\n");

    pipeline = cg_pipeline_new(test_dev);
    cg_pipeline_add_snippet(pipeline, snippet);
    cg_framebuffer_draw_rectangle(test_fb, pipeline, 50, 0, 60, 10);
    cg_object_unref(pipeline);

    pipeline = cg_pipeline_new(test_dev);
    cg_pipeline_add_snippet(pipeline, snippet);
    cg_framebuffer_draw_rectangle(test_fb, pipeline, 60, 0, 70, 10);
    cg_object_unref(pipeline);

    cg_object_unref(snippet);

    test_cg_check_pixel(test_fb, 55, 5, 0x00ff00ff);
    test_cg_check_pixel(test_fb, 65, 5, 0x00ff00ff);
}

static void
test_replace_string(TestState *state)
{
    cg_pipeline_t *pipeline;
    cg_snippet_t *snippet;

    /* Check the replace string */
    snippet = cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT, NULL, NULL);
    cg_snippet_set_pre(snippet,
                       "cg_color_out = vec4(0.0, 0.5, 0.0, 1.0);");
    /* Remove the generated output. If the replace string isn't working
       then the code from the pre string would get overwritten with
       white */
    cg_snippet_set_replace(snippet, "/* do nothing */");
    cg_snippet_set_post(snippet,
                        "cg_color_out += vec4(0.5, 0.0, 0.0, 1.0);");

    pipeline = cg_pipeline_new(test_dev);
    cg_pipeline_add_snippet(pipeline, snippet);
    cg_framebuffer_draw_rectangle(test_fb, pipeline, 70, 0, 80, 10);
    cg_object_unref(pipeline);

    cg_object_unref(snippet);

    test_cg_check_pixel(test_fb, 75, 5, 0x808000ff);
}

static void
test_texture_lookup_hook(TestState *state)
{
    cg_pipeline_t *pipeline;
    cg_snippet_t *snippet;

    /* Check the texture lookup hook */
    snippet = cg_snippet_new(CG_SNIPPET_HOOK_TEXTURE_LOOKUP,
                             NULL,
                             "cg_texel.b += 1.0;");

    /* Flip the texture coordinates around the y axis so that it will
     * get the green texel.
     *
     * XXX: the - 0.1 to avoid sampling at the texture border since we aren't
     * sure there won't be some inprecision in flipping the coordinate and we
     * might sample the wrong texel with the default _REPEAT wrap mode.
     */
    cg_snippet_set_pre(snippet, "cg_tex_coord.x =(1.0 - cg_tex_coord.x) - 0.1;");

    pipeline = create_texture_pipeline(state);
    cg_pipeline_add_layer_snippet(pipeline, 0, snippet);
    cg_framebuffer_draw_textured_rectangle(test_fb,
                                           pipeline,
                                           80, 0, 90, 10,
                                           0, 0, 0, 0);
    cg_object_unref(pipeline);

    cg_object_unref(snippet);

    test_cg_check_pixel(test_fb, 85, 5, 0x00ffffff);
}

static void
test_multiple_samples(TestState *state)
{
    cg_pipeline_t *pipeline;
    cg_snippet_t *snippet;

    /* Check that we can use the passed in sampler in the texture lookup
       to sample multiple times */
    snippet = cg_snippet_new(CG_SNIPPET_HOOK_TEXTURE_LOOKUP,
                             NULL,
                             NULL);
    cg_snippet_set_replace(snippet,
                           "cg_texel = "
                           "texture2D(cg_sampler, vec2(0.25, 0.25)) + "
                           "texture2D(cg_sampler, vec2(0.75, 0.25));");

    pipeline = create_texture_pipeline(state);
    cg_pipeline_add_layer_snippet(pipeline, 0, snippet);
    cg_framebuffer_draw_rectangle(test_fb, pipeline, 0, 0, 10, 10);
    cg_object_unref(pipeline);

    cg_object_unref(snippet);

    test_cg_check_pixel(test_fb, 5, 5, 0xffff00ff);
}

static void
test_replace_lookup_hook(TestState *state)
{
    cg_pipeline_t *pipeline;
    cg_snippet_t *snippet;

    /* Check replacing the texture lookup hook */
    snippet = cg_snippet_new(CG_SNIPPET_HOOK_TEXTURE_LOOKUP, NULL, NULL);
    cg_snippet_set_replace(snippet, "cg_texel = vec4(0.0, 0.0, 1.0, 0.0);");

    pipeline = create_texture_pipeline(state);
    cg_pipeline_add_layer_snippet(pipeline, 0, snippet);
    cg_framebuffer_draw_textured_rectangle(test_fb,
                                           pipeline,
                                           90, 0, 100, 10,
                                           0, 0, 0, 0);
    cg_object_unref(pipeline);

    cg_object_unref(snippet);

    test_cg_check_pixel(test_fb, 95, 5, 0x0000ffff);
}

static void
test_replace_snippet(TestState *state)
{
    cg_pipeline_t *pipeline;
    cg_snippet_t *snippet;

    /* Test replacing a previous snippet */
    pipeline = create_texture_pipeline(state);

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT,
                             NULL,
                             "cg_color_out = vec4(0.5, 0.5, 0.5, 1.0);");
    cg_pipeline_add_snippet(pipeline, snippet);
    cg_object_unref(snippet);

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT, NULL, NULL);
    cg_snippet_set_pre(snippet, "cg_color_out = vec4(1.0, 1.0, 1.0, 1.0);");
    cg_snippet_set_replace(snippet,
                           "cg_color_out *= vec4(1.0, 0.0, 0.0, 1.0);");
    cg_pipeline_add_snippet(pipeline, snippet);
    cg_object_unref(snippet);

    cg_framebuffer_draw_textured_rectangle(test_fb,
                                           pipeline,
                                           100, 0, 110, 10,
                                           0, 0, 0, 0);
    cg_object_unref(pipeline);

    test_cg_check_pixel(test_fb, 105, 5, 0xff0000ff);
}

static void
test_replace_fragment_layer(TestState *state)
{
    cg_pipeline_t *pipeline;
    cg_snippet_t *snippet;

    /* Test replacing the fragment layer code */
    pipeline = create_texture_pipeline(state);

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_LAYER_FRAGMENT, NULL, NULL);
    cg_snippet_set_replace(snippet, "frag = vec4(0.0, 0.0, 1.0, 1.0);\n");
    cg_pipeline_add_layer_snippet(pipeline, 0, snippet);
    cg_object_unref(snippet);

    /* Add a second layer which references the texture of the first
     * layer. Even though the first layer is ignoring that layer's
     * texture sample we should still be able to reference it in a
     * another layer...
     */
    snippet = cg_snippet_new(CG_SNIPPET_HOOK_LAYER_FRAGMENT, NULL, NULL);
    cg_snippet_set_replace(snippet, "frag += cg_texel0;\n");
    cg_pipeline_add_layer_snippet(pipeline, 1, snippet);
    cg_object_unref(snippet);

    cg_framebuffer_draw_textured_rectangle(test_fb,
                                           pipeline,
                                           110, 0, 120, 10,
                                           0, 0, 0, 0);
    cg_object_unref(pipeline);

    test_cg_check_pixel(test_fb, 115, 5, 0xff00ffff);
}

static void
test_modify_fragment_layer(TestState *state)
{
    cg_pipeline_t *pipeline;
    cg_snippet_t *snippet;

    /* Test modifying the fragment layer code */
    pipeline = cg_pipeline_new(test_dev);

    cg_pipeline_set_uniform_1f(pipeline,
                               cg_pipeline_get_uniform_location(pipeline,
                                                                "a_value"),
                               0.5);

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_LAYER_FRAGMENT,
                             "uniform float a_value;",
                             "frag.g = a_value;");
    cg_pipeline_add_layer_snippet(pipeline, 0, snippet);
    cg_object_unref(snippet);

    cg_framebuffer_draw_textured_rectangle(test_fb,
                                           pipeline,
                                           120, 0, 130, 10,
                                           0, 0, 0, 0);
    cg_object_unref(pipeline);

    test_cg_check_pixel(test_fb, 125, 5, 0xff80ffff);
}

static void
test_modify_vertex_layer(TestState *state)
{
    cg_pipeline_t *pipeline;
    cg_snippet_t *snippet;

    /* Test modifying the vertex layer code */
    pipeline = create_texture_pipeline(state);

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_TEXTURE_COORD_TRANSFORM,
                             NULL,
                             "cg_tex_coord.x = 1.0;");
    cg_pipeline_add_layer_snippet(pipeline, 0, snippet);
    cg_object_unref(snippet);

    cg_framebuffer_draw_textured_rectangle(test_fb,
                                           pipeline,
                                           130, 0, 140, 10,
                                           0, 1, 0, 1);
    cg_object_unref(pipeline);

    test_cg_check_pixel(test_fb, 135, 5, 0xffff00ff);
}

static void
test_replace_vertex_layer(TestState *state)
{
    cg_pipeline_t *pipeline;
    cg_snippet_t *snippet;

    /* Test replacing the vertex layer code */
    pipeline = create_texture_pipeline(state);

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_TEXTURE_COORD_TRANSFORM,
                             NULL,
                             NULL);
    cg_snippet_set_replace(snippet, "cg_tex_coord.xy = vec2(1.0, 0.0);\n");
    cg_pipeline_add_layer_snippet(pipeline, 0, snippet);
    cg_object_unref(snippet);

    cg_framebuffer_draw_textured_rectangle(test_fb,
                                           pipeline,
                                           140, 0, 150, 10,
                                           1, 1, 1, 1);
    cg_object_unref(pipeline);

    test_cg_check_pixel(test_fb, 145, 5, 0x00ff00ff);
}

static void
test_vertex_transform_hook(TestState *state)
{
    cg_pipeline_t *pipeline;
    cg_snippet_t *snippet;
    c_matrix_t identity_matrix;
    c_matrix_t matrix;
    int location;

    /* Test the vertex transform hook */

    c_matrix_init_identity(&identity_matrix);

    pipeline = cg_pipeline_new(test_dev);

    cg_pipeline_set_color4ub(pipeline, 255, 0, 255, 255);

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_VERTEX_TRANSFORM,
                             "uniform mat4 pmat;",
                             NULL);
    cg_snippet_set_replace(snippet, "cg_position_out = "
                           "pmat * cg_position_in;");
    cg_pipeline_add_snippet(pipeline, snippet);
    cg_object_unref(snippet);

    /* Copy the current projection matrix to a uniform */
    cg_framebuffer_get_projection_matrix(test_fb, &matrix);
    location = cg_pipeline_get_uniform_location(pipeline, "pmat");
    cg_pipeline_set_uniform_matrix(pipeline,
                                   location,
                                   4, /* dimensions */
                                   1, /* count */
                                   false, /* don't transpose */
                                   c_matrix_get_array(&matrix));

    /* Replace the real projection matrix with the identity. This should
       mess up the drawing unless the snippet replacement is working */
    cg_framebuffer_set_projection_matrix(test_fb, &identity_matrix);

    cg_framebuffer_draw_rectangle(test_fb, pipeline, 150, 0, 160, 10);
    cg_object_unref(pipeline);

    /* Restore the projection matrix */
    cg_framebuffer_set_projection_matrix(test_fb, &matrix);

    test_cg_check_pixel(test_fb, 155, 5, 0xff00ffff);
}

static void
test_global_vertex_hook(TestState *state)
{
    cg_pipeline_t *pipeline;
    cg_snippet_t *snippet;

    pipeline = cg_pipeline_new(test_dev);

    /* Creates a function in the global declarations hook which is used
     * by a subsequent snippet. The subsequent snippets replace any
     * previous snippets but this shouldn't prevent the global
     * declarations from being generated */

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_VERTEX_GLOBALS,
                             /* declarations */
                             "float\n"
                             "multiply_by_two(float number)\n"
                             "{\n"
                             "  return number * 2.0;\n"
                             "}\n",
                             /* post */
                             "This string shouldn't be used so "
                             "we can safely put garbage in here.");
    cg_snippet_set_pre(snippet,
                       "This string shouldn't be used so "
                       "we can safely put garbage in here.");
    cg_snippet_set_replace(snippet,
                           "This string shouldn't be used so "
                           "we can safely put garbage in here.");
    cg_pipeline_add_snippet(pipeline, snippet);
    cg_object_unref(snippet);

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_VERTEX,
                             NULL, /* declarations */
                             NULL /* replace */);
    cg_snippet_set_replace(snippet,
                           "cg_color_out.r = multiply_by_two(0.5);\n"
                           "cg_color_out.gba = vec3(0.0, 0.0, 1.0);\n"
                           "cg_position_out = cg_position_in;\n");
    cg_pipeline_add_snippet(pipeline, snippet);
    cg_object_unref(snippet);

    cg_framebuffer_draw_rectangle(test_fb,
                                  pipeline,
                                  -1, 1,
                                  10.0f * 2.0f / state->fb_width - 1.0f,
                                  10.0f * 2.0f / state->fb_height - 1.0f);

    cg_object_unref(pipeline);

    test_cg_check_pixel(test_fb, 5, 5, 0xff0000ff);
}

static void
test_global_fragment_hook(TestState *state)
{
    cg_pipeline_t *pipeline;
    cg_snippet_t *snippet;

    pipeline = cg_pipeline_new(test_dev);

    /* Creates a function in the global declarations hook which is used
     * by a subsequent snippet. The subsequent snippets replace any
     * previous snippets but this shouldn't prevent the global
     * declarations from being generated */

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT_GLOBALS,
                             /* declarations */
                             "float\n"
                             "multiply_by_four(float number)\n"
                             "{\n"
                             "  return number * 4.0;\n"
                             "}\n",
                             /* post */
                             "This string shouldn't be used so "
                             "we can safely put garbage in here.");
    cg_snippet_set_pre(snippet,
                       "This string shouldn't be used so "
                       "we can safely put garbage in here.");
    cg_snippet_set_replace(snippet,
                           "This string shouldn't be used so "
                           "we can safely put garbage in here.");
    cg_pipeline_add_snippet(pipeline, snippet);
    cg_object_unref(snippet);

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT,
                             NULL, /* declarations */
                             NULL /* replace */);
    cg_snippet_set_replace(snippet,
                           "cg_color_out.r = multiply_by_four(0.25);\n"
                           "cg_color_out.gba = vec3(0.0, 0.0, 1.0);\n");
    cg_pipeline_add_snippet(pipeline, snippet);
    cg_object_unref(snippet);

    cg_framebuffer_draw_rectangle(test_fb,
                                  pipeline,
                                  0, 0, 10, 10);

    cg_object_unref(pipeline);

    test_cg_check_pixel(test_fb, 5, 5, 0xff0000ff);
}

static void
test_snippet_order(TestState *state)
{
    cg_pipeline_t *pipeline;
    cg_snippet_t *snippet;

    /* Verify that the snippets are executed in the right order. We'll
       replace the r component of the color in the pre sections of the
       snippets and the g component in the post. The pre sections should
       be executed in the reverse order they were added and the post
       sections in the same order as they were added. Therefore the r
       component should be taken from the the second snippet and the g
       component from the first */
    pipeline = cg_pipeline_new(test_dev);

    cg_pipeline_set_color4ub(pipeline, 0, 0, 0, 255);

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT,
                             NULL,
                             "cg_color_out.g = 0.5;\n");
    cg_snippet_set_pre(snippet, "cg_color_out.r = 0.5;\n");
    cg_snippet_set_replace(snippet, "cg_color_out.ba = vec2(0.0, 1.0);");
    cg_pipeline_add_snippet(pipeline, snippet);
    cg_object_unref(snippet);

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT,
                             NULL,
                             "cg_color_out.g = 1.0;\n");
    cg_snippet_set_pre(snippet, "cg_color_out.r = 1.0;\n");
    cg_pipeline_add_snippet(pipeline, snippet);
    cg_object_unref(snippet);

    cg_framebuffer_draw_rectangle(test_fb, pipeline, 160, 0, 170, 10);
    cg_object_unref(pipeline);

    test_cg_check_pixel(test_fb, 165, 5, 0x80ff00ff);
}

static void
test_naming_texture_units(TestState *state)
{
    cg_pipeline_t *pipeline;
    cg_snippet_t *snippet;
    cg_texture_t *tex1, *tex2;

    /* Test that we can sample from an arbitrary texture unit by naming
       its layer number */

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT,
                             NULL,
                             NULL);
    cg_snippet_set_replace(snippet,
                           "cg_color_out = "
                           "texture2D(cg_sampler100, vec2(0.0, 0.0)) + "
                           "texture2D(cg_sampler200, vec2(0.0, 0.0));");

    tex1 = test_cg_create_color_texture(test_dev, 0xff0000ff);
    tex2 = test_cg_create_color_texture(test_dev, 0x00ff00ff);

    pipeline = cg_pipeline_new(test_dev);

    cg_pipeline_set_layer_texture(pipeline, 100, tex1);
    cg_pipeline_set_layer_texture(pipeline, 200, tex2);

    cg_pipeline_add_snippet(pipeline, snippet);

    cg_framebuffer_draw_rectangle(test_fb, pipeline, 0, 0, 10, 10);

    cg_object_unref(pipeline);
    cg_object_unref(snippet);
    cg_object_unref(tex1);
    cg_object_unref(tex2);

    test_cg_check_pixel(test_fb, 5, 5, 0xffff00ff);
}

static void
test_snippet_properties(TestState *state)
{
    cg_snippet_t *snippet;

    /* Sanity check modifying the snippet */
    snippet = cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT, "foo", "bar");
    c_assert_cmpstr(cg_snippet_get_declarations(snippet), ==, "foo");
    c_assert_cmpstr(cg_snippet_get_post(snippet), ==, "bar");
    c_assert_cmpstr(cg_snippet_get_replace(snippet), ==, NULL);
    c_assert_cmpstr(cg_snippet_get_pre(snippet), ==, NULL);

    cg_snippet_set_declarations(snippet, "fu");
    c_assert_cmpstr(cg_snippet_get_declarations(snippet), ==, "fu");
    c_assert_cmpstr(cg_snippet_get_post(snippet), ==, "bar");
    c_assert_cmpstr(cg_snippet_get_replace(snippet), ==, NULL);
    c_assert_cmpstr(cg_snippet_get_pre(snippet), ==, NULL);

    cg_snippet_set_post(snippet, "ba");
    c_assert_cmpstr(cg_snippet_get_declarations(snippet), ==, "fu");
    c_assert_cmpstr(cg_snippet_get_post(snippet), ==, "ba");
    c_assert_cmpstr(cg_snippet_get_replace(snippet), ==, NULL);
    c_assert_cmpstr(cg_snippet_get_pre(snippet), ==, NULL);

    cg_snippet_set_pre(snippet, "fuba");
    c_assert_cmpstr(cg_snippet_get_declarations(snippet), ==, "fu");
    c_assert_cmpstr(cg_snippet_get_post(snippet), ==, "ba");
    c_assert_cmpstr(cg_snippet_get_replace(snippet), ==, NULL);
    c_assert_cmpstr(cg_snippet_get_pre(snippet), ==, "fuba");

    cg_snippet_set_replace(snippet, "baba");
    c_assert_cmpstr(cg_snippet_get_declarations(snippet), ==, "fu");
    c_assert_cmpstr(cg_snippet_get_post(snippet), ==, "ba");
    c_assert_cmpstr(cg_snippet_get_replace(snippet), ==, "baba");
    c_assert_cmpstr(cg_snippet_get_pre(snippet), ==, "fuba");

    c_assert_cmpint(cg_snippet_get_hook(snippet),
                    ==,
                    CG_SNIPPET_HOOK_FRAGMENT);
}

static snippet_test_func_t tests[] = {
    simple_fragment_snippet,
    simple_vertex_snippet,
    shared_uniform,
    lots_snippets,
    shared_variable_pre_post,
    test_pipeline_caching,
    test_replace_string,
    test_texture_lookup_hook,
    test_multiple_samples,
    test_replace_lookup_hook,
    test_replace_snippet,
    test_replace_fragment_layer,
    test_modify_fragment_layer,
    test_modify_vertex_layer,
    test_replace_vertex_layer,
    test_vertex_transform_hook,
    test_global_fragment_hook,
    test_global_vertex_hook,
    test_snippet_order,
    test_naming_texture_units,
    test_snippet_properties
};

static void
run_tests(TestState *state)
{
    int i;

    for(i = 0; i < C_N_ELEMENTS(tests); i++) {
        cg_framebuffer_clear4f(test_fb,
                               CG_BUFFER_BIT_COLOR,
                               0, 0, 0, 1);

        tests[i](state);
    }
}

void
test_snippets(void)
{
    TestState state;

    state.fb_width = cg_framebuffer_get_width(test_fb);
    state.fb_height = cg_framebuffer_get_height(test_fb);

    cg_framebuffer_orthographic(test_fb,
                                0, 0,
                                state.fb_width,
                                state.fb_height,
                                -1,
                                100);

    run_tests(&state);

    if(test_verbose())
        c_print("OK\n");
}
