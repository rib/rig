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

#ifndef __CG_DEVICE_PRIVATE_H
#define __CG_DEVICE_PRIVATE_H

#ifdef CG_HAS_UV_SUPPORT
#include <uv.h>
#endif

#include "cg-device.h"
#include "cg-winsys-private.h"
#include "cg-flags.h"

#ifdef CG_HAS_XLIB_SUPPORT
#include "cg-xlib-renderer-private.h"
#endif

#include "cg-display-private.h"
#include "cg-clip-stack.h"
#include "cg-matrix-stack.h"
#include "cg-pipeline-private.h"
#include "cg-buffer-private.h"
#include "cg-bitmask.h"
#include "cg-atlas-set.h"
#include "cg-driver.h"
#include "cg-texture-driver.h"
#include "cg-pipeline-cache.h"
#include "cg-texture-2d.h"
#include "cg-texture-3d.h"
#include "cg-sampler-cache-private.h"
#include "cg-gpu-info-private.h"
#include "cg-gl-header.h"
#include "cg-framebuffer-private.h"
#include "cg-onscreen-private.h"
#include "cg-fence-private.h"
#include "cg-loop-private.h"
#include "cg-private.h"

typedef struct {
    GLfloat v[3];
    GLfloat t[2];
    GLubyte c[4];
} cg_texture_gl_vertex_t;

struct _cg_device_t {
    cg_object_t _parent;

    cg_renderer_t *renderer;
    cg_display_t *display;

    cg_driver_t driver;

    bool connected;

    /* Information about the GPU and driver which we can use to
       determine certain workarounds */
    cg_gpu_info_t gpu;

    /* vtables for the driver functions */
    const cg_driver_vtable_t *driver_vtable;
    const cg_texture_driver_t *texture_driver;

    int glsl_major;
    int glsl_minor;

    /* This is the GLSL version that we will claim that snippets are
     * written against using the #version pragma. This will be the
     * largest version that is less than or equal to the version
     * provided by the driver without massively altering the syntax. Eg,
     * we wouldn't use version 1.3 even if it is available because that
     * removes the ‘attribute’ and ‘varying’ keywords. */
    int glsl_version_to_use;

    /* Features cache */
    unsigned long features[CG_FLAGS_N_LONGS_FOR_SIZE(_CG_N_FEATURE_IDS)];
    unsigned long private_features
    [CG_FLAGS_N_LONGS_FOR_SIZE(CG_N_PRIVATE_FEATURES)];

#ifdef CG_HAS_UV_SUPPORT
    uv_loop_t *uv_mainloop;
    uv_prepare_t uv_mainloop_prepare;
    uv_timer_t uv_mainloop_timer;
    uv_check_t uv_mainloop_check;
    c_list_t uv_poll_sources;
    int uv_poll_sources_age;
    c_array_t *uv_poll_fds;
#endif

    bool needs_viewport_scissor_workaround;
    cg_framebuffer_t *viewport_scissor_workaround_framebuffer;

    cg_pipeline_t *default_pipeline;
    cg_pipeline_layer_t *default_layer_0;
    cg_pipeline_layer_t *default_layer_n;
    cg_pipeline_layer_t *dummy_layer_dependant;

    c_hash_table_t *attribute_name_states_hash;
    c_array_t *attribute_name_index_map;
    int n_attribute_names;

    CGlibBitmask enabled_custom_attributes;

    /* A temporary bitmask that is used when enabling/disabling
     * custom attribute arrays */
    CGlibBitmask enable_custom_attributes_tmp;
    CGlibBitmask changed_bits_tmp;

    /* A few handy matrix constants */
    c_matrix_t identity_matrix;
    c_matrix_t y_flip_matrix;

    /* The matrix stack entries that should be flushed during the next
     * pipeline state flush */
    cg_matrix_entry_t *current_projection_entry;
    cg_matrix_entry_t *current_modelview_entry;

    cg_matrix_entry_t identity_entry;

    /* A cache of the last (immutable) matrix stack entries that were
     * flushed to the GL matrix builtins */
    cg_matrix_entry_cache_t builtin_flushed_projection;
    cg_matrix_entry_cache_t builtin_flushed_modelview;

    c_array_t *texture_units;
    int active_texture_unit;

    /* Pipelines */
    cg_pipeline_t *opaque_color_pipeline; /* to check for simple pipelines */
    c_string_t *codegen_header_buffer;
    c_string_t *codegen_source_buffer;

    cg_pipeline_cache_t *pipeline_cache;

    /* Textures */
    cg_texture_2d_t *default_gl_texture_2d_tex;
    cg_texture_3d_t *default_gl_texture_3d_tex;

    c_llist_t *framebuffers;

    /* Some simple caching, to minimize state changes... */
    cg_pipeline_t *current_pipeline;
    unsigned long current_pipeline_changes_since_flush;
    bool current_pipeline_with_color_attrib;
    bool current_pipeline_unknown_color_alpha;
    unsigned long current_pipeline_age;

    bool gl_blend_enable_cache;

    bool depth_test_enabled_cache;
    cg_depth_test_function_t depth_test_function_cache;
    bool depth_writing_enabled_cache;
    float depth_range_near_cache;
    float depth_range_far_cache;

    cg_buffer_t *current_buffer[CG_BUFFER_BIND_TARGET_COUNT];

    /* Framebuffers */
    unsigned long current_draw_buffer_state_flushed;
    unsigned long current_draw_buffer_changes;
    cg_framebuffer_t *current_draw_buffer;
    cg_framebuffer_t *current_read_buffer;

    bool have_last_offscreen_allocate_flags;
    cg_offscreen_allocate_flags_t last_offscreen_allocate_flags;

    cg_gles2_context_t *current_gles2_context;
    c_queue_t gles2_context_stack;

    /* This becomes true the first time the context is bound to an
     * onscreen buffer. This is used by cg-framebuffer-gl to determine
     * when to initialise the glDrawBuffer state */
    bool was_bound_to_onscreen;

    /* Primitives */
    cg_pipeline_t *stencil_pipeline;

    /* Pre-generated VBOs containing indices to generate GL_TRIANGLES
       out of a vertex array of quads */
    cg_indices_t *rectangle_byte_indices;
    cg_indices_t *rectangle_short_indices;
    int rectangle_short_indices_len;

    cg_pipeline_t *texture_download_pipeline;
    cg_pipeline_t *blit_texture_pipeline;

    cg_atlas_set_t *atlas_set;

    /* Cached values for GL_MAX_TEXTURE_[IMAGE_]UNITS to avoid calling
       glGetInteger too often */
    GLint max_texture_units;
    GLint max_activateable_texture_units;

    GLuint current_gl_program;

    bool current_gl_dither_enabled;
    cg_color_mask_t current_gl_color_mask;

    /* Clipping */
    /* true if we have a valid clipping stack flushed. In that case
       current_clip_stack will describe what the current state is. If
       this is false then the current clip stack is completely unknown
       so it will need to be reflushed. In that case current_clip_stack
       doesn't need to be a valid pointer. We can't just use NULL in
       current_clip_stack to mark a dirty state because NULL is a valid
       stack (meaning no clipping) */
    bool current_clip_stack_valid;
    /* The clip state that was flushed. This isn't intended to be used
       as a stack to push and pop new entries. Instead the current stack
       that the user wants is part of the framebuffer state. This is
       just used to record the flush state so we can avoid flushing the
       same state multiple times. When the clip state is flushed this
       will hold a reference */
    cg_clip_stack_t *current_clip_stack;

    /* This is used as a temporary buffer to fill a cg_buffer_t when
       cg_buffer_map fails and we only want to map to fill it with new
       data */
    c_byte_array_t *buffer_map_fallback_array;
    bool buffer_map_fallback_in_use;
    size_t buffer_map_fallback_offset;

    cg_winsys_rectangle_state_t rectangle_state;

    cg_sampler_cache_t *sampler_cache;

/* FIXME: remove these when we remove the last xlib based clutter
 * backend. they should be tracked as part of the renderer but e.g.
 * the eglx backend doesn't yet have a corresponding CGlib winsys
 * and so we wont have a renderer in that case. */
#ifdef CG_HAS_XLIB_SUPPORT
    int damage_base;
    /* List of callback functions that will be given every Xlib event */
    c_sllist_t *event_filters;
    /* Current top of the XError trap state stack. The actual memory for
       these is expected to be allocated on the stack by the caller */
    cg_xlib_trap_state_t *trap_state;
#endif

    unsigned long winsys_features
    [CG_FLAGS_N_LONGS_FOR_SIZE(CG_WINSYS_FEATURE_N_FEATURES)];
    void *winsys;

    /* Array of names of uniforms. These are used like quarks to give a
       unique number to each uniform name except that we ensure that
       they increase sequentially so that we can use the id as an index
       into a bitfield representing the uniforms that a pipeline
       overrides from its parent. */
    c_ptr_array_t *uniform_names;
    /* A hash table to quickly get an index given an existing name. The
       name strings are owned by the uniform_names array. The values are
       the uniform location cast to a pointer. */
    c_hash_table_t *uniform_name_hash;
    int n_uniform_names;

    cg_poll_source_t *fences_poll_source;
    c_list_t fences;

    /* True once we've seen some kind of presentation
     * timestamp which we can then determine if it corresponds to
     * c_get_monotonic_time() or not */
    bool presentation_time_seen;

    /* Valid once above presentation_clock_seen == true then
     * this can be checked to know if presentation timestamps
     * come from the same clock as c_get_monotonic_time() */
    bool presentation_clock_is_monotonic;

/* This defines a list of function pointers that CGlib uses from
   either GL or GLES. All functions are accessed indirectly through
   these pointers rather than linking to them directly */
#ifndef APIENTRY
#define APIENTRY
#endif

#define CG_EXT_BEGIN(name,                                                     \
                     min_gl_major,                                             \
                     min_gl_minor,                                             \
                     gles_availability,                                        \
                     extension_suffixes,                                       \
                     extension_names)
#define CG_EXT_FUNCTION(ret, name, args) ret(APIENTRY *name) args;
#define CG_EXT_END()

#include "gl-prototypes/cg-all-functions.h"

#undef CG_EXT_BEGIN
#undef CG_EXT_FUNCTION
#undef CG_EXT_END
};

cg_device_t *_cg_device_get_default();

const cg_winsys_vtable_t *_cg_device_get_winsys(cg_device_t *dev);

/* Query the GL extensions and lookup the corresponding function
 * pointers. Theoretically the list of extensions can change for
 * different GL contexts so it is the winsys backend's responsiblity
 * to know when to re-query the GL extensions. The backend should also
 * check whether the GL context is supported by CGlib. If not it should
 * return false and set @error */
bool _cg_device_update_features(cg_device_t *dev, cg_error_t **error);

/* Obtains the context and returns retval if NULL */
#define _CG_GET_DEVICE(devvar, retval) \
    cg_device_t *devvar = _cg_device_get_default(); \
    if (devvar == NULL) \
        return retval;

#define NO_RETVAL

void _cg_device_set_current_projection_entry(cg_device_t *dev,
                                             cg_matrix_entry_t *entry);

void _cg_device_set_current_modelview_entry(cg_device_t *dev,
                                            cg_matrix_entry_t *entry);

/*
 * _cg_device_get_gl_extensions:
 * @context: A cg_device_t
 *
 * Return value: a NULL-terminated array of strings representing the
 *   supported extensions by the current driver. This array is owned
 *   by the caller and should be freed with c_strfreev().
 */
char **_cg_device_get_gl_extensions(cg_device_t *dev);

const char *_cg_device_get_gl_version(cg_device_t *dev);

cg_atlas_set_t *_cg_get_atlas_set(cg_device_t *dev);

#endif /* __CG_DEVICE_PRIVATE_H */
