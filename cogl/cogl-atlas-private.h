/*
 * Cogl
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

#ifndef _COGL_ATLAS_PRIVATE_H_
#define _COGL_ATLAS_PRIVATE_H_

#include "cogl-object-private.h"
#include "cogl-texture.h"
#include "cogl-list.h"
#include "cogl-rectangle-map.h"
#include "cogl-atlas.h"

typedef enum
{
  COGL_ATLAS_CLEAR_TEXTURE     = (1 << 0),
  COGL_ATLAS_DISABLE_MIGRATION = (1 << 1)
} CoglAtlasFlags;

struct _CoglAtlas
{
  CoglObject _parent;

  CoglContext *context;

  CoglRectangleMap *map;

  CoglTexture *texture;
  CoglPixelFormat internal_format;
  CoglAtlasFlags flags;

  CoglList allocate_closures;

  CoglList pre_reorganize_closures;
  CoglList post_reorganize_closures;
};

CoglAtlas *
_cogl_atlas_new (CoglContext *context,
                 CoglPixelFormat internal_format,
                 CoglAtlasFlags flags);

bool
_cogl_atlas_allocate_space (CoglAtlas *atlas,
                            int width,
                            int height,
                            void *allocation_data);

void
_cogl_atlas_remove (CoglAtlas *atlas,
                    int x,
                    int y,
                    int width,
                    int height);

CoglTexture *
_cogl_atlas_migrate_allocation (CoglAtlas *atlas,
                                int x,
                                int y,
                                int width,
                                int height,
                                CoglPixelFormat internal_format);

#endif /* _COGL_ATLAS_PRIVATE_H_ */
