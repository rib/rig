/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012, 2013 Intel Corporation.
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

#include <rut-config.h>

#include <cglib/cglib.h>

#include <clib.h>

#include "rut-texture-cache.h"

typedef struct _rut_texture_cache_entry_t {
    rut_shell_t *shell;
    c_quark_t filename_quark;
    cg_texture_t *texture;
} rut_texture_cache_entry_t;

static cg_user_data_key_t texture_cache_key;

static void
_rut_texture_cache_entry_free(rut_texture_cache_entry_t *entry)
{
    c_slice_free(rut_texture_cache_entry_t, entry);
}

static void
_rut_texture_cache_entry_destroy_cb(void *entry)
{
    _rut_texture_cache_entry_free(entry);
}

void
rut_texture_cache_init(rut_shell_t *shell)
{
    shell->texture_cache =
        c_hash_table_new_full(c_direct_hash,
                              c_direct_equal,
                              NULL,
                              _rut_texture_cache_entry_destroy_cb);
}

static void
texture_destroyed_cb(void *user_data)
{
    rut_texture_cache_entry_t *entry = user_data;

    c_hash_table_remove(entry->shell->texture_cache,
                        C_UINT_TO_POINTER(entry->filename_quark));
}

cg_texture_t *
rut_load_texture(rut_shell_t *shell, const char *filename, c_error_t **error)
{
    c_quark_t filename_quark = c_quark_from_string(filename);
    rut_texture_cache_entry_t *entry =
        c_hash_table_lookup(shell->texture_cache,
                            C_UINT_TO_POINTER(filename_quark));
    cg_texture_t *texture;
    cg_error_t *catch = NULL;

    if (entry)
        return cg_object_ref(entry->texture);

    texture = (cg_texture_t *)cg_texture_2d_new_from_file(shell->cg_device,
                                                          filename, &catch);
    if (!texture) {
        *error = c_error_new(C_FILE_ERROR,
                             C_FILE_ERROR_FAILED,
                             catch->message);
        cg_error_free(catch);
        return NULL;
    }

    entry = c_slice_new0(rut_texture_cache_entry_t);
    entry->shell = shell;
    entry->filename_quark = filename_quark;
    /* Note: we don't take a reference on the texture. The aim of this
     * cache is simply to avoid multiple loads of the same file and
     * doesn't affect the lifetime of the tracked textures. */
    entry->texture = texture;

    /* Track when the texture is freed... */
    cg_object_set_user_data(
        texture, &texture_cache_key, entry, texture_destroyed_cb);

    c_hash_table_insert(shell->texture_cache,
                        C_UINT_TO_POINTER(filename_quark), entry);

    return texture;
}

cg_texture_t *
rut_load_texture_from_data_file(rut_shell_t *shell,
                                const char *filename,
                                c_error_t **error)
{
    char *full_path = rut_find_data_file(filename);
    cg_texture_t *tex;

    if (full_path == NULL) {
        *error = c_error_new(C_FILE_ERROR,
                             C_FILE_ERROR_EXIST,
                             "File not found");
        return NULL;
    }

    tex = rut_load_texture(shell, full_path, error);
    c_free(full_path);

    return tex;
}

void
rut_texture_cache_destroy(rut_shell_t *shell)
{
    c_hash_table_destroy(shell->texture_cache);
}
