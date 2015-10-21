#include <config.h>

#include <cglib/cglib.h>

#include "test-cg-fixtures.h"

/* Keep track of the number of textures that we've created and are
 * still alive */
static int destroyed_texture_count = 0;

#define N_TEXTURES 3

static void
free_texture_cb(void *user_data)
{
    destroyed_texture_count++;
}

static cg_texture_t *
create_texture(void)
{
    static const uint8_t data[] = { 0xff, 0xff, 0xff, 0xff };
    static cg_user_data_key_t texture_data_key;
    cg_texture_2d_t *tex_2d;

    tex_2d = cg_texture_2d_new_from_data(test_dev,
                                         1, 1, /* width / height */
                                         CG_PIXEL_FORMAT_RGBA_8888_PRE,
                                         4, /* rowstride */
                                         data,
                                         NULL);

    /* Set some user data on the texture so we can track when it has
     * been destroyed */
    cg_object_set_user_data(CG_OBJECT(tex_2d),
                            &texture_data_key,
                            C_INT_TO_POINTER(1),
                            free_texture_cb);

    return tex_2d;
}

void
test_pipeline_cache_unrefs_texture(void)
{
    cg_pipeline_t *pipeline = cg_pipeline_new(test_dev);
    cg_pipeline_t *simple_pipeline;
    cg_snippet_t *blue_snippet;
    int i;

    /* Create a pipeline with three texture layers. That way we can be
     * pretty sure the pipeline will cause a unique shader to be
     * generated in the cache */
    for(i = 0; i < N_TEXTURES; i++) {
        cg_texture_t *tex = create_texture();
        cg_pipeline_set_layer_texture(pipeline, i, tex);
        cg_object_unref(tex);
    }

    /* Draw something with the pipeline to ensure it gets into the
     * pipeline cache */
    cg_framebuffer_draw_rectangle(test_fb,
                                  pipeline,
                                  0, 0, 10, 10);
    cg_framebuffer_finish(test_fb);

    /* Draw something else so that it is no longer the current flushed
     * pipeline, and the units have a different texture bound */
    simple_pipeline = cg_pipeline_new(test_dev);
    blue_snippet = cg_snippet_new(CG_SNIPPET_HOOK_LAYER_FRAGMENT,
                                  NULL, "frag = vec4(0.0, 0.0, 1.0, 1.0);");

    for(i = 0; i < N_TEXTURES; i++)
        cg_pipeline_add_layer_snippet(simple_pipeline, i, blue_snippet);

    cg_object_unref(blue_snippet);

    cg_framebuffer_draw_rectangle(test_fb, simple_pipeline, 0, 0, 10, 10);
    cg_framebuffer_finish(test_fb);
    cg_object_unref(simple_pipeline);

    c_assert_cmpint(destroyed_texture_count, ==, 0);

    /* Destroy the pipeline. This should immediately cause the textures
     * to be freed */
    cg_object_unref(pipeline);

    c_assert_cmpint(destroyed_texture_count, ==, N_TEXTURES);

    if(test_verbose())
        c_print("OK\n");
}
