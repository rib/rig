/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2011,2014 Intel Corporation.
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
 *
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#include <rig-config.h>

#include <stdbool.h>

#include <clib.h>

#include <cglib/cglib.h>

#include "rig-text-pipeline-cache.h"

typedef struct _rig_text_pipeline_cache_entry_t rig_text_pipeline_cache_entry_t;

struct _rig_text_pipeline_cache_entry_t {
    /* This will take a reference or it can be NULL to represent the
       pipeline used to render colors */
    cg_texture_t *texture;

    /* This will only take a weak reference */
    cg_pipeline_t *pipeline;
};

static void
_rig_text_pipeline_cache_key_destroy(void *data)
{
    if (data)
        cg_object_unref(data);
}

static void
_rig_text_pipeline_cache_value_destroy(void *data)
{
    rig_text_pipeline_cache_entry_t *cache_entry = data;

    if (cache_entry->texture)
        cg_object_unref(cache_entry->texture);

    /* We don't need to unref the pipeline because it only takes a weak
       reference */

    c_slice_free(rig_text_pipeline_cache_entry_t, cache_entry);
}

rig_text_pipeline_cache_t *
rig_text_pipeline_cache_new(cg_device_t *dev,
                            bool use_mipmapping)
{
    rig_text_pipeline_cache_t *cache = c_new(rig_text_pipeline_cache_t, 1);

    cache->dev = dev;

    /* The key is the pipeline pointer. A reference is taken when the
       pipeline is used as a key so we should unref it again in the
       destroy function */
    cache->hash_table =
        c_hash_table_new_full(c_direct_hash,
                              c_direct_equal,
                              _rig_text_pipeline_cache_key_destroy,
                              _rig_text_pipeline_cache_value_destroy);

    cache->base_texture_rgba_pipeline = NULL;
    cache->base_texture_alpha_pipeline = NULL;

    cache->use_mipmapping = use_mipmapping;

    return cache;
}

static cg_pipeline_t *
get_base_texture_rgba_pipeline(rig_text_pipeline_cache_t *cache)
{
    if (cache->base_texture_rgba_pipeline == NULL) {
        cg_pipeline_t *pipeline;

        pipeline = cache->base_texture_rgba_pipeline =
                       cg_pipeline_new(cache->dev);

        cg_pipeline_set_layer_wrap_mode(
            pipeline, 0, CG_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);

        if (cache->use_mipmapping)
            cg_pipeline_set_layer_filters(
                pipeline,
                0,
                CG_PIPELINE_FILTER_LINEAR_MIPMAP_LINEAR,
                CG_PIPELINE_FILTER_LINEAR);
    }

    return cache->base_texture_rgba_pipeline;
}

static cg_pipeline_t *
get_base_texture_alpha_pipeline(rig_text_pipeline_cache_t *cache)
{
    if (cache->base_texture_alpha_pipeline == NULL) {
        cg_pipeline_t *pipeline;
        cg_snippet_t *snippet;

        pipeline = cg_pipeline_copy(get_base_texture_rgba_pipeline(cache));
        cache->base_texture_alpha_pipeline = pipeline;

        /* The default combine mode of materials is to modulate (A x B)
         * the texture RGBA channels with the RGBA channels of the
         * previous layer (which in our case is just the font color)
         *
         * Since the RGB for an alpha texture is defined as 0, this gives us:
         *
         *  result.rgb = color.rgb * 0
         *  result.a = color.a * texture.a
         *
         * What we want is premultiplied rgba values:
         *
         *  result.rgba = color.rgb * texture.a
         *  result.a = color.a * texture.a
         */

        snippet = cg_snippet_new(CG_SNIPPET_HOOK_LAYER_FRAGMENT, NULL, NULL);
        cg_snippet_set_replace(snippet, "frag *= cg_texel0.a;\n");

        cg_pipeline_add_layer_snippet(pipeline, 0, snippet);
        cg_object_unref(snippet);
    }

    return cache->base_texture_alpha_pipeline;
}

typedef struct {
    rig_text_pipeline_cache_t *cache;
    cg_texture_t *texture;
} PipelineDestroyNotifyData;

static void
pipeline_destroy_notify_cb(void *user_data)
{
    PipelineDestroyNotifyData *data = user_data;

    c_hash_table_remove(data->cache->hash_table, data->texture);
    c_slice_free(PipelineDestroyNotifyData, data);
}

cg_pipeline_t *
rig_text_pipeline_cache_get(rig_text_pipeline_cache_t *cache,
                            cg_texture_t *texture)
{
    rig_text_pipeline_cache_entry_t *entry;
    PipelineDestroyNotifyData *destroy_data;
    static cg_user_data_key_t pipeline_destroy_notify_key;

    /* Look for an existing entry */
    entry = c_hash_table_lookup(cache->hash_table, texture);

    if (entry)
        return cg_object_ref(entry->pipeline);

    /* No existing pipeline was found so let's create another */
    entry = c_slice_new(rig_text_pipeline_cache_entry_t);

    if (texture) {
        cg_pipeline_t *base;

        entry->texture = cg_object_ref(texture);

        if (cg_texture_get_components(entry->texture) ==
            CG_TEXTURE_COMPONENTS_A)
            base = get_base_texture_alpha_pipeline(cache);
        else
            base = get_base_texture_rgba_pipeline(cache);

        entry->pipeline = cg_pipeline_copy(base);

        cg_pipeline_set_layer_texture(
            entry->pipeline, 0 /* layer */, texture);
    } else {
        entry->texture = NULL;
        entry->pipeline = cg_pipeline_new(cache->dev);
    }

    /* Add a weak reference to the pipeline so we can remove it from the
       hash table when it is destroyed */
    destroy_data = c_slice_new(PipelineDestroyNotifyData);
    destroy_data->cache = cache;
    destroy_data->texture = texture;
    cg_object_set_user_data(CG_OBJECT(entry->pipeline),
                            &pipeline_destroy_notify_key,
                            destroy_data,
                            pipeline_destroy_notify_cb);

    c_hash_table_insert(cache->hash_table,
                        texture ? cg_object_ref(texture) : NULL, entry);

    /* This doesn't take a reference on the pipeline so that it will use
       the newly created reference */
    return entry->pipeline;
}

void
rig_text_pipeline_cache_free(rig_text_pipeline_cache_t *cache)
{
    if (cache->base_texture_rgba_pipeline)
        cg_object_unref(cache->base_texture_rgba_pipeline);
    if (cache->base_texture_alpha_pipeline)
        cg_object_unref(cache->base_texture_alpha_pipeline);

    c_hash_table_destroy(cache->hash_table);

    c_free(cache);
}
