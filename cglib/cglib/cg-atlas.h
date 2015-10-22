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

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef _CG_ATLAS_H_
#define _CG_ATLAS_H_

#include <cglib/cg-types.h>
#include <cglib/cg-object.h>
#include <cglib/cg-texture.h>

typedef struct _cg_atlas_allocation_t {
    int x;
    int y;
    int width;
    int height;
} cg_atlas_allocation_t;

typedef struct _cg_atlas_t cg_atlas_t;

/* XXX: Note that during migration _cg_atlas_get_texture() may not match the
 * @texture given here. @texture is more up to date... */
typedef void (*cg_atlas_allocate_callback_t)(
    cg_atlas_t *atlas,
    cg_texture_t *texture,
    const cg_atlas_allocation_t *allocation,
    void *allocation_data,
    void *user_data);

typedef struct _cg_closure_t cg_atlas_allocate_closure_t;

cg_atlas_allocate_closure_t *
cg_atlas_add_allocate_callback(cg_atlas_t *atlas,
                               cg_atlas_allocate_callback_t callback,
                               void *user_data,
                               cg_user_data_destroy_callback_t destroy);

void cg_atlas_remove_allocate_callback(cg_atlas_t *atlas,
                                       cg_atlas_allocate_closure_t *closure);

cg_texture_t *cg_atlas_get_texture(cg_atlas_t *atlas);

typedef void (*cg_atlas_foreach_callback_t)(
    cg_atlas_t *atlas,
    const cg_atlas_allocation_t *allocation,
    void *allocation_data,
    void *user_data);
void cg_atlas_foreach(cg_atlas_t *atlas,
                      cg_atlas_foreach_callback_t callback,
                      void *user_data);

int cg_atlas_get_n_allocations(cg_atlas_t *atlas);

typedef struct _cg_closure_t cg_atlas_reorganize_closure_t;

typedef void (*cg_atlas_reorganize_callback_t)(cg_atlas_t *atlas,
                                               void *user_data);

cg_atlas_reorganize_closure_t *
cg_atlas_add_pre_reorganize_callback(cg_atlas_t *atlas,
                                     cg_atlas_reorganize_callback_t callback,
                                     void *user_data,
                                     cg_user_data_destroy_callback_t destroy);

void
cg_atlas_remove_pre_reorganize_callback(cg_atlas_t *atlas,
                                        cg_atlas_reorganize_closure_t *closure);

cg_atlas_reorganize_closure_t *
cg_atlas_add_post_reorganize_callback(cg_atlas_t *atlas,
                                      cg_atlas_reorganize_callback_t callback,
                                      void *user_data,
                                      cg_user_data_destroy_callback_t destroy);

void
cg_atlas_remove_post_reorganize_callback(cg_atlas_t *atlas,
                                         cg_atlas_reorganize_closure_t *closure);

bool cg_is_atlas(void *object);

#endif /* _CG_ATLAS_H_ */
