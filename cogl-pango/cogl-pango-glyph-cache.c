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

typedef struct _CoglPangoGlyphCacheKey     CoglPangoGlyphCacheKey;

typedef struct _AtlasClosureState
{
  CoglList list_node;
  CoglAtlas *atlas;
  CoglAtlasReorganizeClosure *reorganize_closure;
  CoglAtlasAllocateClosure *allocate_closure;
} AtlasClosureState;

struct _CoglPangoGlyphCache
{
  CoglContext *ctx;

  /* Hash table to quickly check whether a particular glyph in a
     particular font is already cached */
  GHashTable *hash_table;

  /* Set of CoglAtlases */
  CoglAtlasSet *atlas_set;

  CoglList atlas_closures;

  /* List of callbacks to invoke when an atlas is reorganized */
  GHookList reorganize_callbacks;

  /* True if some of the glyphs are dirty. This is used as an
     optimization in _cogl_pango_glyph_cache_set_dirty_glyphs to avoid
     iterating the hash table if we know none of them are dirty */
  bool has_dirty_glyphs;

  /* Whether mipmapping is being used for this cache. This only
     affects whether we decide to put the glyph in the global atlas */
  bool use_mipmapping;
};

struct _CoglPangoGlyphCacheKey
{
  PangoFont  *font;
  PangoGlyph  glyph;
};

static void
cogl_pango_glyph_cache_value_free (CoglPangoGlyphCacheValue *value)
{
  if (value->texture)
    {
      cogl_object_unref (value->texture);
      cogl_object_unref (value->atlas);
    }
  g_slice_free (CoglPangoGlyphCacheValue, value);
}

static void
cogl_pango_glyph_cache_key_free (CoglPangoGlyphCacheKey *key)
{
  g_object_unref (key->font);
  g_slice_free (CoglPangoGlyphCacheKey, key);
}

static guint
cogl_pango_glyph_cache_hash_func (const void *key)
{
  const CoglPangoGlyphCacheKey *cache_key
    = (const CoglPangoGlyphCacheKey *) key;

  /* Generate a number affected by both the font and the glyph
     number. We can safely directly compare the pointers because the
     key holds a reference to the font so it is not possible that a
     different font will have the same memory address */
  return GPOINTER_TO_UINT (cache_key->font) ^ cache_key->glyph;
}

static gboolean
cogl_pango_glyph_cache_equal_func (const void *a, const void *b)
{
  const CoglPangoGlyphCacheKey *key_a
    = (const CoglPangoGlyphCacheKey *) a;
  const CoglPangoGlyphCacheKey *key_b
    = (const CoglPangoGlyphCacheKey *) b;

  /* We can safely directly compare the pointers for the fonts because
     the key holds a reference to the font so it is not possible that
     a different font will have the same memory address */
  return key_a->font == key_b->font
    && key_a->glyph == key_b->glyph;
}

static void
atlas_reorganize_cb (CoglAtlas *atlas, void *user_data)
{
  CoglPangoGlyphCache *cache = user_data;

  g_hook_list_invoke (&cache->reorganize_callbacks, false);
}

static void
allocate_glyph_cb (CoglAtlas *atlas,
                   CoglTexture *texture,
                   const CoglAtlasAllocation *allocation,
                   void *allocation_data,
                   void *user_data)
{
  CoglPangoGlyphCacheValue *value = allocation_data;
  float tex_width, tex_height;

  if (value->texture)
    {
      cogl_object_unref (value->texture);
      cogl_object_unref (value->atlas);
    }
  value->atlas = cogl_object_ref (atlas);
  value->texture = cogl_object_ref (texture);

  tex_width = cogl_texture_get_width (texture);
  tex_height = cogl_texture_get_height (texture);

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
atlas_callback (CoglAtlasSet *set,
                CoglAtlas *atlas,
                CoglAtlasSetEvent event,
                void *user_data)
{
  CoglPangoGlyphCache *cache = user_data;
  AtlasClosureState *state;

  switch (event)
    {
    case COGL_ATLAS_SET_EVENT_ADDED:
      state = g_slice_new (AtlasClosureState);
      state->atlas = atlas;
      state->reorganize_closure =
        cogl_atlas_add_post_reorganize_callback (atlas,
                                                 atlas_reorganize_cb,
                                                 cache,
                                                 NULL); /* destroy */
      state->allocate_closure =
        cogl_atlas_add_allocate_callback (atlas,
                                          allocate_glyph_cb,
                                          cache,
                                          NULL); /* destroy */

      _cogl_list_insert (cache->atlas_closures.prev, &state->list_node);
      break;
    case COGL_ATLAS_SET_EVENT_REMOVED:
      break;
    }
}

static void
add_global_atlas_cb (CoglAtlas *atlas,
                     void *user_data)
{
  CoglPangoGlyphCache *cache = user_data;

  atlas_callback (_cogl_get_atlas_set (cache->ctx),
                  atlas,
                  COGL_ATLAS_SET_EVENT_ADDED,
                  cache);
}

CoglPangoGlyphCache *
cogl_pango_glyph_cache_new (CoglContext *ctx,
                            bool use_mipmapping)
{
  CoglPangoGlyphCache *cache;

  cache = g_malloc (sizeof (CoglPangoGlyphCache));

  /* Note: as a rule we don't take references to a CoglContext
   * internally since */
  cache->ctx = ctx;

  cache->hash_table = g_hash_table_new_full
    (cogl_pango_glyph_cache_hash_func,
     cogl_pango_glyph_cache_equal_func,
     (c_destroy_func_t) cogl_pango_glyph_cache_key_free,
     (c_destroy_func_t) cogl_pango_glyph_cache_value_free);

  _cogl_list_init (&cache->atlas_closures);

  cache->atlas_set = cogl_atlas_set_new (ctx);

  cogl_atlas_set_set_components (cache->atlas_set,
                                 COGL_TEXTURE_COMPONENTS_A);

  cogl_atlas_set_set_migration_enabled (cache->atlas_set, false);
  cogl_atlas_set_set_clear_enabled (cache->atlas_set, true);

  /* We want to be notified when new atlases are added to our local
   * atlas set so they can be monitored for being re-arranged... */
  cogl_atlas_set_add_atlas_callback (cache->atlas_set,
                                     atlas_callback,
                                     cache,
                                     NULL); /* destroy */

  /* We want to be notified when new atlases are added to the global
   * atlas set so they can be monitored for being re-arranged... */
  cogl_atlas_set_add_atlas_callback (_cogl_get_atlas_set (ctx),
                                     atlas_callback,
                                     cache,
                                     NULL); /* destroy */
  /* The global atlas set may already have atlases that we will
   * want to monitor... */
  cogl_atlas_set_foreach (_cogl_get_atlas_set (ctx),
                          add_global_atlas_cb,
                          cache);

  g_hook_list_init (&cache->reorganize_callbacks, sizeof (GHook));

  cache->has_dirty_glyphs = false;

  cache->use_mipmapping = use_mipmapping;

  return cache;
}

void
cogl_pango_glyph_cache_clear (CoglPangoGlyphCache *cache)
{
  cache->has_dirty_glyphs = false;

  g_hash_table_remove_all (cache->hash_table);
}

void
cogl_pango_glyph_cache_free (CoglPangoGlyphCache *cache)
{
  AtlasClosureState *state, *tmp;

  _cogl_list_for_each_safe (state, tmp, &cache->atlas_closures, list_node)
    {
      cogl_atlas_remove_post_reorganize_callback (state->atlas,
                                                  state->reorganize_closure);
      cogl_atlas_remove_allocate_callback (state->atlas,
                                           state->allocate_closure);
      _cogl_list_remove (&state->list_node);
      g_slice_free (AtlasClosureState, state);
    }

  cogl_pango_glyph_cache_clear (cache);

  g_hash_table_unref (cache->hash_table);

  g_hook_list_clear (&cache->reorganize_callbacks);

  g_free (cache);
}

static bool
cogl_pango_glyph_cache_add_to_global_atlas (CoglPangoGlyphCache *cache,
                                            PangoFont *font,
                                            PangoGlyph glyph,
                                            CoglPangoGlyphCacheValue *value)
{
  CoglAtlasTexture *texture;
  CoglError *ignore_error = NULL;

  if (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_SHARED_ATLAS))
    return false;

  /* If the cache is using mipmapping then we can't use the global
     atlas because it would just get migrated back out */
  if (cache->use_mipmapping)
    return false;

  texture = cogl_atlas_texture_new_with_size (cache->ctx,
                                              value->draw_width,
                                              value->draw_height);
  if (!cogl_texture_allocate (COGL_TEXTURE (texture), &ignore_error))
    {
      cogl_error_free (ignore_error);
      return false;
    }

  value->texture = COGL_TEXTURE (texture);
  value->tx1 = 0;
  value->ty1 = 0;
  value->tx2 = 1;
  value->ty2 = 1;
  value->tx_pixel = 0;
  value->ty_pixel = 0;

  return true;
}

static bool
cogl_pango_glyph_cache_add_to_local_atlas (CoglPangoGlyphCache *cache,
                                           PangoFont *font,
                                           PangoGlyph glyph,
                                           CoglPangoGlyphCacheValue *value)
{
  CoglAtlas *atlas;

  /* Add two pixels for the border
   * FIXME: two pixels isn't enough if mipmapping is in use
   */
  atlas = cogl_atlas_set_allocate_space (cache->atlas_set,
                                         value->draw_width + 2,
                                         value->draw_height + 2,
                                         value);
  if (!atlas)
    return false;

  return true;
}

CoglPangoGlyphCacheValue *
cogl_pango_glyph_cache_lookup (CoglPangoGlyphCache *cache,
                               bool             create,
                               PangoFont           *font,
                               PangoGlyph           glyph)
{
  CoglPangoGlyphCacheKey lookup_key;
  CoglPangoGlyphCacheValue *value;

  lookup_key.font = font;
  lookup_key.glyph = glyph;

  value = g_hash_table_lookup (cache->hash_table, &lookup_key);

  if (create && value == NULL)
    {
      CoglPangoGlyphCacheKey *key;
      PangoRectangle ink_rect;

      value = g_slice_new (CoglPangoGlyphCacheValue);
      value->texture = NULL;

      pango_font_get_glyph_extents (font, glyph, &ink_rect, NULL);
      pango_extents_to_pixels (&ink_rect, NULL);

      value->draw_x = ink_rect.x;
      value->draw_y = ink_rect.y;
      value->draw_width = ink_rect.width;
      value->draw_height = ink_rect.height;

      /* If the glyph is zero-sized then we don't need to reserve any
         space for it and we can just avoid painting anything */
      if (ink_rect.width < 1 || ink_rect.height < 1)
        value->dirty = false;
      else
        {
          /* Try adding the glyph to the global atlas set... */
          if (!cogl_pango_glyph_cache_add_to_global_atlas (cache,
                                                           font,
                                                           glyph,
                                                           value) &&
              /* If it fails try the local atlas set */
              !cogl_pango_glyph_cache_add_to_local_atlas (cache,
                                                          font,
                                                          glyph,
                                                          value))
            {
              cogl_pango_glyph_cache_value_free (value);
              return NULL;
            }

          value->dirty = true;
          cache->has_dirty_glyphs = true;
        }

      key = g_slice_new (CoglPangoGlyphCacheKey);
      key->font = g_object_ref (font);
      key->glyph = glyph;

      g_hash_table_insert (cache->hash_table, key, value);
    }

  return value;
}

static void
_cogl_pango_glyph_cache_set_dirty_glyphs_cb (void *key_ptr,
                                             void *value_ptr,
                                             void *user_data)
{
  CoglPangoGlyphCacheKey *key = key_ptr;
  CoglPangoGlyphCacheValue *value = value_ptr;
  CoglPangoGlyphCacheDirtyFunc func = user_data;

  if (value->dirty)
    {
      func (key->font, key->glyph, value);

      value->dirty = false;
    }
}

void
_cogl_pango_glyph_cache_set_dirty_glyphs (CoglPangoGlyphCache *cache,
                                          CoglPangoGlyphCacheDirtyFunc func)
{
  /* If we know that there are no dirty glyphs then we can shortcut
     out early */
  if (!cache->has_dirty_glyphs)
    return;

  g_hash_table_foreach (cache->hash_table,
                        _cogl_pango_glyph_cache_set_dirty_glyphs_cb,
                        func);

  cache->has_dirty_glyphs = false;
}

void
_cogl_pango_glyph_cache_add_reorganize_callback (CoglPangoGlyphCache *cache,
                                                 GHookFunc func,
                                                 void *user_data)
{
  GHook *hook = g_hook_alloc (&cache->reorganize_callbacks);
  hook->func = func;
  hook->data = user_data;
  g_hook_prepend (&cache->reorganize_callbacks, hook);
}

void
_cogl_pango_glyph_cache_remove_reorganize_callback (CoglPangoGlyphCache *cache,
                                                    GHookFunc func,
                                                    void *user_data)
{
  GHook *hook = g_hook_find_func_data (&cache->reorganize_callbacks,
                                       false,
                                       func,
                                       user_data);

  if (hook)
    g_hook_destroy_link (&cache->reorganize_callbacks, hook);
}
