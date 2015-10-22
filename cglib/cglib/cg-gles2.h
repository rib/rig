/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Collabora Ltd.
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
 * Authors:
 *  Tomeu Vizoso <tomeu.vizoso@collabora.com>
 *  Robert Bragg <robert@linux.intel.com>
 *
 */

#ifndef __CG_GLES2_H__
#define __CG_GLES2_H__

/* NB: cg-gles2.h is a top-level header that can be included directly
 * but we want to be careful not to define __CG_H_INSIDE__ when this
 * is included internally while building CGlib itself since
 * __CG_H_INSIDE__ is used in headers to guard public vs private
 * api definitions
 */
#ifndef CG_COMPILATION
#define __CG_H_INSIDE__
#endif

#include <cglib/cg-defines.h>
#include <cglib/cg-device.h>
#include <cglib/cg-framebuffer.h>
#include <cglib/cg-texture.h>
#include <cglib/cg-texture-2d.h>

/* cg_gles2_vtable_t depends on GLES 2.0 typedefs being available but we
 * want to be careful that the public api doesn't expose arbitrary
 * system GL headers as part of the CGlib API so although when building
 * internally we consistently refer to the system headers to avoid
 * conflicts we only expose the minimal set of GLES 2.0 types and enums
 * publicly.
 */
#ifdef CG_COMPILATION
#include "cg-gl-header.h"
#else
#include <cglib/cg-gles2-types.h>
#endif

CG_BEGIN_DECLS

/**
 * SECTION:cg-gles2
 * @short_description: A portable api to access OpenGLES 2.0
 *
 * CGlib provides portable access to the OpenGLES api through a single
 * library that is able to smooth over inconsistencies between the
 * different vendor drivers for OpenGLES in a single place.
 *
 * The api is designed to allow CGlib to transparently implement the
 * api on top of other drivers, such as OpenGL, D3D or on CGlib's own
 * drawing api so even if your platform doesn't come with an
 * OpenGLES 2.0 api CGlib may still be able to expose the api to your
 * application.
 *
 * Since CGlib is a library and not an api specification it is possible
 * to add OpenGLES 2.0 api features to CGlib which can immidiately
 * benefit developers regardless of what platform they are running on.
 *
 * With this api it's possible to re-use existing OpenGLES 2.0 code
 * within applications that are rendering with the CGlib API and also
 * it's possible for applications that render using OpenGLES 2.0 to
 * incorporate content rendered with CGlib.
 *
 * Applications can check for OpenGLES 2.0 api support by checking for
 * %CG_FEATURE_ID_GLES2_CONTEXT support with cg_has_feature().
 *
 * Stability: unstable
 */

/**
 * cg_gles2_context_t:
 *
 * Represents an OpenGLES 2.0 api context used as a sandbox for
 * OpenGLES 2.0 state. This is comparable to an EGLContext for those
 * who have used OpenGLES 2.0 with EGL before.
 *
 * Stability: unstable
 */
typedef struct _cg_gles2_context_t cg_gles2_context_t;

/**
 * cg_gles2_vtable_t:
 *
 * Provides function pointers for the full OpenGLES 2.0 api. The
 * api must be accessed this way and not by directly calling
 * symbols of any system OpenGLES 2.0 api.
 *
 * Stability: unstable
 */
typedef struct _cg_gles2_vtable_t cg_gles2_vtable_t;

struct _cg_gles2_vtable_t {
/*< private >*/
#define CG_EXT_BEGIN(name,                                                     \
                     min_gl_major,                                             \
                     min_gl_minor,                                             \
                     gles_availability,                                        \
                     extension_suffixes,                                       \
                     extension_names)

#define CG_EXT_FUNCTION(ret, name, args) ret(*name) args;

#define CG_EXT_END()

#include <cglib/gl-prototypes/cg-gles2-functions.h>

#undef CG_EXT_BEGIN
#undef CG_EXT_FUNCTION
#undef CG_EXT_END
};

uint32_t _cg_gles2_context_error_domain(void);

/**
 * CG_GLES2_CONTEXT_ERROR:
 *
 * An error domain for runtime exceptions relating to the
 * cg_gles2_context api.
 *
 * Stability: unstable
 */
#define CG_GLES2_CONTEXT_ERROR (_cg_gles2_context_error_domain())

/**
 * cg_gles2_context_error_t:
 * @CG_GLES2_CONTEXT_ERROR_UNSUPPORTED: Creating GLES2 contexts
 *    isn't supported. Applications should use cg_has_feature() to
 *    check for the %CG_FEATURE_ID_GLES2_CONTEXT.
 * @CG_GLES2_CONTEXT_ERROR_DRIVER: An underlying driver error
 *    occured.
 *
 * Error codes that relate to the cg_gles2_context api.
 */
typedef enum { /*< prefix=CG_GLES2_CONTEXT_ERROR >*/
    CG_GLES2_CONTEXT_ERROR_UNSUPPORTED,
    CG_GLES2_CONTEXT_ERROR_DRIVER
} cg_gles2_context_error_t;

/**
 * cg_gles2_context_new:
 * @dev: A #cg_device_t
 * @error: A pointer to a #cg_error_t for returning exceptions
 *
 * Allocates a new OpenGLES 2.0 context that can be used to render to
 * #cg_offscreen_t framebuffers (Rendering to #cg_onscreen_t
 * framebuffers is not currently supported).
 *
 * To actually access the OpenGLES 2.0 api itself you need to use
 * cg_gles2_context_get_vtable(). You should not try to directly link
 * to and use the symbols provided by the a system OpenGLES 2.0
 * driver.
 *
 * Once you have allocated an OpenGLES 2.0 context you can make it
 * current using cg_push_gles2_context(). For those familiar with
 * using the EGL api, this serves a similar purpose to eglMakeCurrent.
 *
 * <note>Before using this api applications can check for OpenGLES 2.0
 * api support by checking for %CG_FEATURE_ID_GLES2_CONTEXT support
 * with cg_has_feature(). This function will return %false and
 * return an %CG_GLES2_CONTEXT_ERROR_UNSUPPORTED error if the
 * feature isn't available.</note>
 *
 * Return value: A newly allocated #cg_gles2_context_t or %NULL if there
 *               was an error and @error will be updated in that case.
 * Stability: unstable
 */
cg_gles2_context_t *cg_gles2_context_new(cg_device_t *dev, cg_error_t **error);

/**
 * cg_gles2_context_get_vtable:
 * @gles2_ctx: A #cg_gles2_context_t allocated with
 *             cg_gles2_context_new()
 *
 * Queries the OpenGLES 2.0 api function pointers that should be
 * used for rendering with the given @gles2_ctx.
 *
 * <note>You should not try to directly link to and use the symbols
 * provided by any system OpenGLES 2.0 driver.</note>
 *
 * Return value: A pointer to a #cg_gles2_vtable_t providing pointers
 *               to functions for the full OpenGLES 2.0 api.
 * Stability: unstable
 */
const cg_gles2_vtable_t *
cg_gles2_context_get_vtable(cg_gles2_context_t *gles2_ctx);

/**
 * cg_push_gles2_context:
 * @dev: A #cg_device_t
 * @gles2_ctx: A #cg_gles2_context_t allocated with
 *             cg_gles2_context_new()
 * @read_buffer: A #cg_framebuffer_t to access to read operations
 *               such as glReadPixels. (must be a #cg_offscreen_t
 *               framebuffer currently)
 * @write_buffer: A #cg_framebuffer_t to access for drawing operations
 *                such as glDrawArrays. (must be a #cg_offscreen_t
 *               framebuffer currently)
 * @error: A pointer to a #cg_error_t for returning exceptions
 *
 * Pushes the given @gles2_ctx onto a stack associated with @ctx so
 * that the OpenGLES 2.0 api can be used instead of the CGlib
 * rendering apis to read and write to the specified framebuffers.
 *
 * Usage of the api available through a #cg_gles2_vtable_t is only
 * allowed between cg_push_gles2_context() and
 * cg_pop_gles2_context() calls.
 *
 * If there is a runtime problem with switching over to the given
 * @gles2_ctx then this function will return %false and return
 * an error through @error.
 *
 * Return value: %true if operation was successfull or %false
 *               otherwise and @error will be updated.
 * Stability: unstable
 */
bool cg_push_gles2_context(cg_device_t *dev,
                           cg_gles2_context_t *gles2_ctx,
                           cg_framebuffer_t *read_buffer,
                           cg_framebuffer_t *write_buffer,
                           cg_error_t **error);

/**
 * cg_pop_gles2_context:
 * @dev: A #cg_device_t
 *
 * Restores the previously active #cg_gles2_context_t if there
 * were nested calls to cg_push_gles2_context() or otherwise
 * restores the ability to render with the CGlib api instead
 * of OpenGLES 2.0.
 *
 * The behaviour is undefined if calls to cg_pop_gles2_context()
 * are not balenced with the number of corresponding calls to
 * cg_push_gles2_context().
 *
 * Stability: unstable
 */
void cg_pop_gles2_context(cg_device_t *dev);

/**
 * cg_gles2_get_current_vtable:
 *
 * Returns the OpenGL ES 2.0 api vtable for the currently pushed
 * #cg_gles2_context_t (last pushed with cg_push_gles2_context()) or
 * %NULL if no #cg_gles2_context_t has been pushed.
 *
 * Return value: The #cg_gles2_vtable_t for the currently pushed
 *               #cg_gles2_context_t or %NULL if none has been pushed.
 * Stability: unstable
 */
cg_gles2_vtable_t *cg_gles2_get_current_vtable(void);

/**
 * cg_gles2_texture_2d_new_from_handle:
 * @dev: A #cg_device_t
 * @gles2_ctx: A #cg_gles2_context_t allocated with
 *             cg_gles2_context_new()
 * @handle: An OpenGL ES 2.0 texture handle created with
 *          glGenTextures()
 * @width: Width of the texture to allocate
 * @height: Height of the texture to allocate
 * @format: The format of the texture
 *
 * Creates a #cg_texture_2d_t from an OpenGL ES 2.0 texture handle that
 * was created within the given @gles2_ctx via glGenTextures(). The
 * texture needs to have been associated with the GL_TEXTURE_2D target.
 *
 * <note>This interface is only intended for sharing textures to read
 * from.  The behaviour is undefined if the texture is modified using
 * the CGlib api.</note>
 *
 * <note>Applications should only pass this function handles that were
 * created via a #cg_gles2_vtable_t or via libcg-gles2 and not pass
 * handles created directly using the system's native libGLESv2
 * api.</note>
 *
 * Stability: unstable
 */
cg_texture_2d_t *
cg_gles2_texture_2d_new_from_handle(cg_device_t *dev,
                                    cg_gles2_context_t *gles2_ctx,
                                    unsigned int handle,
                                    int width,
                                    int height,
                                    cg_pixel_format_t format);

/**
 * cg_gles2_texture_get_handle:
 * @texture: A #cg_texture_t
 * @handle: A return location for an OpenGL ES 2.0 texture handle
 * @target: A return location for an OpenGL ES 2.0 texture target
 *
 * Gets an OpenGL ES 2.0 texture handle for a #cg_texture_t that can
 * then be referenced by a #cg_gles2_context_t. As well as returning
 * a texture handle the texture's target (such as GL_TEXTURE_2D) is
 * also returned.
 *
 * If the #cg_texture_t can not be shared with a #cg_gles2_context_t then
 * this function will return %false.
 *
 * This api does not affect the lifetime of the cg_texture_t and you
 * must take care not to reference the returned handle after the
 * original texture has been freed.
 *
 * <note>This interface is only intended for sharing textures to read
 * from.  The behaviour is undefined if the texture is modified by a
 * GLES2 context.</note>
 *
 * <note>This function will only return %true for low-level
 * #cg_texture_t<!-- -->s such as #cg_texture_2d_t or #cg_texture_3d_t but
 * not for high level meta textures such as
 * #cg_texture_2d_sliced_t</note>
 *
 * <note>The handle returned should not be passed directly to a system
 * OpenGL ES 2.0 library, the handle is only intended to be used via
 * a #cg_gles2_vtable_t or via libcg-gles2.</note>
 *
 * Return value: %true if a handle and target could be returned
 *               otherwise %false is returned.
 * Stability: unstable
 */
bool cg_gles2_texture_get_handle(cg_texture_t *texture,
                                 unsigned int *handle,
                                 unsigned int *target);

/**
 * cg_is_gles2_context:
 * @object: A #cg_object_t pointer
 *
 * Gets whether the given object references a #cg_gles2_context_t.
 *
 * Return value: %true if the object references a #cg_gles2_context_t
 *   and %false otherwise.
 * Stability: unstable
 */
bool cg_is_gles2_context(void *object);

CG_END_DECLS

#ifndef CG_COMPILATION
#undef __CG_H_INSIDE__
#endif

#endif /* __CG_GLES2_H__ */
