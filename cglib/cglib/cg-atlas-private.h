/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010,2011 Intel Corporation.
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

#ifndef _CG_ATLAS_PRIVATE_H_
#define _CG_ATLAS_PRIVATE_H_

#include <clib.h>

#include "cg-object-private.h"
#include "cg-texture.h"
#include "cg-rectangle-map.h"
#include "cg-atlas.h"

typedef enum {
    CG_ATLAS_CLEAR_TEXTURE = (1 << 0),
    CG_ATLAS_DISABLE_MIGRATION = (1 << 1)
} cg_atlas_flags_t;

struct _cg_atlas_t {
    cg_object_t _parent;

    cg_device_t *dev;

    cg_rectangle_map_t *map;

    cg_texture_t *texture;
    cg_pixel_format_t internal_format;
    cg_atlas_flags_t flags;

    c_list_t allocate_closures;

    c_list_t pre_reorganize_closures;
    c_list_t post_reorganize_closures;
};

cg_atlas_t *_cg_atlas_new(cg_device_t *dev,
                          cg_pixel_format_t internal_format,
                          cg_atlas_flags_t flags);

bool _cg_atlas_allocate_space(cg_atlas_t *atlas,
                              int width,
                              int height,
                              void *allocation_data);

void _cg_atlas_remove(cg_atlas_t *atlas, int x, int y, int width, int height);

cg_texture_t *_cg_atlas_migrate_allocation(cg_atlas_t *atlas,
                                           int x,
                                           int y,
                                           int width,
                                           int height,
                                           cg_pixel_format_t internal_format);

#endif /* _CG_ATLAS_PRIVATE_H_ */
