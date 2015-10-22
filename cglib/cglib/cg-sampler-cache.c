/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Intel Corporation.
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
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#include <cglib-config.h>

#include "cg-sampler-cache-private.h"
#include "cg-device-private.h"
#include "cg-util-gl-private.h"

#ifndef GL_TEXTURE_WRAP_R
#define GL_TEXTURE_WRAP_R 0x8072
#endif

struct _cg_sampler_cache_t {
    cg_device_t *dev;

    /* The samplers are hashed in two tables. One is using the enum
       values that CGlib exposes (so it can include the 'automatic' wrap
       mode) and the other is using the converted values that will be
       given to GL. The first is used to get a unique pointer for the
       sampler state so that pipelines only need to store a single
       pointer instead of the whole state and the second is used so that
       only a single GL sampler object will be created for each unique
       GL state. */
    c_hash_table_t *hash_table_cg;
    c_hash_table_t *hash_table_gl;

    /* This is used for generated fake unique sampler object numbers
       when the sampler object extension is not supported */
    GLuint next_fake_sampler_object_number;
};

static void
canonicalize_key(cg_sampler_cache_entry_t *key)
{
    /* This converts the wrap modes to the enums that will actually be
     * given to GL so that it can be used as a key to get a unique GL
     * sampler object for the state.
     *
     * XXX: actually this is a NOP since the cg wrap mode enum is
     * based on the GL enum values.
     */
}

static bool
wrap_mode_equal_gl(cg_sampler_cache_wrap_mode_t wrap_mode0,
                   cg_sampler_cache_wrap_mode_t wrap_mode1)
{
    /* We want to compare the actual GLenum that will be used so that if
       two different wrap_modes actually use the same GL state we'll
       still use the same sampler object */
    return (cg_sampler_cache_wrap_mode_t)(wrap_mode0) == (cg_sampler_cache_wrap_mode_t)(wrap_mode1);
}

static bool
sampler_state_equal_gl(const void *value0, const void *value1)
{
    const cg_sampler_cache_entry_t *state0 = value0;
    const cg_sampler_cache_entry_t *state1 = value1;

    return (state0->mag_filter == state1->mag_filter &&
            state0->min_filter == state1->min_filter &&
            wrap_mode_equal_gl(state0->wrap_mode_s, state1->wrap_mode_s) &&
            wrap_mode_equal_gl(state0->wrap_mode_t, state1->wrap_mode_t) &&
            wrap_mode_equal_gl(state0->wrap_mode_p, state1->wrap_mode_p));
}

static unsigned int
hash_wrap_mode_gl(unsigned int hash,
                  cg_sampler_cache_wrap_mode_t wrap_mode)
{
    /* We want to hash the actual GLenum that will be used so that if
       two different wrap_modes actually use the same GL state we'll
       still use the same sampler object */
    GLenum real_wrap_mode = (cg_sampler_cache_wrap_mode_t)wrap_mode;

    return _cg_util_one_at_a_time_hash(
        hash, &real_wrap_mode, sizeof(real_wrap_mode));
}

static unsigned int
hash_sampler_state_gl(const void *key)
{
    const cg_sampler_cache_entry_t *entry = key;
    unsigned int hash = 0;

    hash = _cg_util_one_at_a_time_hash(
        hash, &entry->mag_filter, sizeof(entry->mag_filter));
    hash = _cg_util_one_at_a_time_hash(
        hash, &entry->min_filter, sizeof(entry->min_filter));
    hash = hash_wrap_mode_gl(hash, entry->wrap_mode_s);
    hash = hash_wrap_mode_gl(hash, entry->wrap_mode_t);
    hash = hash_wrap_mode_gl(hash, entry->wrap_mode_p);

    return _cg_util_one_at_a_time_mix(hash);
}

static bool
sampler_state_equal_cg(const void *value0, const void *value1)
{
    const cg_sampler_cache_entry_t *state0 = value0;
    const cg_sampler_cache_entry_t *state1 = value1;

    return (state0->mag_filter == state1->mag_filter &&
            state0->min_filter == state1->min_filter &&
            state0->wrap_mode_s == state1->wrap_mode_s &&
            state0->wrap_mode_t == state1->wrap_mode_t &&
            state0->wrap_mode_p == state1->wrap_mode_p);
}

static unsigned int
hash_sampler_state_cg(const void *key)
{
    const cg_sampler_cache_entry_t *entry = key;
    unsigned int hash = 0;

    hash = _cg_util_one_at_a_time_hash(
        hash, &entry->mag_filter, sizeof(entry->mag_filter));
    hash = _cg_util_one_at_a_time_hash(
        hash, &entry->min_filter, sizeof(entry->min_filter));
    hash = _cg_util_one_at_a_time_hash(
        hash, &entry->wrap_mode_s, sizeof(entry->wrap_mode_s));
    hash = _cg_util_one_at_a_time_hash(
        hash, &entry->wrap_mode_t, sizeof(entry->wrap_mode_t));
    hash = _cg_util_one_at_a_time_hash(
        hash, &entry->wrap_mode_p, sizeof(entry->wrap_mode_p));

    return _cg_util_one_at_a_time_mix(hash);
}

cg_sampler_cache_t *
_cg_sampler_cache_new(cg_device_t *dev)
{
    cg_sampler_cache_t *cache = c_new(cg_sampler_cache_t, 1);

    /* No reference is taken on the context because it would create a
       circular reference */
    cache->dev = dev;

    cache->hash_table_gl =
        c_hash_table_new(hash_sampler_state_gl, sampler_state_equal_gl);
    cache->hash_table_cg =
        c_hash_table_new(hash_sampler_state_cg, sampler_state_equal_cg);
    cache->next_fake_sampler_object_number = 1;

    return cache;
}

static void
set_wrap_mode(cg_device_t *dev,
              GLuint sampler_object,
              GLenum param,
              cg_sampler_cache_wrap_mode_t wrap_mode)
{
    GE(dev, glSamplerParameteri(sampler_object, param, wrap_mode));
}

static cg_sampler_cache_entry_t *
_cg_sampler_cache_get_entry_gl(cg_sampler_cache_t *cache,
                               const cg_sampler_cache_entry_t *key)
{
    cg_sampler_cache_entry_t *entry;

    entry = c_hash_table_lookup(cache->hash_table_gl, key);

    if (entry == NULL) {
        cg_device_t *dev = cache->dev;

        entry = c_slice_dup(cg_sampler_cache_entry_t, key);

        if (_cg_has_private_feature(dev,
                                    CG_PRIVATE_FEATURE_SAMPLER_OBJECTS)) {
            GE(dev, glGenSamplers(1, &entry->sampler_object));

            GE(dev,
               glSamplerParameteri(entry->sampler_object,
                                   GL_TEXTURE_MIN_FILTER,
                                   entry->min_filter));
            GE(dev,
               glSamplerParameteri(entry->sampler_object,
                                   GL_TEXTURE_MAG_FILTER,
                                   entry->mag_filter));

            set_wrap_mode(dev,
                          entry->sampler_object,
                          GL_TEXTURE_WRAP_S,
                          entry->wrap_mode_s);
            set_wrap_mode(dev,
                          entry->sampler_object,
                          GL_TEXTURE_WRAP_T,
                          entry->wrap_mode_t);
            set_wrap_mode(dev,
                          entry->sampler_object,
                          GL_TEXTURE_WRAP_R,
                          entry->wrap_mode_p);
        } else {
            /* If sampler objects aren't supported then we'll invent a
               unique number so that pipelines can still compare the
               unique state just by comparing the sampler object
               numbers */
            entry->sampler_object = cache->next_fake_sampler_object_number++;
        }

        c_hash_table_insert(cache->hash_table_gl, entry, entry);
    }

    return entry;
}

static cg_sampler_cache_entry_t *
_cg_sampler_cache_get_entry_cg(cg_sampler_cache_t *cache,
                                 const cg_sampler_cache_entry_t *key)
{
    cg_sampler_cache_entry_t *entry;

    entry = c_hash_table_lookup(cache->hash_table_cg, key);

    if (entry == NULL) {
        cg_sampler_cache_entry_t canonical_key;
        cg_sampler_cache_entry_t *gl_entry;

        entry = c_slice_dup(cg_sampler_cache_entry_t, key);

        /* Get the sampler object number from the canonical GL version
           of the sampler state cache */
        canonical_key = *key;
        canonicalize_key(&canonical_key);
        gl_entry = _cg_sampler_cache_get_entry_gl(cache, &canonical_key);
        entry->sampler_object = gl_entry->sampler_object;

        c_hash_table_insert(cache->hash_table_cg, entry, entry);
    }

    return entry;
}

const cg_sampler_cache_entry_t *
_cg_sampler_cache_get_default_entry(cg_sampler_cache_t *cache)
{
    cg_sampler_cache_entry_t key;

    key.wrap_mode_s = CG_SAMPLER_CACHE_WRAP_MODE_REPEAT;
    key.wrap_mode_t = CG_SAMPLER_CACHE_WRAP_MODE_REPEAT;
    key.wrap_mode_p = CG_SAMPLER_CACHE_WRAP_MODE_REPEAT;

    key.min_filter = GL_LINEAR;
    key.mag_filter = GL_LINEAR;

    return _cg_sampler_cache_get_entry_cg(cache, &key);
}

const cg_sampler_cache_entry_t *
_cg_sampler_cache_update_wrap_modes(cg_sampler_cache_t *cache,
                                    const cg_sampler_cache_entry_t *old_entry,
                                    cg_sampler_cache_wrap_mode_t wrap_mode_s,
                                    cg_sampler_cache_wrap_mode_t wrap_mode_t,
                                    cg_sampler_cache_wrap_mode_t wrap_mode_p)
{
    cg_sampler_cache_entry_t key = *old_entry;

    key.wrap_mode_s = wrap_mode_s;
    key.wrap_mode_t = wrap_mode_t;
    key.wrap_mode_p = wrap_mode_p;

    return _cg_sampler_cache_get_entry_cg(cache, &key);
}

const cg_sampler_cache_entry_t *
_cg_sampler_cache_update_filters(cg_sampler_cache_t *cache,
                                 const cg_sampler_cache_entry_t *old_entry,
                                 GLenum min_filter,
                                 GLenum mag_filter)
{
    cg_sampler_cache_entry_t key = *old_entry;

    key.min_filter = min_filter;
    key.mag_filter = mag_filter;

    return _cg_sampler_cache_get_entry_cg(cache, &key);
}

static void
hash_table_free_gl_cb(void *key, void *value, void *user_data)
{
    cg_device_t *dev = user_data;
    cg_sampler_cache_entry_t *entry = value;

    if (_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_SAMPLER_OBJECTS))
        GE(dev, glDeleteSamplers(1, &entry->sampler_object));

    c_slice_free(cg_sampler_cache_entry_t, entry);
}

static void
hash_table_free_cg_cb(void *key, void *value, void *user_data)
{
    cg_sampler_cache_entry_t *entry = value;

    c_slice_free(cg_sampler_cache_entry_t, entry);
}

void
_cg_sampler_cache_free(cg_sampler_cache_t *cache)
{
    c_hash_table_foreach(
        cache->hash_table_gl, hash_table_free_gl_cb, cache->dev);

    c_hash_table_destroy(cache->hash_table_gl);

    c_hash_table_foreach(
        cache->hash_table_cg, hash_table_free_cg_cb, cache->dev);

    c_hash_table_destroy(cache->hash_table_cg);

    c_free(cache);
}
