/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2013,2014 Intel Corporation.
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
 */

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef _CG_ATLAS_SET_H_
#define _CG_ATLAS_SET_H_

#include <cglib/cg-types.h>
#include <cglib/cg-object.h>
#include <cglib/cg-atlas.h>

/**
 * cg_atlas_set_t:
 *
 * A #cg_atlas_set_t represents a set of #cg_atlas_t<!-- -->es and a
 * #cg_atlas_t represents one texture that is sub divided into smaller
 * allocations.
 *
 * After creating a #cg_atlas_t you can specify a common format for all
 * #cg_atlas_t textures that will belong to that set via
 * cg_atlas_set_set_components() and
 * cg_atlas_set_set_premultiplied(). These can't be changed once you
 * start allocating from the set.
 *
 * Two notable properties of a #cg_atlas_set_t are whether automatic
 * clearing is enabled and whether migration is enabled.
 *
 * Enabling automatic clearing via cg_atlas_set_clear_enabled()
 * ensures that each new #cg_atlas_t texture that's created is
 * initialized to contain zeros for all components. Enabling clearing
 * can be useful for applications that might end up sampling outside
 * the bounds of individual atlas allocations due to filtering so they
 * can avoid random values bleeding into samples, resulting in
 * artefacts.
 *
 * When there is not enough room in an atlas texture for a new
 * allocation, CGlib will try to allocate a larger texture and then
 * migrate the contents of previous allocations to the new, larger
 * texture. For images that can easily be re-created and that are
 * perhaps only used in an add-hoc fashion it may not be worthwhile
 * the cost of migrating the previous allocations. Migration of
 * allocations can be disabled via
 * cg_atlas_set_set_migration_enabled(). With migrations disabled
 * then previous allocations will be re-allocated space in any
 * replacement texture, but no image data will be copied.
 */
typedef struct _cg_atlas_set_t cg_atlas_set_t;

/**
 * cg_atlas_set_new:
 * @dev: A #cg_device_t pointer
 *
 * Return value: A newly allocated #cg_atlas_set_t
 */
cg_atlas_set_t *cg_atlas_set_new(cg_device_t *dev);

bool cg_is_atlas_set(void *object);

void cg_atlas_set_set_components(cg_atlas_set_t *set,
                                 cg_texture_components_t components);

cg_texture_components_t cg_atlas_set_get_components(cg_atlas_set_t *set);

void cg_atlas_set_set_premultiplied(cg_atlas_set_t *set, bool premultiplied);

bool cg_atlas_set_get_premultiplied(cg_atlas_set_t *set);

void cg_atlas_set_set_clear_enabled(cg_atlas_set_t *set, bool clear_enabled);

bool cg_atlas_set_get_clear_enabled(cg_atlas_set_t *set, bool clear_enabled);

void cg_atlas_set_set_migration_enabled(cg_atlas_set_t *set,
                                        bool migration_enabled);

bool cg_atlas_set_get_migration_enabled(cg_atlas_set_t *set);

void cg_atlas_set_clear(cg_atlas_set_t *set);

typedef enum _cg_atlas_set_event_t {
    CG_ATLAS_SET_EVENT_ADDED = 1,
    CG_ATLAS_SET_EVENT_REMOVED = 2
} cg_atlas_set_event_t;

typedef struct _cg_closure_t cg_atlas_set_atlas_closure_t;

typedef void (*cg_atlas_set_atlas_callback_t)(cg_atlas_set_t *set,
                                              cg_atlas_t *atlas,
                                              cg_atlas_set_event_t event,
                                              void *user_data);

cg_atlas_set_atlas_closure_t *
cg_atlas_set_add_atlas_callback(cg_atlas_set_t *set,
                                cg_atlas_set_atlas_callback_t callback,
                                void *user_data,
                                cg_user_data_destroy_callback_t destroy);

void cg_atlas_set_remove_atlas_callback(cg_atlas_set_t *set,
                                        cg_atlas_set_atlas_closure_t *closure);

cg_atlas_t *cg_atlas_set_allocate_space(cg_atlas_set_t *set,
                                        int width,
                                        int height,
                                        void *allocation_data);

typedef void (*cg_atlas_set_foreach_callback_t)(cg_atlas_t *atlas,
                                                void *user_data);

void cg_atlas_set_foreach(cg_atlas_set_t *atlas_set,
                          cg_atlas_set_foreach_callback_t callback,
                          void *user_data);

#endif /* _CG_ATLAS_SET_H_ */
