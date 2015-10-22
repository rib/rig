/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010 Intel Corporation.
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

#ifndef __CG_TEXTURE_PIXMAP_X11_H
#define __CG_TEXTURE_PIXMAP_X11_H

/* NB: this is a top-level header that can be included directly but we
 * want to be careful not to define __CG_H_INSIDE__ when this is
 * included internally while building CGlib itself since
 * __CG_H_INSIDE__ is used in headers to guard public vs private api
 * definitions
 */
#ifndef CG_COMPILATION
#define __CG_H_INSIDE__
#endif

#include <cglib/cg-device.h>

CG_BEGIN_DECLS

/**
 * SECTION:cg-texture-pixmap-x11
 * @short_description: Functions for creating and manipulating 2D meta
 *                     textures derived from X11 pixmaps.
 *
 * These functions allow high-level meta textures (See the
 * #cg_meta_texture_t interface) that derive their contents from an X11
 * pixmap.
 */

typedef struct _cg_texture_pixmap_x11_t cg_texture_pixmap_x11_t;

#define CG_TEXTURE_PIXMAP_X11(X) ((cg_texture_pixmap_x11_t *)X)

typedef enum {
    CG_TEXTURE_PIXMAP_X11_DAMAGE_RAW_RECTANGLES,
    CG_TEXTURE_PIXMAP_X11_DAMAGE_DELTA_RECTANGLES,
    CG_TEXTURE_PIXMAP_X11_DAMAGE_BOUNDING_BOX,
    CG_TEXTURE_PIXMAP_X11_DAMAGE_NON_EMPTY
} cg_texture_pixmap_x11_report_level_t;

/**
 * CG_TEXTURE_PIXMAP_X11_ERROR:
 *
 * #cg_error_t domain for texture-pixmap-x11 errors.
 *
 */
#define CG_TEXTURE_PIXMAP_X11_ERROR (cg_texture_pixmap_x11_error_domain())

/**
 * cg_texture_pixmap_x11_error_t:
 * @CG_TEXTURE_PIXMAP_X11_ERROR_X11: An X11 protocol error
 *
 * Error codes that can be thrown when performing texture-pixmap-x11
 * operations.
 *
 */
typedef enum {
    CG_TEXTURE_PIXMAP_X11_ERROR_X11,
} cg_texture_pixmap_x11_error_t;

uint32_t cg_texture_pixmap_x11_error_domain(void);

/**
 * cg_texture_pixmap_x11_new:
 * @dev: A #cg_device_t
 * @pixmap: A X11 pixmap ID
 * @automatic_updates: Whether to automatically copy the contents of
 * the pixmap to the texture.
 * @error: A #cg_error_t for exceptions
 *
 * Creates a texture that contains the contents of @pixmap. If
 * @automatic_updates is %true then CGlib will attempt to listen for
 * damage events on the pixmap and automatically update the texture
 * when it changes.
 *
 * Return value: a new #cg_texture_pixmap_x11_t instance
 *
 * Stability: Unstable
 */
cg_texture_pixmap_x11_t *cg_texture_pixmap_x11_new(cg_device_t *dev,
                                                   uint32_t pixmap,
                                                   bool automatic_updates,
                                                   cg_error_t **error);

/**
 * cg_texture_pixmap_x11_update_area:
 * @texture: A #cg_texture_pixmap_x11_t instance
 * @x: x coordinate of the area to update
 * @y: y coordinate of the area to update
 * @width: width of the area to update
 * @height: height of the area to update
 *
 * Forces an update of the given @texture so that it is refreshed with
 * the contents of the pixmap that was given to
 * cg_texture_pixmap_x11_new().
 *
 * Stability: Unstable
 */
void cg_texture_pixmap_x11_update_area(
    cg_texture_pixmap_x11_t *texture, int x, int y, int width, int height);

/**
 * cg_texture_pixmap_x11_is_using_tfp_extension:
 * @texture: A #cg_texture_pixmap_x11_t instance
 *
 * Checks whether the given @texture is using the
 * GLX_EXT_texture_from_pixmap or similar extension to copy the
 * contents of the pixmap to the texture.  This extension is usually
 * implemented as zero-copy operation so it implies the updates are
 * working efficiently.
 *
 * Return value: %true if the texture is using an efficient extension
 *   and %false otherwise
 *
 * Stability: Unstable
 */
bool
cg_texture_pixmap_x11_is_using_tfp_extension(cg_texture_pixmap_x11_t *texture);

/**
 * cg_texture_pixmap_x11_set_damage_object:
 * @texture: A #cg_texture_pixmap_x11_t instance
 * @damage: A X11 Damage object or 0
 * @report_level: The report level which describes how to interpret
 *   the damage events. This should match the level that the damage
 *   object was created with.
 *
 * Sets the damage object that will be used to track automatic updates
 * to the @texture. Damage tracking can be disabled by passing 0 for
 * @damage. Otherwise this damage will replace the one used if %true
 * was passed for automatic_updates to cg_texture_pixmap_x11_new().
 *
 * Note that CGlib will subtract from the damage region as it processes
 * damage events.
 *
 * Stability: Unstable
 */
void cg_texture_pixmap_x11_set_damage_object(
    cg_texture_pixmap_x11_t *texture,
    uint32_t damage,
    cg_texture_pixmap_x11_report_level_t report_level);

/**
 * cg_is_texture_pixmap_x11:
 * @object: A pointer to a #cg_object_t
 *
 * Checks whether @object points to a #cg_texture_pixmap_x11_t instance.
 *
 * Return value: %true if the object is a #cg_texture_pixmap_x11_t, and
 *   %false otherwise
 *
 * Stability: Unstable
 */
bool cg_is_texture_pixmap_x11(void *object);

CG_END_DECLS

#ifndef CG_COMPILATION
#undef __CG_H_INSIDE__
#endif

#endif /* __CG_TEXTURE_PIXMAP_X11_H */
