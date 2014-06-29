/*
 * Cogl
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008 OpenedHand
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
 */

#include <config.h>

#include <glib.h>

#include "cogl-pango-glyph-cache.h"
#include "cogl-pango-private.h"
#include "cogl/cogl-atlas-set.h"
#include "cogl/cogl-atlas-texture-private.h"
#include "cogl/cogl-context-private.h"

typedef struct _cg_pango_glyph_cache_key_t cg_pango_glyph_cache_key_t;

typedef struct _atlas_closure_state_t {
    cg_list_t list_node;
    cg_atlas_t *atlas;
    cg_atlas_reorganize_closure_t *reorganize_closure;
    cg_atlas_allocate_closure_t *allocate_closure;
} atlas_closure_state_t;

struct _cg_pango_glyph_cache_t {
    cg_context_t *ctx;

    /* Hash table to quickly check whether a particular glyph in a
       particular font is already cached */
    GHashTable *hash_table;

    /* Set of cg_atlas_tes */
    cg_atlas_set_t *atlas_set;

    cg_list_t atlas_closures;

    /* List of callbacks to invoke when an atlas is reorganized */
    GHookList reorganize_callbacks;

    /* True if some of the glyphs are dirty. This is used as an
       optimization in _cg_pango_glyph_cache_set_dirty_glyphs to avoid
       iterating the hash table if we know none of them are dirty */
    bool has_dirty_glyphs;

    /* Whether mipmapping is being used for this cache. This only
       affects whether we decide to put the glyph in the global atlas */
    bool use_mipmapping;
};

struct _cg_pango_glyph_cache_key_t {
    PangoFont *font;
    PangoGlyph glyph;
};

static void
cg_pango_glyph_cache_value_free(cg_pango_glyph_cache_value_t *value)
{
    if (value->texture) {
        cg_object_unref(value->texture);
        cg_object_unref(value->atlas);
    }
    g_slice_free(cg_pango_glyph_cache_value_t, value);
}

static void
cg_pango_glyph_cache_key_free(cg_pango_glyph_cache_key_t *key)
{
    g_object_unref(key->font);
    g_slice_free(cg_pango_glyph_cache_key_t, key);
}

static guint
cg_pango_glyph_cache_hash_func(const void *key)
{
    const cg_pango_glyph_cache_key_t *cache_key =
        (const cg_pango_glyph_cache_key_t *)key;

    /* Generate a number affected by both the font and the glyph
       number. We can safely directly compare the pointers because the
       key holds a reference to the font so it is not possible that a
       different font will have the same memory address */
    return GPOINTER_TO_UINT(cache_key->font) ^ cache_key->glyph;
}

static gboolean
cg_pango_glyph_cache_equal_func(const void *a, const void *b)
{
    const cg_pango_glyph_cache_key_t *key_a =
        (const cg_pango_glyph_cache_key_t *)a;
    const cg_pango_glyph_cache_key_t *key_b =
        (const cg_pango_glyph_cache_key_t *)b;

    /* We can safely directly compare the pointers for the fonts because
       the key holds a reference to the font so it is not possible that
       a different font will have the same memory address */
    return key_a->font == key_b->font && key_a->glyph == key_b->glyph;
}

static void
atlas_reorganize_cb(cg_atlas_t *atlas, void *user_data)
{
    cg_pango_glyph_cache_t *cache = user_data;

    g_hook_list_invoke(&cache->reorganize_callbacks, false);
}

static void
allocate_glyph_cb(cg_atlas_t *atlas,
                  cg_texture_t *texture,
                  const cg_atlas_allocation_t *allocation,
                  void *allocation_data,
                  void *user_data)
{
    cg_pango_glyph_cache_value_t *value = allocation_data;
    float tex_width, tex_height;

    if (value->texture) {
        cg_object_unref(value->texture);
        cg_object_unref(value->atlas);
    }
    value->atlas = cg_object_ref(atlas);
    value->texture = cg_object_ref(texture);

    tex_width = cg_texture_get_width(texture);
    tex_height = cg_texture_get_height(texture);

    value->tx1 = allocation->x / tex_width;
    value->ty1 = allocation->y / tex_height;
    value->tx2 = (allocation->x + value->draw_width) / tex_width;
    value->ty2 = (allocation->y + value->draw_height) / tex_height;

    value->tx_pixel = allocation->x;
    value->ty_pixel = allocation->y;

    /* The glyph has changed position so it will need to be redrawn */
    value->dirty = true;
}

static void
atlas_callback(cg_atlas_set_t *set,
               cg_atlas_t *atlas,
               cg_atlas_set_event_t event,
               void *user_data)
{
    cg_pango_glyph_cache_t *cache = user_data;
    atlas_closure_state_t *state;

    switch (event) {
    case CG_ATLAS_SET_EVENT_ADDED:
        state = g_slice_new(atlas_closure_state_t);
        state->atlas = atlas;
        state->reorganize_closure = cg_atlas_add_post_reorganize_callback(
            atlas, atlas_reorganize_cb, cache, NULL); /* destroy */
        state->allocate_closure = cg_atlas_add_allocate_callback(
            atlas, allocate_glyph_cb, cache, NULL); /* destroy */

        _cg_list_insert(cache->atlas_closures.prev, &state->list_node);
        break;
    case CG_ATLAS_SET_EVENT_REMOVED:
        break;
    }
}

static void
add_global_atlas_cb(cg_atlas_t *atlas, void *user_data)
{
    cg_pango_glyph_cache_t *cache = user_data;

    atlas_callback(
        _cg_get_atlas_set(cache->ctx), atlas, CG_ATLAS_SET_EVENT_ADDED, cache);
}

cg_pango_glyph_cache_t *
cg_pango_glyph_cache_new(cg_context_t *ctx,
                         bool use_mipmapping)
{
    cg_pango_glyph_cache_t *cache;

    cache = g_malloc(sizeof(cg_pango_glyph_cache_t));

    /* Note: as a rule we don't take references to a cg_context_t
     * internally since */
    cache->ctx = ctx;

    cache->hash_table = g_hash_table_new_full(
        cg_pango_glyph_cache_hash_func,
        cg_pango_glyph_cache_equal_func,
        (c_destroy_func_t)cg_pango_glyph_cache_key_free,
        (c_destroy_func_t)cg_pango_glyph_cache_value_free);

    _cg_list_init(&cache->atlas_closures);

    cache->atlas_set = cg_atlas_set_new(ctx);

    cg_atlas_set_set_components(cache->atlas_set, CG_TEXTURE_COMPONENTS_A);

    cg_atlas_set_set_migration_enabled(cache->atlas_set, false);
    cg_atlas_set_set_clear_enabled(cache->atlas_set, true);

    /* We want to be notified when new atlases are added to our local
     * atlas set so they can be monitored for being re-arranged... */
    cg_atlas_set_add_atlas_callback(
        cache->atlas_set, atlas_callback, cache, NULL); /* destroy */

    /* We want to be notified when new atlases are added to the global
     * atlas set so they can be monitored for being re-arranged... */
    cg_atlas_set_add_atlas_callback(
        _cg_get_atlas_set(ctx), atlas_callback, cache, NULL); /* destroy */
    /* The global atlas set may already have atlases that we will
     * want to monitor... */
    cg_atlas_set_foreach(_cg_get_atlas_set(ctx), add_global_atlas_cb, cache);

    g_hook_list_init(&cache->reorganize_callbacks, sizeof(GHook));

    cache->has_dirty_glyphs = false;

    cache->use_mipmapping = use_mipmapping;

    return cache;
}

void
cg_pango_glyph_cache_clear(cg_pango_glyph_cache_t *cache)
{
    cache->has_dirty_glyphs = false;

    g_hash_table_remove_all(cache->hash_table);
}

void
cg_pango_glyph_cache_free(cg_pango_glyph_cache_t *cache)
{
    atlas_closure_state_t *state, *tmp;

    _cg_list_for_each_safe(state, tmp, &cache->atlas_closures, list_node)
    {
        cg_atlas_remove_post_reorganize_callback(state->atlas,
                                                 state->reorganize_closure);
        cg_atlas_remove_allocate_callback(state->atlas,
                                          state->allocate_closure);
        _cg_list_remove(&state->list_node);
        g_slice_free(atlas_closure_state_t, state);
    }

    cg_pango_glyph_cache_clear(cache);

    g_hash_table_unref(cache->hash_table);

    g_hook_list_clear(&cache->reorganize_callbacks);

    g_free(cache);
}

static bool
cg_pango_glyph_cache_add_to_global_atlas(cg_pango_glyph_cache_t *cache,
                                         PangoFont *font,
                                         PangoGlyph glyph,
                                         cg_pango_glyph_cache_value_t *value)
{
    cg_atlas_texture_t *texture;
    cg_error_t *ignore_error = NULL;

    if (CG_DEBUG_ENABLED(CG_DEBUG_DISABLE_SHARED_ATLAS))
        return false;

    /* If the cache is using mipmapping then we can't use the global
       atlas because it would just get migrated back out */
    if (cache->use_mipmapping)
        return false;

    texture = cg_atlas_texture_new_with_size(
        cache->ctx, value->draw_width, value->draw_height);
    if (!cg_texture_allocate(CG_TEXTURE(texture), &ignore_error)) {
        cg_error_free(ignore_error);
        return false;
    }

    value->texture = CG_TEXTURE(texture);
    value->tx1 = 0;
    value->ty1 = 0;
    value->tx2 = 1;
    value->ty2 = 1;
    value->tx_pixel = 0;
    value->ty_pixel = 0;

    return true;
}

static bool
cg_pango_glyph_cache_add_to_local_atlas(cg_pango_glyph_cache_t *cache,
                                        PangoFont *font,
                                        PangoGlyph glyph,
                                        cg_pango_glyph_cache_value_t *value)
{
    cg_atlas_t *atlas;

    /* Add two pixels for the border
     * FIXME: two pixels isn't enough if mipmapping is in use
     */
    atlas = cg_atlas_set_allocate_space(
        cache->atlas_set, value->draw_width + 2, value->draw_height + 2, value);
    if (!atlas)
        return false;

    return true;
}

cg_pango_glyph_cache_value_t *
cg_pango_glyph_cache_lookup(cg_pango_glyph_cache_t *cache,
                            bool create,
                            PangoFont *font,
                            PangoGlyph glyph)
{
    cg_pango_glyph_cache_key_t lookup_key;
    cg_pango_glyph_cache_value_t *value;

    lookup_key.font = font;
    lookup_key.glyph = glyph;

    value = g_hash_table_lookup(cache->hash_table, &lookup_key);

    if (create && value == NULL) {
        cg_pango_glyph_cache_key_t *key;
        PangoRectangle ink_rect;

        value = g_slice_new(cg_pango_glyph_cache_value_t);
        value->texture = NULL;

        pango_font_get_glyph_extents(font, glyph, &ink_rect, NULL);
        pango_extents_to_pixels(&ink_rect, NULL);

        value->draw_x = ink_rect.x;
        value->draw_y = ink_rect.y;
        value->draw_width = ink_rect.width;
        value->draw_height = ink_rect.height;

        /* If the glyph is zero-sized then we don't need to reserve any
           space for it and we can just avoid painting anything */
        if (ink_rect.width < 1 || ink_rect.height < 1)
            value->dirty = false;
        else {
            /* Try adding the glyph to the global atlas set... */
            if (!cg_pango_glyph_cache_add_to_global_atlas(
                    cache, font, glyph, value) &&
                /* If it fails try the local atlas set */
                !cg_pango_glyph_cache_add_to_local_atlas(
                    cache, font, glyph, value)) {
                cg_pango_glyph_cache_value_free(value);
                return NULL;
            }

            value->dirty = true;
            cache->has_dirty_glyphs = true;
        }

        key = g_slice_new(cg_pango_glyph_cache_key_t);
        key->font = g_object_ref(font);
        key->glyph = glyph;

        g_hash_table_insert(cache->hash_table, key, value);
    }

    return value;
}

static void
_cg_pango_glyph_cache_set_dirty_glyphs_cb(void *key_ptr,
                                          void *value_ptr,
                                          void *user_data)
{
    cg_pango_glyph_cache_key_t *key = key_ptr;
    cg_pango_glyph_cache_value_t *value = value_ptr;
    cg_pango_glyph_cache_dirty_func_t func = user_data;

    if (value->dirty) {
        func(key->font, key->glyph, value);

        value->dirty = false;
    }
}

void
_cg_pango_glyph_cache_set_dirty_glyphs(cg_pango_glyph_cache_t *cache,
                                       cg_pango_glyph_cache_dirty_func_t func)
{
    /* If we know that there are no dirty glyphs then we can shortcut
       out early */
    if (!cache->has_dirty_glyphs)
        return;

    g_hash_table_foreach(
        cache->hash_table, _cg_pango_glyph_cache_set_dirty_glyphs_cb, func);

    cache->has_dirty_glyphs = false;
}

void
_cg_pango_glyph_cache_add_reorganize_callback(
    cg_pango_glyph_cache_t *cache, GHookFunc func, void *user_data)
{
    GHook *hook = g_hook_alloc(&cache->reorganize_callbacks);
    hook->func = func;
    hook->data = user_data;
    g_hook_prepend(&cache->reorganize_callbacks, hook);
}

void
_cg_pango_glyph_cache_remove_reorganize_callback(
    cg_pango_glyph_cache_t *cache, GHookFunc func, void *user_data)
{
    GHook *hook = g_hook_find_func_data(
        &cache->reorganize_callbacks, false, func, user_data);

    if (hook)
        g_hook_destroy_link(&cache->reorganize_callbacks, hook);
}
