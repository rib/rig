/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2015 Intel Corporation
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

#include "rut-text-blob.h"

struct _rut_text_blob_t {
    rut_object_base_t _base;

    char *text;
};

static rut_object_t *
_rut_text_blob_copy(rut_object_t *object)
{
    rut_text_blob_t *blob = object;

    return rut_text_blob_new(blob->text);
}

static bool
_rut_text_blob_has(rut_object_t *object, rut_mimable_type_t type)
{
    if (type == RUT_MIMABLE_TYPE_TEXT)
        return true;

    return false;
}

static void *
_rut_text_blob_get(rut_object_t *object, rut_mimable_type_t type)
{
    rut_text_blob_t *blob = object;

    if (type == RUT_MIMABLE_TYPE_TEXT)
        return blob->text;
    else
        return NULL;
}

static void
_rut_text_blob_free(void *object)
{
    rut_text_blob_t *blob = object;

    c_free(blob->text);

    rut_object_free(rut_text_blob_t, object);
}

rut_type_t rut_text_blob_type;

static void
_rut_text_blob_init_type(void)
{
    static rut_mimable_vtable_t mimable_vtable = { .copy = _rut_text_blob_copy,
                                                   .has = _rut_text_blob_has,
                                                   .get = _rut_text_blob_get, };

    rut_type_t *type = &rut_text_blob_type;
#define TYPE rut_text_blob_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rut_text_blob_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_MIMABLE,
                       0, /* no associated properties */
                       &mimable_vtable);

#undef TYPE
}

rut_text_blob_t *
rut_text_blob_new(const char *text)
{
    rut_text_blob_t *blob = rut_object_alloc0(
        rut_text_blob_t, &rut_text_blob_type, _rut_text_blob_init_type);

    blob->text = c_strdup(text);

    return blob;
}
