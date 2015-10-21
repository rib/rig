#include <config.h>

#include <cglib/cglib.h>
#include <string.h>

#include "test-cg-fixtures.h"

typedef struct _test_state_t {
    int fb_width;
    int fb_height;
} test_state_t;

static void
test_sparse_layer_combine(test_state_t *state)
{
    cg_pipeline_t *pipeline;
    cg_texture_t *tex1, *tex2;
    cg_snippet_t *snippet;

    cg_framebuffer_clear4f(test_fb, CG_BUFFER_BIT_COLOR, 0, 0, 0, 1);

    /* This tests that the TEXTURE_* numbers used in the layer combine
       string refer to the layer number rather than the unit numbers by
       creating a pipeline with very large layer numbers. This should
       end up being mapped to much smaller unit numbers */

    tex1 = test_cg_create_color_texture(test_dev, 0xff0000ff);
    tex2 = test_cg_create_color_texture(test_dev, 0x00ff00ff);

    pipeline = cg_pipeline_new(test_dev);

    cg_pipeline_set_layer_texture(pipeline, 50, tex1);
    cg_pipeline_set_layer_texture(pipeline, 100, tex2);

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_LAYER_FRAGMENT, NULL, NULL);
    cg_snippet_set_replace(snippet, "frag = cg_texel50 + cg_texel100;\n");
    cg_pipeline_add_layer_snippet(pipeline, 200, snippet);
    cg_object_unref(snippet);

    cg_framebuffer_draw_rectangle(test_fb, pipeline, -1, -1, 1, 1);

    test_cg_check_pixel(test_fb, 2, 2, 0xffff00ff);

    cg_object_unref(pipeline);
    cg_object_unref(tex1);
    cg_object_unref(tex2);
}

void
test_sparse_pipeline(void)
{
    test_state_t state;

    state.fb_width = cg_framebuffer_get_width(test_fb);
    state.fb_height = cg_framebuffer_get_height(test_fb);

    test_sparse_layer_combine(&state);

    /* FIXME: This should have a lot more tests, for example testing
       whether using an attribute with sparse texture coordinates will
       work */

    if(test_verbose())
        c_print("OK\n");
}

