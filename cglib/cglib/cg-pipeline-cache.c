/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011-2015 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <cglib-config.h>

#include <test-fixtures/test-cg-fixtures.h>

#include "cg-device-private.h"
#include "cg-pipeline-private.h"
#include "cg-pipeline-cache.h"
#include "cg-pipeline-hash-table.h"

struct _cg_pipeline_cache_t {
    cg_device_t *dev;

    cg_pipeline_hash_table_t fragment_hash;
    cg_pipeline_hash_table_t vertex_hash;
    cg_pipeline_hash_table_t combined_hash;
};

cg_pipeline_cache_t *
_cg_pipeline_cache_new(cg_device_t *dev)
{
    cg_pipeline_cache_t *cache = c_new(cg_pipeline_cache_t, 1);
    unsigned long vertex_state;
    unsigned long layer_vertex_state;
    unsigned int fragment_state;
    unsigned int layer_fragment_state;

    cache->dev = dev;

    vertex_state = _cg_pipeline_get_state_for_vertex_codegen(dev);
    layer_vertex_state = CG_PIPELINE_LAYER_STATE_AFFECTS_VERTEX_CODEGEN;
    fragment_state = _cg_pipeline_get_state_for_fragment_codegen(dev);
    layer_fragment_state =
        _cg_pipeline_get_layer_state_for_fragment_codegen(dev);

    _cg_pipeline_hash_table_init(&cache->vertex_hash,
                                 dev,
                                 vertex_state,
                                 layer_vertex_state,
                                 "vertex shaders");
    _cg_pipeline_hash_table_init(&cache->fragment_hash,
                                 dev,
                                 fragment_state,
                                 layer_fragment_state,
                                 "fragment shaders");
    _cg_pipeline_hash_table_init(&cache->combined_hash,
                                 dev,
                                 vertex_state | fragment_state,
                                 layer_vertex_state | layer_fragment_state,
                                 "programs");

    return cache;
}

void
_cg_pipeline_cache_free(cg_pipeline_cache_t *cache)
{
    _cg_pipeline_hash_table_destroy(&cache->fragment_hash);
    _cg_pipeline_hash_table_destroy(&cache->vertex_hash);
    _cg_pipeline_hash_table_destroy(&cache->combined_hash);
    c_free(cache);
}

cg_pipeline_cache_entry_t *
_cg_pipeline_cache_get_fragment_template(cg_pipeline_cache_t *cache,
                                         cg_pipeline_t *key_pipeline)
{
    return _cg_pipeline_hash_table_get(&cache->fragment_hash, key_pipeline);
}

cg_pipeline_cache_entry_t *
_cg_pipeline_cache_get_vertex_template(cg_pipeline_cache_t *cache,
                                       cg_pipeline_t *key_pipeline)
{
    return _cg_pipeline_hash_table_get(&cache->vertex_hash, key_pipeline);
}

cg_pipeline_cache_entry_t *
_cg_pipeline_cache_get_combined_template(cg_pipeline_cache_t *cache,
                                         cg_pipeline_t *key_pipeline)
{
    return _cg_pipeline_hash_table_get(&cache->combined_hash, key_pipeline);
}

#ifdef ENABLE_UNIT_TESTS

static void
create_pipelines(cg_pipeline_t **pipelines, int n_pipelines)
{
    int i;

    for (i = 0; i < n_pipelines; i++) {
        char *source = c_strdup_printf("  cg_color_out = "
                                       "vec4 (%f, 0.0, 0.0, 1.0);\n",
                                       i / 255.0f);
        cg_snippet_t *snippet = cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT,
                                               NULL, /* declarations */
                                               source);

        c_free(source);

        pipelines[i] = cg_pipeline_new(test_dev);
        cg_pipeline_add_snippet(pipelines[i], snippet);
        cg_object_unref(snippet);
    }

    /* Test that drawing with them works. This should create the entries
     * in the cache */
    for (i = 0; i < n_pipelines; i++) {
        cg_framebuffer_draw_rectangle(test_fb, pipelines[i], i, 0, i + 1, 1);
        test_cg_check_pixel_rgb(test_fb, i, 0, i, 0, 0);
    }
}

TEST(check_pipeline_pruning)
{
    cg_pipeline_t *pipelines[18];
    int fb_width, fb_height;
    cg_pipeline_hash_table_t *fragment_hash;
    cg_pipeline_hash_table_t *combined_hash;
    int i;

    test_cg_init();

    fragment_hash = &test_dev->pipeline_cache->fragment_hash;
    combined_hash = &test_dev->pipeline_cache->combined_hash;

    fb_width = cg_framebuffer_get_width(test_fb);
    fb_height = cg_framebuffer_get_height(test_fb);

    cg_framebuffer_orthographic(test_fb, 0, 0, fb_width, fb_height, -1, 100);

    /* Create 18 unique pipelines. This should end up being more than
     * the initial expected minimum size so it will trigger the garbage
     * collection. However all of the pipelines will be in use so they
     * won't be collected */
    create_pipelines(pipelines, 18);

    /* These pipelines should all have unique entries in the cache. We
     * should have run the garbage collection once and at that point the
     * expected minimum size would have been 17 */
    c_assert_cmpint(c_hash_table_size(fragment_hash->table), ==, 18);
    c_assert_cmpint(c_hash_table_size(combined_hash->table), ==, 18);
    c_assert_cmpint(fragment_hash->expected_min_size, ==, 17);
    c_assert_cmpint(combined_hash->expected_min_size, ==, 17);

    /* Destroy the original pipelines and create some new ones. This
     * should run the garbage collector again but this time the
     * pipelines won't be in use so it should free some of them */
    for (i = 0; i < 18; i++)
        cg_object_unref(pipelines[i]);

    create_pipelines(pipelines, 18);

    /* The garbage collection should have freed half of the original 18
     * pipelines which means there should now be 18*1.5 = 27 */
    c_assert_cmpint(c_hash_table_size(fragment_hash->table), ==, 27);
    c_assert_cmpint(c_hash_table_size(combined_hash->table), ==, 27);
    /* The 35th pipeline would have caused the garbage collection. At
     * that point there would be 35-18=17 used unique pipelines. */
    c_assert_cmpint(fragment_hash->expected_min_size, ==, 17);
    c_assert_cmpint(combined_hash->expected_min_size, ==, 17);

    for (i = 0; i < 18; i++)
        cg_object_unref(pipelines[i]);

    test_cg_fini();
}

#endif /* ENABLE_UNIT_TESTS */
