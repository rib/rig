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

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef _COGL_ATLAS_H_
#define _COGL_ATLAS_H_

#include <cogl/cogl-types.h>
#include <cogl/cogl-object.h>
#include <cogl/cogl-texture.h>

typedef struct _CoglAtlasAllocation
{
  int x;
  int y;
  int width;
  int height;
} CoglAtlasAllocation;

typedef struct _CoglAtlas CoglAtlas;

/* XXX: Note that during migration _cogl_atlas_get_texture() may not match the
 * @texture given here. @texture is more up to date... */
typedef void
(* CoglAtlasAllocateCallback) (CoglAtlas *atlas,
                               CoglTexture *texture,
                               const CoglAtlasAllocation *allocation,
                               void *allocation_data,
                               void *user_data);

typedef struct _CoglClosure CoglAtlasAllocateClosure;

CoglAtlasAllocateClosure *
cogl_atlas_add_allocate_callback (CoglAtlas *atlas,
                                  CoglAtlasAllocateCallback callback,
                                  void *user_data,
                                  CoglUserDataDestroyCallback destroy);

void
cogl_atlas_remove_allocate_callback (CoglAtlas *atlas,
                                     CoglAtlasAllocateClosure *closure);

CoglTexture *
cogl_atlas_get_texture (CoglAtlas *atlas);

typedef void (*CoglAtlasForeachCallback) (CoglAtlas *atlas,
                                          const CoglAtlasAllocation *allocation,
                                          void *allocation_data,
                                          void *user_data);
void
cogl_atlas_foreach (CoglAtlas *atlas,
                    CoglAtlasForeachCallback callback,
                    void *user_data);

int
cogl_atlas_get_n_allocations (CoglAtlas *atlas);

typedef struct _CoglClosure CoglAtlasReorganizeClosure;

typedef void (*CoglAtlasReorganizeCallback) (CoglAtlas *atlas,
                                             void *user_data);

CoglAtlasReorganizeClosure *
cogl_atlas_add_pre_reorganize_callback (CoglAtlas *atlas,
                                        CoglAtlasReorganizeCallback callback,
                                        void *user_data,
                                        CoglUserDataDestroyCallback destroy);

void
cogl_atlas_remove_pre_reorganize_callback (CoglAtlas *atlas,
                                           CoglAtlasReorganizeClosure *closure);

CoglAtlasReorganizeClosure *
cogl_atlas_add_post_reorganize_callback (CoglAtlas *atlas,
                                         CoglAtlasReorganizeCallback callback,
                                         void *user_data,
                                         CoglUserDataDestroyCallback destroy);

void
cogl_atlas_remove_post_reorganize_callback (CoglAtlas *atlas,
                                            CoglAtlasReorganizeClosure *closure);

bool
cogl_is_atlas (void *object);

#endif /* _COGL_ATLAS_H_ */
