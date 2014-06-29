/*
 * Cogl
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
 * Authors:
 *  Robert Bragg <robert@linux.intel.com>
 *
 */

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __CG_CONTEXT_H__
#define __CG_CONTEXT_H__

/* We forward declare the cg_context_t type here to avoid some circular
 * dependency issues with the following headers.
 */
typedef struct _cg_context_t cg_context_t;

#include <cogl/cogl-defines.h>
#include <cogl/cogl-display.h>
#include <cogl/cogl-primitive.h>
#ifdef CG_HAS_EGL_PLATFORM_ANDROID_SUPPORT
#include <android/native_window.h>
#endif

CG_BEGIN_DECLS

/**
 * SECTION:cogl-context
 * @short_description: The top level application context.
 *
 * A #cg_context_t is the top most sandbox of Cogl state for an
 * application or toolkit. Its main purpose is to act as a sandbox
 * for the memory management of state objects. Normally an application
 * will only create a single context since there is no way to share
 * resources between contexts.
 *
 * For those familiar with OpenGL or perhaps Cairo it should be
 * understood that unlike these APIs a Cogl context isn't a rendering
 * context as such. In other words Cogl doesn't aim to provide a state
 * machine style model for configuring rendering parameters. Most
 * rendering state in Cogl is directly associated with user managed
 * objects called pipelines and geometry is drawn with a specific
 * pipeline object to a framebuffer object and those 3 things fully
 * define the state for drawing. This is an important part of Cogl's
 * design since it helps you write orthogonal rendering components
 * that can all access the same GPU without having to worry about
 * what state other components have left you with.
 *
 * <note><para>Cogl does not maintain internal references to the context for
 * resources that depend on the context so applications. This is to
 * help applications control the lifetime a context without us needing to
 * introduce special api to handle the breakup of internal circular
 * references due to internal resources and caches associated with the
 * context.
 *
 * One a context has been destroyed then all directly or indirectly
 * dependant resources will be in an inconsistent state and should not
 * be manipulated or queried in any way.
 *
 * For applications that rely on the operating system to clean up
 * resources this policy shouldn't affect them, but for applications
 * that need to carefully destroy and re-create Cogl contexts multiple
 * times throughout their lifetime (such as Android applications) they
 * should be careful to destroy all context dependant resources, such as
 * framebuffers or textures etc before unrefing and destroying the
 * context.</para></note>
 */

#define CG_CONTEXT(OBJECT) ((cg_context_t *)OBJECT)

/**
 * cg_context_new: (constructor)
 * @display: (allow-none): A #cg_display_t pointer
 * @error: A cg_error_t return location.
 *
 * Creates a new #cg_context_t which acts as an application sandbox
 * for any state objects that are allocated.
 *
 * Return value: (transfer full): A newly allocated #cg_context_t
 * Stability: unstable
 */
cg_context_t *cg_context_new(cg_display_t *display, cg_error_t **error);

/**
 * cg_context_get_display:
 * @context: A #cg_context_t pointer
 *
 * Retrieves the #cg_display_t that is internally associated with the
 * given @context. This will return the same #cg_display_t that was
 * passed to cg_context_new() or if %NULL was passed to
 * cg_context_new() then this function returns a pointer to the
 * display that was automatically setup internally.
 *
 * Return value: (transfer none): The #cg_display_t associated with the
 *               given @context.
 * Stability: unstable
 */
cg_display_t *cg_context_get_display(cg_context_t *context);

/**
 * cg_context_get_renderer:
 * @context: A #cg_context_t pointer
 *
 * Retrieves the #cg_renderer_t that is internally associated with the
 * given @context. This will return the same #cg_renderer_t that was
 * passed to cg_display_new() or if %NULL was passed to
 * cg_display_new() or cg_context_new() then this function returns
 * a pointer to the renderer that was automatically connected
 * internally.
 *
 * Return value: (transfer none): The #cg_renderer_t associated with the
 *               given @context.
 * Stability: unstable
 */
cg_renderer_t *cg_context_get_renderer(cg_context_t *context);

#ifdef CG_HAS_EGL_PLATFORM_ANDROID_SUPPORT
/**
 * cg_android_set_native_window:
 * @window: A native Android window
 *
 * Allows Android applications to inform Cogl of the native window
 * that they have been given which Cogl can render too. On Android
 * this API must be used before creating a #cg_renderer_t, #cg_display_t
 * and #cg_context_t.
 *
 * Stability: unstable
 */
void cg_android_set_native_window(ANativeWindow *window);
#endif

/**
 * cg_is_context:
 * @object: An object or %NULL
 *
 * Gets whether the given object references an existing context object.
 *
 * Return value: %true if the @object references a #cg_context_t,
 *   %false otherwise
 *
 * Stability: Unstable
 */
bool cg_is_context(void *object);

/* XXX: not guarded by the EXPERIMENTAL_API defines to avoid
 * upsetting glib-mkenums, but this can still be considered implicitly
 * experimental since it's only useable with experimental API... */
/**
 * cg_feature_id_t:
 * @CG_FEATURE_ID_TEXTURE_NPOT_BASIC: The hardware supports non power
 *     of two textures, but you also need to check the
 *     %CG_FEATURE_ID_TEXTURE_NPOT_MIPMAP and %CG_FEATURE_ID_TEXTURE_NPOT_REPEAT
 *     features to know if the hardware supports npot texture mipmaps
 *     or repeat modes other than
 *     %CG_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE respectively.
 * @CG_FEATURE_ID_TEXTURE_NPOT_MIPMAP: Mipmapping is supported in
 *     conjuntion with non power of two textures.
 * @CG_FEATURE_ID_TEXTURE_NPOT_REPEAT: Repeat modes other than
 *     %CG_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE are supported by the
 *     hardware.
 * @CG_FEATURE_ID_TEXTURE_NPOT: Non power of two textures are supported
 *    by the hardware. This is a equivalent to the
 *    %CG_FEATURE_ID_TEXTURE_NPOT_BASIC, %CG_FEATURE_ID_TEXTURE_NPOT_MIPMAP
 *    and %CG_FEATURE_ID_TEXTURE_NPOT_REPEAT features combined.
 * @CG_FEATURE_ID_TEXTURE_RG: Support for
 *    %CG_TEXTURE_COMPONENTS_RG as the internal components of a
 *    texture.
 * @CG_FEATURE_ID_TEXTURE_3D: 3D texture support
 * @CG_FEATURE_ID_OFFSCREEN: Offscreen rendering support
 * @CG_FEATURE_ID_OFFSCREEN_MULTISAMPLE: Multisample support for
 *    offscreen framebuffers
 * @CG_FEATURE_ID_ONSCREEN_MULTIPLE: Multiple onscreen framebuffers
 *    supported.
 * @CG_FEATURE_ID_GLSL: GLSL support
 * @CG_FEATURE_ID_UNSIGNED_INT_INDICES: Set if
 *     %CG_INDICES_TYPE_UNSIGNED_INT is supported in
 *     cg_indices_new().
 * @CG_FEATURE_ID_DEPTH_RANGE: cg_pipeline_set_depth_range() support
 * @CG_FEATURE_ID_POINT_SPRITE: Whether
 *     cg_pipeline_set_layer_point_sprite_coords_enabled() is supported.
 * @CG_FEATURE_ID_PER_VERTEX_POINT_SIZE: Whether cg_point_size_in
 *     can be used as an attribute to set a per-vertex point size.
 * @CG_FEATURE_ID_MAP_BUFFER_FOR_READ: Whether cg_buffer_map() is
 *     supported with cg_buffer_access_t including read support.
 * @CG_FEATURE_ID_MAP_BUFFER_FOR_WRITE: Whether cg_buffer_map() is
 *     supported with cg_buffer_access_t including write support.
 * @CG_FEATURE_ID_MIRRORED_REPEAT: Whether
 *    %CG_PIPELINE_WRAP_MODE_MIRRORED_REPEAT is supported.
 * @CG_FEATURE_ID_GLES2_CONTEXT: Whether creating new GLES2 contexts is
 *    suported.
 * @CG_FEATURE_ID_DEPTH_TEXTURE: Whether #cg_framebuffer_t support rendering
 *     the depth buffer to a texture.
 * @CG_FEATURE_ID_PRESENTATION_TIME: Whether frame presentation
 *    time stamps will be recorded in #cg_frame_info_t objects.
 *
 * All the capabilities that can vary between different GPUs supported
 * by Cogl. Applications that depend on any of these features should explicitly
 * check for them using cg_has_feature() or cg_has_features().
 *
 */
typedef enum _cg_feature_id_t {
    CG_FEATURE_ID_TEXTURE_NPOT_BASIC = 1,
    CG_FEATURE_ID_TEXTURE_NPOT_MIPMAP,
    CG_FEATURE_ID_TEXTURE_NPOT_REPEAT,
    CG_FEATURE_ID_TEXTURE_NPOT,
    CG_FEATURE_ID_TEXTURE_3D,
    CG_FEATURE_ID_GLSL,
    CG_FEATURE_ID_OFFSCREEN,
    CG_FEATURE_ID_OFFSCREEN_MULTISAMPLE,
    CG_FEATURE_ID_ONSCREEN_MULTIPLE,
    CG_FEATURE_ID_UNSIGNED_INT_INDICES,
    CG_FEATURE_ID_DEPTH_RANGE,
    CG_FEATURE_ID_POINT_SPRITE,
    CG_FEATURE_ID_MAP_BUFFER_FOR_READ,
    CG_FEATURE_ID_MAP_BUFFER_FOR_WRITE,
    CG_FEATURE_ID_MIRRORED_REPEAT,
    CG_FEATURE_ID_GLES2_CONTEXT,
    CG_FEATURE_ID_DEPTH_TEXTURE,
    CG_FEATURE_ID_PRESENTATION_TIME,
    CG_FEATURE_ID_FENCE,
    CG_FEATURE_ID_PER_VERTEX_POINT_SIZE,
    CG_FEATURE_ID_TEXTURE_RG,

    /*< private >*/
    _CG_N_FEATURE_IDS /*< skip >*/
} cg_feature_id_t;

/**
 * cg_has_feature:
 * @context: A #cg_context_t pointer
 * @feature: A #cg_feature_id_t
 *
 * Checks if a given @feature is currently available
 *
 * Cogl does not aim to be a lowest common denominator API, it aims to
 * expose all the interesting features of GPUs to application which
 * means applications have some responsibility to explicitly check
 * that certain features are available before depending on them.
 *
 * Returns: %true if the @feature is currently supported or %false if
 * not.
 *
 * Stability: unstable
 */
bool cg_has_feature(cg_context_t *context, cg_feature_id_t feature);

/**
 * cg_has_features:
 * @context: A #cg_context_t pointer
 * @...: A 0 terminated list of cg_feature_id_t<!-- -->s
 *
 * Checks if a list of features are all currently available.
 *
 * This checks all of the listed features using cg_has_feature() and
 * returns %true if all the features are available or %false
 * otherwise.
 *
 * Return value: %true if all the features are available, %false
 * otherwise.
 *
 * Stability: unstable
 */
bool cg_has_features(cg_context_t *context, ...);

/**
 * cg_feature_callback_t:
 * @feature: A single feature currently supported by Cogl
 * @user_data: A private pointer passed to cg_foreach_feature().
 *
 * A callback used with cg_foreach_feature() for enumerating all
 * context level features supported by Cogl.
 *
 * Stability: unstable
 */
typedef void (*cg_feature_callback_t)(cg_feature_id_t feature, void *user_data);

/**
 * cg_foreach_feature:
 * @context: A #cg_context_t pointer
 * @callback: (scope call): A #cg_feature_callback_t called for each
 *            supported feature
 * @user_data: (closure): Private data to pass to the callback
 *
 * Iterates through all the context level features currently supported
 * for a given @context and for each feature @callback is called.
 *
 * Stability: unstable
 */
void cg_foreach_feature(cg_context_t *context,
                        cg_feature_callback_t callback,
                        void *user_data);

/**
 * cg_get_clock_time:
 * @context: a #cg_context_t pointer
 *
 * Returns the current time value from Cogl's internal clock. This
 * clock is used for measuring times such as the presentation time
 * in a #cg_frame_info_t.
 *
 * This method is meant for converting timestamps retrieved from Cogl
 * to other time systems, and is not meant to be used as a standalone
 * timing system. For that reason, if this function is called without
 * having retrieved a valid (non-zero) timestamp from Cogl first, it
 * may return 0 to indicate that Cogl has no active internal clock.
 *
 * Return value: the time value for the Cogl clock, in nanoseconds
 *  from an arbitrary point in time, or 0 if Cogl doesn't have an
 *  active internal clock.
 * Stability: unstable
 */
int64_t cg_get_clock_time(cg_context_t *context);

CG_END_DECLS

#endif /* __CG_CONTEXT_H__ */
