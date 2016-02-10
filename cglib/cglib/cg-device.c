/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009,2013 Intel Corporation.
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

#include <cglib-config.h>

#include "cg-object.h"
#include "cg-private.h"
#include "cg-winsys-private.h"
#include "winsys/cg-winsys-stub-private.h"
#include "cg-profile.h"
#include "cg-util.h"
#include "cg-device-private.h"
#include "cg-util-gl-private.h"
#include "cg-display-private.h"
#include "cg-renderer-private.h"
#include "cg-texture-private.h"
#include "cg-texture-2d-private.h"
#include "cg-texture-3d-private.h"
#include "cg-atlas-set.h"
#include "cg-atlas-texture-private.h"
#include "cg-pipeline-private.h"
#include "cg-pipeline-opengl-private.h"
#include "cg-framebuffer-private.h"
#include "cg-onscreen-private.h"
#include "cg-attribute-private.h"
#include "cg-gpu-info-private.h"
#include "cg-config-private.h"
#include "cg-error-private.h"

#ifdef CG_HAS_UV_SUPPORT
#include "cg-uv-private.h"
#endif

#include <string.h>
#include <stdlib.h>

/* These aren't defined in the GLES headers */
#ifndef GL_POINT_SPRITE
#define GL_POINT_SPRITE 0x8861
#endif

#ifndef GL_NUM_EXTENSIONS
#define GL_NUM_EXTENSIONS 0x821D
#endif

static void _cg_device_free(cg_device_t *dev);

CG_OBJECT_DEFINE(Device, device);

extern void _cg_create_context_driver(cg_device_t *dev);

static cg_device_t *_cg_device = NULL;

static void
_cg_init_feature_overrides(cg_device_t *dev)
{
    if (C_UNLIKELY(CG_DEBUG_ENABLED(CG_DEBUG_DISABLE_VBOS)))
        CG_FLAGS_SET(dev->private_features, CG_PRIVATE_FEATURE_VBOS, false);

    if (C_UNLIKELY(CG_DEBUG_ENABLED(CG_DEBUG_DISABLE_PBOS)))
        CG_FLAGS_SET(dev->private_features, CG_PRIVATE_FEATURE_PBOS, false);

    if (C_UNLIKELY(CG_DEBUG_ENABLED(CG_DEBUG_DISABLE_GLSL))) {
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_GLSL, false);
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_PER_VERTEX_POINT_SIZE,
                     false);
    }

    if (C_UNLIKELY(CG_DEBUG_ENABLED(CG_DEBUG_DISABLE_NPOT_TEXTURES))) {
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_TEXTURE_NPOT, false);
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_TEXTURE_NPOT_BASIC, false);
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_TEXTURE_NPOT_MIPMAP, false);
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_TEXTURE_NPOT_REPEAT, false);
    }
}

const cg_winsys_vtable_t *
_cg_device_get_winsys(cg_device_t *dev)
{
    return dev->renderer->winsys_vtable;
}

/* For reference: There was some deliberation over whether to have a
 * constructor that could throw an exception but looking at standard
 * practices with several high level OO languages including python, C++,
 * C# Java and Ruby they all support exceptions in constructors and the
 * general consensus appears to be that throwing an exception is neater
 * than successfully constructing with an internal error status that
 * would then have to be explicitly checked via some form of ::is_ok()
 * method.
 */
cg_device_t *
cg_device_new(void)
{
    cg_device_t *dev;

    _cg_init();

#ifdef ENABLE_PROFILE
    /* We need to be absolutely sure that uprof has been initialized
     * before calling _cg_uprof_init. uprof_init (NULL, NULL)
     * will be a NOP if it has been initialized but it will also
     * mean subsequent parsing of the UProf GOptionGroup will have no
     * affect.
     *
     * Sadly GOptionGroup based library initialization is extremely
     * fragile by design because GOptionGroups have no notion of
     * dependencies and so the order things are initialized isn't
     * currently under tight control.
     */
    uprof_init(NULL, NULL);
    _cg_uprof_init();
#endif

    dev = c_malloc0(sizeof(cg_device_t));
    memset(dev, 0, sizeof(cg_device_t));

    /* Convert the context into an object immediately in case any of the
       code below wants to verify that the context pointer is a valid
       object */
    _cg_device_object_new(dev);

    /* TODO: remove final uses of _CG_GET_DEVICE() which depends on
     * having one globally accessible device pointer. */
    _cg_device = dev;

    /* Init default values */
    memset(dev->features, 0, sizeof(dev->features));
    memset(dev->private_features, 0, sizeof(dev->private_features));

    dev->rectangle_state = CG_WINSYS_RECTANGLE_STATE_UNKNOWN;

    memset(dev->winsys_features, 0, sizeof(dev->winsys_features));

    return dev;
}

void
cg_device_set_renderer(cg_device_t *dev, cg_renderer_t *renderer)
{
    if (renderer)
        cg_object_ref(renderer);

    if (dev->renderer)
        cg_object_unref(dev->renderer);

    dev->renderer = renderer;
}

void
cg_device_set_display(cg_device_t *dev, cg_display_t *display)
{
    if (display)
        cg_object_ref(display);

    if (dev->display)
        cg_object_unref(dev->display);

    dev->display = display;
}

bool
cg_device_connect(cg_device_t *dev, cg_error_t **error)
{
    uint8_t white_pixel[] = { 0xff, 0xff, 0xff, 0xff };
    const cg_winsys_vtable_t *winsys;
    int i;
    cg_error_t *internal_error = NULL;

    if (dev->connected)
        return true;

    /* Mark as connected now to avoid recursion issues,
     * but revert in error paths */
    dev->connected = true;

    if (!dev->renderer) {
        cg_renderer_t *renderer = cg_renderer_new();
        if (!cg_renderer_connect(renderer, error)) {
            cg_object_unref(renderer);
            dev->connected = false;
            return false;
        }
        cg_device_set_renderer(dev, renderer);
    }

    if (!dev->display) {
        cg_display_t *display = cg_display_new(dev->renderer, NULL);
        if (!cg_display_setup(display, error)) {
            cg_object_unref(display);
            dev->connected = false;
            return false;
        }
        cg_device_set_display(dev, display);
    }

    /* This is duplicated data, but it's much more convenient to have
       the driver attached to the context and the value is accessed a
       lot throughout CGlib */
    dev->driver = dev->renderer->driver;

    /* Again this is duplicated data, but it convenient to be able
     * access these from the context. */
    dev->driver_vtable = dev->renderer->driver_vtable;
    dev->texture_driver = dev->renderer->texture_driver;

    for (i = 0; i < C_N_ELEMENTS(dev->private_features); i++)
        dev->private_features[i] |= dev->renderer->private_features[i];

    winsys = _cg_device_get_winsys(dev);
    if (!winsys->device_init(dev, error)) {
        dev->connected = false;
        return false;
    }

    dev->attribute_name_states_hash =
        c_hash_table_new_full(c_str_hash, c_str_equal, c_free, c_free);
    dev->attribute_name_index_map = NULL;
    dev->n_attribute_names = 0;

    /* The "cg_color_in" attribute needs a deterministic name_index
     * so we make sure it's the first attribute name we register */
    _cg_attribute_register_attribute_name(dev, "cg_color_in");

    dev->uniform_names =
        c_ptr_array_new_with_free_func((c_destroy_func_t)c_free);
    dev->uniform_name_hash = c_hash_table_new(c_str_hash, c_str_equal);
    dev->n_uniform_names = 0;

    /* Initialise the driver specific state */
    _cg_init_feature_overrides(dev);

    /* XXX: ONGOING BUG: Intel viewport scissor
     *
     * Intel gen6 drivers don't currently correctly handle offset
     * viewports, since primitives aren't clipped within the bounds of
     * the viewport.  To workaround this we push our own clip for the
     * viewport that will use scissoring to ensure we clip as expected.
     *
     * TODO: file a bug upstream!
     */
    if (dev->gpu.driver_package == CG_GPU_INFO_DRIVER_PACKAGE_MESA &&
        dev->gpu.architecture == CG_GPU_INFO_ARCHITECTURE_SANDYBRIDGE &&
        !getenv("CG_DISABLE_INTEL_VIEWPORT_SCISSORT_WORKAROUND"))
        dev->needs_viewport_scissor_workaround = true;
    else
        dev->needs_viewport_scissor_workaround = false;

    dev->sampler_cache = _cg_sampler_cache_new(dev);

    _cg_pipeline_init_default_pipeline(dev);
    _cg_pipeline_init_default_layers(dev);
    _cg_pipeline_init_state_hash_functions();
    _cg_pipeline_init_layer_state_hash_functions();

    dev->current_clip_stack_valid = false;
    dev->current_clip_stack = NULL;

    c_matrix_init_identity(&dev->identity_matrix);
    c_matrix_init_identity(&dev->y_flip_matrix);
    c_matrix_scale(&dev->y_flip_matrix, 1, -1, 1);

    dev->texture_units =
        c_array_new(false, false, sizeof(cg_texture_unit_t));

    if (_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_ANY_GL)) {
        /* See cg-pipeline.c for more details about why we leave texture unit
         * 1
         * active by default... */
        dev->active_texture_unit = 1;
        GE(dev, glActiveTexture(GL_TEXTURE1));
    }

    dev->opaque_color_pipeline = cg_pipeline_new(dev);
    dev->codegen_header_buffer = c_string_new("");
    dev->codegen_source_buffer = c_string_new("");

    dev->default_gl_texture_2d_tex = NULL;
    dev->default_gl_texture_3d_tex = NULL;

    dev->framebuffers = NULL;
    dev->current_draw_buffer = NULL;
    dev->current_read_buffer = NULL;
    dev->current_draw_buffer_state_flushed = 0;
    dev->current_draw_buffer_changes = CG_FRAMEBUFFER_STATE_ALL;

    c_queue_init(&dev->gles2_context_stack);

    dev->current_pipeline = NULL;
    dev->current_pipeline_changes_since_flush = 0;
    dev->current_pipeline_with_color_attrib = false;

    _cg_bitmask_init(&dev->enabled_custom_attributes);
    _cg_bitmask_init(&dev->enable_custom_attributes_tmp);
    _cg_bitmask_init(&dev->changed_bits_tmp);

    dev->max_texture_units = -1;
    dev->max_activateable_texture_units = -1;

    dev->current_gl_program = 0;

    dev->current_gl_dither_enabled = true;
    dev->current_gl_color_mask = CG_COLOR_MASK_ALL;

    dev->gl_blend_enable_cache = false;

    dev->depth_test_enabled_cache = false;
    dev->depth_test_function_cache = CG_DEPTH_TEST_FUNCTION_LESS;
    dev->depth_writing_enabled_cache = true;
    dev->depth_range_near_cache = 0;
    dev->depth_range_far_cache = 1;

    dev->pipeline_cache = _cg_pipeline_cache_new(dev);

    for (i = 0; i < CG_BUFFER_BIND_TARGET_COUNT; i++)
        dev->current_buffer[i] = NULL;

    dev->stencil_pipeline = cg_pipeline_new(dev);

    dev->rectangle_byte_indices = NULL;
    dev->rectangle_short_indices = NULL;
    dev->rectangle_short_indices_len = 0;

    dev->texture_download_pipeline = NULL;
    dev->blit_texture_pipeline = NULL;

#if defined(CG_HAS_GL_SUPPORT)
    if ((dev->driver == CG_DRIVER_GL3)) {
        GLuint vertex_array;

        /* In a forward compatible context, GL 3 doesn't support rendering
         * using the default vertex array object. CGlib doesn't use vertex
         * array objects yet so for now we just create a dummy array
         * object that we will use as our own default object. Eventually
         * it could be good to attach the vertex array objects to
         * cg_primitive_ts */
        dev->glGenVertexArrays(1, &vertex_array);
        dev->glBindVertexArray(vertex_array);
    }
#endif

    dev->current_modelview_entry = NULL;
    dev->current_projection_entry = NULL;
    _cg_matrix_entry_identity_init(&dev->identity_entry);
    _cg_matrix_entry_cache_init(&dev->builtin_flushed_projection);
    _cg_matrix_entry_cache_init(&dev->builtin_flushed_modelview);

    /* Create default textures used for fall backs */
    dev->default_gl_texture_2d_tex =
        cg_texture_2d_new_from_data(dev,
                                    1,
                                    1,
                                    CG_PIXEL_FORMAT_RGBA_8888_PRE,
                                    0, /* rowstride */
                                    white_pixel,
                                    NULL); /* abort on error */

    /* If 3D or rectangle textures aren't supported then these will
     * return errors that we can simply ignore. */
    internal_error = NULL;
    dev->default_gl_texture_3d_tex =
        cg_texture_3d_new_from_data(dev,
                                    1,
                                    1,
                                    1, /* width, height, depth */
                                    CG_PIXEL_FORMAT_RGBA_8888_PRE,
                                    0, /* rowstride */
                                    0, /* image stride */
                                    white_pixel,
                                    &internal_error);
    if (internal_error)
        cg_error_free(internal_error);

    dev->buffer_map_fallback_array = c_byte_array_new();
    dev->buffer_map_fallback_in_use = false;

    c_list_init(&dev->fences);

    dev->atlas_set = cg_atlas_set_new(dev);
    cg_atlas_set_set_components(dev->atlas_set, CG_TEXTURE_COMPONENTS_RGBA);
    cg_atlas_set_set_premultiplied(dev->atlas_set, false);
    cg_atlas_set_add_atlas_callback(dev->atlas_set,
                                    _cg_atlas_texture_atlas_event_handler,
                                    NULL, /* user data */
                                    NULL); /* destroy */

    return true;
}

static void
_cg_device_free(cg_device_t *dev)
{
    const cg_winsys_vtable_t *winsys = _cg_device_get_winsys(dev);

    winsys->device_deinit(dev);

    if (dev->atlas_set)
        cg_object_unref(dev->atlas_set);

    if (dev->default_gl_texture_2d_tex)
        cg_object_unref(dev->default_gl_texture_2d_tex);
    if (dev->default_gl_texture_3d_tex)
        cg_object_unref(dev->default_gl_texture_3d_tex);

    if (dev->opaque_color_pipeline)
        cg_object_unref(dev->opaque_color_pipeline);

    if (dev->blit_texture_pipeline)
        cg_object_unref(dev->blit_texture_pipeline);

    c_warn_if_fail(dev->gles2_context_stack.length == 0);

    if (dev->rectangle_byte_indices)
        cg_object_unref(dev->rectangle_byte_indices);
    if (dev->rectangle_short_indices)
        cg_object_unref(dev->rectangle_short_indices);

    if (dev->default_pipeline)
        cg_object_unref(dev->default_pipeline);

    if (dev->dummy_layer_dependant)
        cg_object_unref(dev->dummy_layer_dependant);
    if (dev->default_layer_n)
        cg_object_unref(dev->default_layer_n);
    if (dev->default_layer_0)
        cg_object_unref(dev->default_layer_0);

    if (dev->current_clip_stack_valid)
        _cg_clip_stack_unref(dev->current_clip_stack);

    _cg_bitmask_destroy(&dev->enabled_custom_attributes);
    _cg_bitmask_destroy(&dev->enable_custom_attributes_tmp);
    _cg_bitmask_destroy(&dev->changed_bits_tmp);

    if (dev->current_modelview_entry)
        cg_matrix_entry_unref(dev->current_modelview_entry);
    if (dev->current_projection_entry)
        cg_matrix_entry_unref(dev->current_projection_entry);
    _cg_matrix_entry_cache_destroy(&dev->builtin_flushed_projection);
    _cg_matrix_entry_cache_destroy(&dev->builtin_flushed_modelview);

    _cg_pipeline_cache_free(dev->pipeline_cache);

    _cg_sampler_cache_free(dev->sampler_cache);

    _cg_destroy_texture_units(dev);

    c_ptr_array_free(dev->uniform_names, true);
    c_hash_table_destroy(dev->uniform_name_hash);

    c_hash_table_destroy(dev->attribute_name_states_hash);
    c_array_free(dev->attribute_name_index_map, true);

    c_byte_array_free(dev->buffer_map_fallback_array, true);

#ifdef CG_HAS_UV_SUPPORT
    _cg_uv_cleanup(dev);
#endif

    cg_object_unref(dev->display);
    cg_object_unref(dev->renderer);

    c_free(dev);
}

cg_device_t *
_cg_device_get_default(void)
{
    c_return_val_if_fail(_cg_device != NULL, NULL);
    return _cg_device;
}

cg_display_t *
cg_device_get_display(cg_device_t *dev)
{
    return dev->display;
}

cg_renderer_t *
cg_device_get_renderer(cg_device_t *dev)
{
    return dev->renderer;
}

#ifdef CG_HAS_EGL_SUPPORT
EGLDisplay
cg_egl_context_get_egl_display(cg_device_t *dev)
{
    const cg_winsys_vtable_t *winsys = _cg_device_get_winsys(dev);

    /* This should only be called for EGL contexts */
    c_return_val_if_fail(winsys->device_egl_get_egl_display != NULL, NULL);

    return winsys->device_egl_get_egl_display(dev);
}
#endif

bool
_cg_device_update_features(cg_device_t *dev, cg_error_t **error)
{
    return dev->driver_vtable->update_features(dev, error);
}

void
_cg_device_set_current_projection_entry(cg_device_t *dev,
                                         cg_matrix_entry_t *entry)
{
    cg_matrix_entry_ref(entry);
    if (dev->current_projection_entry)
        cg_matrix_entry_unref(dev->current_projection_entry);
    dev->current_projection_entry = entry;
}

void
_cg_device_set_current_modelview_entry(cg_device_t *dev,
                                        cg_matrix_entry_t *entry)
{
    cg_matrix_entry_ref(entry);
    if (dev->current_modelview_entry)
        cg_matrix_entry_unref(dev->current_modelview_entry);
    dev->current_modelview_entry = entry;
}

char **
_cg_device_get_gl_extensions(cg_device_t *dev)
{
    const char *env_disabled_extensions;
    char **ret;

/* In GL 3, querying GL_EXTENSIONS is deprecated so we have to build
 * the array using glGetStringi instead */
#ifdef CG_HAS_GL_SUPPORT
    if (dev->driver == CG_DRIVER_GL3) {
        int num_extensions, i;

        dev->glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);

        ret = c_malloc(sizeof(char *) * (num_extensions + 1));

        for (i = 0; i < num_extensions; i++) {
            const char *ext =
                (const char *)dev->glGetStringi(GL_EXTENSIONS, i);
            ret[i] = c_strdup(ext);
        }

        ret[num_extensions] = NULL;
    } else
#endif
    {
        const char *all_extensions =
            (const char *)dev->glGetString(GL_EXTENSIONS);

        ret = c_strsplit(all_extensions, " ", 0 /* max tokens */);
    }

    if ((env_disabled_extensions = c_getenv("CG_DISABLE_GL_EXTENSIONS")) ||
        _cg_config_disable_gl_extensions) {
        char **split_env_disabled_extensions;
        char **split_conf_disabled_extensions;
        char **src, **dst;

        if (env_disabled_extensions)
            split_env_disabled_extensions =
                c_strsplit(env_disabled_extensions, ",", 0 /* no max tokens */);
        else
            split_env_disabled_extensions = NULL;

        if (_cg_config_disable_gl_extensions)
            split_conf_disabled_extensions = c_strsplit(
                _cg_config_disable_gl_extensions, ",", 0 /* no max tokens */);
        else
            split_conf_disabled_extensions = NULL;

        for (dst = ret, src = ret; *src; src++) {
            char **d;

            if (split_env_disabled_extensions)
                for (d = split_env_disabled_extensions; *d; d++)
                    if (!strcmp(*src, *d))
                        goto disabled;
            if (split_conf_disabled_extensions)
                for (d = split_conf_disabled_extensions; *d; d++)
                    if (!strcmp(*src, *d))
                        goto disabled;

            *(dst++) = *src;
            continue;

disabled:
            c_free(*src);
            continue;
        }

        *dst = NULL;

        if (split_env_disabled_extensions)
            c_strfreev(split_env_disabled_extensions);
        if (split_conf_disabled_extensions)
            c_strfreev(split_conf_disabled_extensions);
    }

    return ret;
}

const char *
_cg_device_get_gl_version(cg_device_t *dev)
{
    const char *version_override;

    if ((version_override = c_getenv("CG_OVERRIDE_GL_VERSION")))
        return version_override;
    else if (_cg_config_override_gl_version)
        return _cg_config_override_gl_version;
    else
        return (const char *)dev->glGetString(GL_VERSION);
}

int64_t
cg_get_clock_time(cg_device_t *dev)
{
    /* XXX: we used to call into the winsys to let it define a clock
     * source, but to avoid corner cases where we we don't know what
     * clock to use we now always use c_get_monotonic_time() - which
     * at least tends to work out well for drivers on Linux - and
     * otherwise the winsys may have to employ some extra logic to map
     * presentation timestamps onto this clock
     */
    return c_get_monotonic_time();
}

cg_atlas_set_t *
_cg_get_atlas_set(cg_device_t *dev)
{
    return dev->atlas_set;
}
