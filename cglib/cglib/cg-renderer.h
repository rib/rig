/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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

#ifndef __CG_RENDERER_H__
#define __CG_RENDERER_H__

#include <cglib/cg-types.h>
#include <cglib/cg-onscreen-template.h>
#include <cglib/cg-error.h>
#include <cglib/cg-output.h>

CG_BEGIN_DECLS

/**
 * SECTION:cg-renderer
 * @short_description: Choosing a means to render
 *
 * A #cg_renderer_t represents a means to render. It encapsulates the
 * selection of an underlying driver, such as OpenGL or OpenGL-ES and
 * a selection of a window system binding API such as GLX, or EGL or
 * WGL.
 *
 * A #cg_renderer_t has two states, "unconnected" and "connected". When
 * a renderer is first instantiated using cg_renderer_new() it is
 * unconnected so that it can be configured and constraints can be
 * specified for how the backend driver and window system should be
 * chosen.
 *
 * After configuration a #cg_renderer_t can (optionally) be explicitly
 * connected using cg_renderer_connect() which allows for the
 * handling of connection errors so that fallback configurations can
 * be tried if necessary. Applications that don't support any
 * fallbacks though can skip using cg_renderer_connect() and leave
 * CGlib to automatically connect the renderer.
 *
 * Once you have a configured #cg_renderer_t it can be used to create a
 * #cg_display_t object using cg_display_new().
 *
 * <note>Many applications don't need to explicitly use
 * cg_renderer_new() or cg_display_new() and can just jump
 * straight to cg_device_new() and pass a %NULL display argument so
 * CGlib will automatically connect and setup a renderer and
 * display.</note>
 */

/**
 * CG_RENDERER_ERROR:
 *
 * An error domain for exceptions reported by CGlib
 */
#define CG_RENDERER_ERROR cg_renderer_error_domain()

uint32_t cg_renderer_error_domain(void);

typedef struct _cg_renderer_t cg_renderer_t;

/**
 * cg_is_renderer:
 * @object: A #cg_object_t pointer
 *
 * Determines if the given @object is a #cg_renderer_t
 *
 * Return value: %true if @object is a #cg_renderer_t, else %false.
 * Stability: unstable
 */
bool cg_is_renderer(void *object);

/**
 * cg_renderer_new:
 *
 * Instantiates a new (unconnected) #cg_renderer_t object. A
 * #cg_renderer_t represents a means to render. It encapsulates the
 * selection of an underlying driver, such as OpenGL or OpenGL-ES and
 * a selection of a window system binding API such as GLX, or EGL or
 * WGL.
 *
 * While the renderer is unconnected it can be configured so that
 * applications may specify backend constraints, such as "must use
 * x11" for example via cg_renderer_add_constraint().
 *
 * There are also some platform specific configuration apis such
 * as cg_xlib_renderer_set_foreign_display() that may also be
 * used while the renderer is unconnected.
 *
 * Once the renderer has been configured, then it may (optionally) be
 * explicitly connected using cg_renderer_connect() which allows
 * errors to be handled gracefully and potentially fallback
 * configurations can be tried out if there are initial failures.
 *
 * If a renderer is not explicitly connected then cg_display_new()
 * will automatically connect the renderer for you. If you don't
 * have any code to deal with error/fallback situations then its fine
 * to just let CGlib do the connection for you.
 *
 * Once you have setup your renderer then the next step is to create a
 * #cg_display_t using cg_display_new().
 *
 * <note>Many applications don't need to explicitly use
 * cg_renderer_new() or cg_display_new() and can just jump
 * straight to cg_device_new() and pass a %NULL display argument
 * so CGlib will automatically connect and setup a renderer and
 * display.</note>
 *
 * Return value: (transfer full): A newly created #cg_renderer_t.
 *
 * Stability: unstable
 */
cg_renderer_t *cg_renderer_new(void);

/* optional configuration APIs */

/**
 * cg_winsys_id_t:
 * @CG_WINSYS_ID_ANY: Implies no preference for which backend is used
 * @CG_WINSYS_ID_STUB: Use the no-op stub backend
 * @CG_WINSYS_ID_GLX: Use the GLX window system binding API
 * @CG_WINSYS_ID_EGL_XLIB: Use EGL with the X window system via XLib
 * @CG_WINSYS_ID_EGL_NULL: Use EGL with the PowerVR NULL window system
 * @CG_WINSYS_ID_EGL_WAYLAND: Use EGL with the Wayland window system
 * @CG_WINSYS_ID_EGL_KMS: Use EGL with the KMS platform
 * @CG_WINSYS_ID_EGL_ANDROID: Use EGL with the Android platform
 * @CG_WINSYS_ID_WGL: Use the Microsoft Windows WGL binding API
 * @CG_WINSYS_ID_SDL: Use the SDL window system
 * @CG_WINSYS_ID_WEBGL: Use the Emscripten WebGL API
 *
 * Identifies specific window system backends that CGlib supports.
 *
 * These can be used to query what backend CGlib is using or to try and
 * explicitly select a backend to use.
 */
typedef enum {
    CG_WINSYS_ID_ANY,
    CG_WINSYS_ID_STUB,
    CG_WINSYS_ID_GLX,
    CG_WINSYS_ID_EGL_XLIB,
    CG_WINSYS_ID_EGL_NULL,
    CG_WINSYS_ID_EGL_WAYLAND,
    CG_WINSYS_ID_EGL_KMS,
    CG_WINSYS_ID_EGL_ANDROID,
    CG_WINSYS_ID_WGL,
    CG_WINSYS_ID_SDL,
    CG_WINSYS_ID_WEBGL
} cg_winsys_id_t;

/**
 * cg_renderer_set_winsys_id:
 * @renderer: A #cg_renderer_t
 * @winsys_id: An ID of the winsys you explicitly want to use.
 *
 * This allows you to explicitly select a winsys backend to use instead
 * of letting CGlib automatically select a backend.
 *
 * if you select an unsupported backend then cg_renderer_connect()
 * will fail and report an error.
 *
 * This may only be called on an un-connected #cg_renderer_t.
 */
void cg_renderer_set_winsys_id(cg_renderer_t *renderer,
                               cg_winsys_id_t winsys_id);

/**
 * cg_renderer_get_winsys_id:
 * @renderer: A #cg_renderer_t
 *
 * Queries which window system backend CGlib has chosen to use.
 *
 * This may only be called on a connected #cg_renderer_t.
 *
 * Returns: The #cg_winsys_id_t corresponding to the chosen window
 *          system backend.
 */
cg_winsys_id_t cg_renderer_get_winsys_id(cg_renderer_t *renderer);

/**
 * cg_renderer_get_n_fragment_texture_units:
 * @renderer: A #cg_renderer_t
 *
 * Queries how many texture units can be used from fragment programs
 *
 * Returns: the number of texture image units.
 *
 * Stability: Unstable
 */
int cg_renderer_get_n_fragment_texture_units(cg_renderer_t *renderer);

/**
 * cg_renderer_check_onscreen_template:
 * @renderer: A #cg_renderer_t
 * @onscreen_template: A #cg_onscreen_template_t
 * @error: A pointer to a #cg_error_t for reporting exceptions
 *
 * Tests if a given @onscreen_template can be supported with the given
 * @renderer.
 *
 * Return value: %true if the @onscreen_template can be supported,
 *               else %false.
 * Stability: unstable
 */
bool
cg_renderer_check_onscreen_template(cg_renderer_t *renderer,
                                    cg_onscreen_template_t *onscreen_template,
                                    cg_error_t **error);

/* Final connection API */

/**
 * cg_renderer_connect:
 * @renderer: An unconnected #cg_renderer_t
 * @error: a pointer to a #cg_error_t for reporting exceptions
 *
 * Connects the configured @renderer. Renderer connection isn't a
 * very active process, it basically just means validating that
 * any given constraint criteria can be satisfied and that a
 * usable driver and window system backend can be found.
 *
 * Return value: %true if there was no error while connecting the
 *               given @renderer. %false if there was an error.
 * Stability: unstable
 */
bool cg_renderer_connect(cg_renderer_t *renderer, cg_error_t **error);

/**
 * cg_renderer_constraint_t:
 * @CG_RENDERER_CONSTRAINT_USES_X11: Require the renderer to be X11 based
 * @CG_RENDERER_CONSTRAINT_USES_XLIB: Require the renderer to be X11
 *                                      based and use Xlib
 * @CG_RENDERER_CONSTRAINT_USES_EGL: Require the renderer to be EGL based
 * @CG_RENDERER_CONSTRAINT_SUPPORTS_CG_GLES2: Require that the
 *    renderer supports creating a #cg_gles2_context_t via
 *    cg_gles2_context_new(). This can be used to integrate GLES 2.0
 *    code into CGlib based applications.
 *
 * These constraint flags are hard-coded features of the different renderer
 * backends. Sometimes a platform may support multiple rendering options which
 * CGlib will usually choose from automatically. Some of these features are
 * important to higher level applications and frameworks though, such as
 * whether a renderer is X11 based because an application might only support
 * X11 based input handling. An application might also need to ensure EGL is
 * used internally too if they depend on access to an EGLDisplay for some
 * purpose.
 *
 * Applications should ideally minimize how many of these constraints
 * they depend on to ensure maximum portability.
 *
 * Stability: unstable
 */
typedef enum {
    CG_RENDERER_CONSTRAINT_USES_X11 = (1 << 0),
    CG_RENDERER_CONSTRAINT_USES_XLIB = (1 << 1),
    CG_RENDERER_CONSTRAINT_USES_EGL = (1 << 2),
    CG_RENDERER_CONSTRAINT_SUPPORTS_CG_GLES2 = (1 << 3)
} cg_renderer_constraint_t;

/**
 * cg_renderer_add_constraint:
 * @renderer: An unconnected #cg_renderer_t
 * @constraint: A #cg_renderer_constraint_t to add
 *
 * This adds a renderer selection @constraint.
 *
 * Applications should ideally minimize how many of these constraints they
 * depend on to ensure maximum portability.
 *
 * Stability: unstable
 */
void cg_renderer_add_constraint(cg_renderer_t *renderer,
                                cg_renderer_constraint_t constraint);

/**
 * cg_renderer_remove_constraint:
 * @renderer: An unconnected #cg_renderer_t
 * @constraint: A #cg_renderer_constraint_t to remove
 *
 * This removes a renderer selection @constraint.
 *
 * Applications should ideally minimize how many of these constraints they
 * depend on to ensure maximum portability.
 *
 * Stability: unstable
 */
void cg_renderer_remove_constraint(cg_renderer_t *renderer,
                                   cg_renderer_constraint_t constraint);

/**
 * cg_driver_t:
 * @CG_DRIVER_ANY: Implies no preference for which driver is used
 * @CG_DRIVER_NOP: A No-Op driver.
 * @CG_DRIVER_GL: An OpenGL driver.
 * @CG_DRIVER_GL3: An OpenGL driver using the core GL 3.1 profile
 * @CG_DRIVER_GLES2: An OpenGL ES 2.0 driver.
 * @CG_DRIVER_WEBGL: A WebGL driver.
 *
 * Identifiers for underlying hardware drivers that may be used by
 * CGlib for rendering.
 *
 * Stability: unstable
 */
typedef enum {
    CG_DRIVER_ANY,
    CG_DRIVER_NOP,
    CG_DRIVER_GL,
    CG_DRIVER_GL3,
    CG_DRIVER_GLES2,
    CG_DRIVER_WEBGL
} cg_driver_t;

/**
 * cg_renderer_set_driver:
 * @renderer: An unconnected #cg_renderer_t
 *
 * Requests that CGlib should try to use a specific underlying driver
 * for rendering.
 *
 * If you select an unsupported driver then cg_renderer_connect()
 * will fail and report an error. Most applications should not
 * explicitly select a driver and should rely on CGlib automatically
 * choosing the driver.
 *
 * This may only be called on an un-connected #cg_renderer_t.
 *
 * Stability: unstable
 */
void cg_renderer_set_driver(cg_renderer_t *renderer, cg_driver_t driver);

/**
 * cg_renderer_get_driver:
 * @renderer: A connected #cg_renderer_t
 *
 * Queries what underlying driver is being used by CGlib.
 *
 * This may only be called on a connected #cg_renderer_t.
 *
 * Stability: unstable
 */
cg_driver_t cg_renderer_get_driver(cg_renderer_t *renderer);

/**
 * cg_output_callback_t:
 * @output: The current display output being iterated
 * @user_data: The user pointer passed to
 *             cg_renderer_foreach_output()
 *
 * A callback type that can be passed to
 * cg_renderer_foreach_output() for iterating display outputs for a
 * given renderer.
 *
 * Stability: Unstable
 */
typedef void (*cg_output_callback_t)(cg_output_t *output, void *user_data);

/**
 * cg_renderer_foreach_output:
 * @renderer: A connected #cg_renderer_t
 * @callback: (scope call): A #cg_output_callback_t to be called for
 *            each display output
 * @user_data: A user pointer to be passed to @callback
 *
 * Iterates all known display outputs for the given @renderer and
 * passes a corresponding #cg_output_t pointer to the given @callback
 * for each one, along with the given @user_data.
 *
 * Stability: Unstable
 */
void cg_renderer_foreach_output(cg_renderer_t *renderer,
                                cg_output_callback_t callback,
                                void *user_data);

CG_END_DECLS

#endif /* __CG_RENDERER_H__ */
