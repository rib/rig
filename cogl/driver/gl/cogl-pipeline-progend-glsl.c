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
 *
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl-util.h"
#include "cogl-context-private.h"
#include "cogl-util-gl-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-offscreen.h"

#ifdef CG_PIPELINE_PROGEND_GLSL

#include "cogl-context-private.h"
#include "cogl-object-private.h"
#include "cogl-pipeline-fragend-glsl-private.h"
#include "cogl-pipeline-vertend-glsl-private.h"
#include "cogl-pipeline-cache.h"
#include "cogl-pipeline-state-private.h"
#include "cogl-attribute-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-pipeline-progend-glsl-private.h"

/* These are used to generalise updating some uniforms that are
   required when building for drivers missing some fixed function
   state that we use */

typedef void (*update_uniform_func_t)(cg_context_t *ctx,
                                      cg_pipeline_t *pipeline,
                                      int uniform_location,
                                      void *getter_func);

static void update_float_uniform(cg_context_t *ctx,
                                 cg_pipeline_t *pipeline,
                                 int uniform_location,
                                 void *getter_func);

typedef struct {
    const char *uniform_name;
    void *getter_func;
    update_uniform_func_t update_func;
    cg_pipeline_state_t change;

    /* This builtin is only necessary if the following private feature
     * is not implemented in the driver */
    cg_private_feature_t feature_replacement;
} builtin_uniform_data_t;

static builtin_uniform_data_t builtin_uniforms[] = {
    { "cg_point_size_in",
      cg_pipeline_get_point_size,
      update_float_uniform,
      CG_PIPELINE_STATE_POINT_SIZE,
      CG_PRIVATE_FEATURE_BUILTIN_POINT_SIZE_UNIFORM },
    { "_cg_alpha_test_ref", cg_pipeline_get_alpha_test_reference,
      update_float_uniform, CG_PIPELINE_STATE_ALPHA_FUNC_REFERENCE,
      CG_N_PRIVATE_FEATURES } /* XXX: used as a non-existent "feature"
                               * that will never be found. */
};

const cg_pipeline_progend_t _cg_pipeline_glsl_progend;

typedef struct _unit_state_t {
    unsigned int dirty_combine_constant : 1;

    GLint combine_constant_uniform;
} unit_state_t;

typedef struct {
    cg_context_t *ctx;

    unsigned int ref_count;

    GLuint program;

    unsigned long dirty_builtin_uniforms;
    GLint builtin_uniform_locations[C_N_ELEMENTS(builtin_uniforms)];

    GLint modelview_uniform;
    GLint projection_uniform;
    GLint mvp_uniform;

    cg_matrix_entry_cache_t projection_cache;
    cg_matrix_entry_cache_t modelview_cache;

    /* We need to track the last pipeline that the program was used with
     * so know if we need to update all of the uniforms */
    cg_pipeline_t *last_used_for_pipeline;

    /* Array of GL uniform locations indexed by Cogl's uniform
       location. We are careful only to allocated this array if a custom
       uniform is actually set */
    c_array_t *uniform_locations;

    /* Array of attribute locations. */
    c_array_t *attribute_locations;

    /* The 'flip' uniform is used to flip the geometry upside-down when
       the framebuffer requires it only when there are vertex
       snippets. Otherwise this is acheived using the projection
       matrix */
    GLint flip_uniform;
    int flushed_flip_state;

    unit_state_t *unit_state;

    cg_pipeline_cache_entry_t *cache_entry;
} cg_pipeline_program_state_t;

static cg_user_data_key_t program_state_key;

static cg_pipeline_program_state_t *
get_program_state(cg_pipeline_t *pipeline)
{
    return cg_object_get_user_data(CG_OBJECT(pipeline), &program_state_key);
}

#define UNIFORM_LOCATION_UNKNOWN -2

#define ATTRIBUTE_LOCATION_UNKNOWN -2

/* Under GLES2 the vertex attribute API needs to query the attribute
   numbers because it can't used the fixed function API to set the
   builtin attributes. We cache the attributes here because the
   progend knows when the program is changed so it can clear the
   cache. This should always be called after the pipeline is flushed
   so they can assert that the gl program is valid */

/* All attributes names get internally mapped to a global set of
 * sequential indices when they are setup which we need to need to
 * then be able to map to a GL attribute location once we have
 * a linked GLSL program */

int
_cg_pipeline_progend_glsl_get_attrib_location(cg_pipeline_t *pipeline,
                                              int name_index)
{
    cg_pipeline_program_state_t *program_state = get_program_state(pipeline);
    int *locations;

    _CG_GET_CONTEXT(ctx, -1);

    c_return_val_if_fail(program_state != NULL, -1);
    c_return_val_if_fail(program_state->program != 0, -1);

    if (C_UNLIKELY(program_state->attribute_locations == NULL))
        program_state->attribute_locations =
            c_array_new(false, false, sizeof(int));

    if (C_UNLIKELY(program_state->attribute_locations->len <= name_index)) {
        int i = program_state->attribute_locations->len;
        c_array_set_size(program_state->attribute_locations, name_index + 1);
        for (; i < program_state->attribute_locations->len; i++)
            c_array_index(program_state->attribute_locations, int, i) =
                ATTRIBUTE_LOCATION_UNKNOWN;
    }

    locations = &c_array_index(program_state->attribute_locations, int, 0);

    if (locations[name_index] == ATTRIBUTE_LOCATION_UNKNOWN) {
        cg_attribute_name_state_t *name_state =
            c_array_index(ctx->attribute_name_index_map,
                          cg_attribute_name_state_t *,
                          name_index);

        c_return_val_if_fail(name_state != NULL, 0);

        GE_RET(locations[name_index],
               ctx,
               glGetAttribLocation(program_state->program, name_state->name));
    }

    return locations[name_index];
}

static void
clear_attribute_cache(cg_pipeline_program_state_t *program_state)
{
    if (program_state->attribute_locations) {
        c_array_free(program_state->attribute_locations, true);
        program_state->attribute_locations = NULL;
    }
}

static void
clear_flushed_matrix_stacks(cg_pipeline_program_state_t *program_state)
{
    _cg_matrix_entry_cache_destroy(&program_state->projection_cache);
    _cg_matrix_entry_cache_init(&program_state->projection_cache);
    _cg_matrix_entry_cache_destroy(&program_state->modelview_cache);
    _cg_matrix_entry_cache_init(&program_state->modelview_cache);
}

static cg_pipeline_program_state_t *
program_state_new(
    cg_context_t *ctx, int n_layers, cg_pipeline_cache_entry_t *cache_entry)
{
    cg_pipeline_program_state_t *program_state;

    program_state = c_slice_new(cg_pipeline_program_state_t);
    program_state->ctx = ctx;
    program_state->ref_count = 1;
    program_state->program = 0;
    program_state->unit_state = c_new(unit_state_t, n_layers);
    program_state->uniform_locations = NULL;
    program_state->attribute_locations = NULL;
    program_state->cache_entry = cache_entry;
    _cg_matrix_entry_cache_init(&program_state->modelview_cache);
    _cg_matrix_entry_cache_init(&program_state->projection_cache);

    return program_state;
}

static void
destroy_program_state(void *user_data, void *instance)
{
    cg_pipeline_program_state_t *program_state = user_data;

    /* If the program state was last used for this pipeline then clear
       it so that if same address gets used again for a new pipeline
       then we won't think it's the same pipeline and avoid updating the
       uniforms */
    if (program_state->last_used_for_pipeline == instance)
        program_state->last_used_for_pipeline = NULL;

    if (program_state->cache_entry &&
        program_state->cache_entry->pipeline != instance)
        program_state->cache_entry->usage_count--;

    if (--program_state->ref_count == 0) {
        cg_context_t *ctx = program_state->ctx;

        clear_attribute_cache(program_state);

        _cg_matrix_entry_cache_destroy(&program_state->projection_cache);
        _cg_matrix_entry_cache_destroy(&program_state->modelview_cache);

        if (program_state->program)
            GE(ctx, glDeleteProgram(program_state->program));

        c_free(program_state->unit_state);

        if (program_state->uniform_locations)
            c_array_free(program_state->uniform_locations, true);

        c_slice_free(cg_pipeline_program_state_t, program_state);
    }
}

static void
set_program_state(cg_pipeline_t *pipeline,
                  cg_pipeline_program_state_t *program_state)
{
    if (program_state) {
        program_state->ref_count++;

        /* If we're not setting the state on the template pipeline then
         * mark it as a usage of the pipeline cache entry */
        if (program_state->cache_entry &&
            program_state->cache_entry->pipeline != pipeline)
            program_state->cache_entry->usage_count++;
    }

    _cg_object_set_user_data(CG_OBJECT(pipeline),
                             &program_state_key,
                             program_state,
                             destroy_program_state);
}

static void
dirty_program_state(cg_pipeline_t *pipeline)
{
    cg_object_set_user_data(
        CG_OBJECT(pipeline), &program_state_key, NULL, NULL);
}

static void
link_program(cg_context_t *ctx, GLint gl_program)
{
    GLint link_status;

    GE(ctx, glLinkProgram(gl_program));

    GE(ctx, glGetProgramiv(gl_program, GL_LINK_STATUS, &link_status));

    if (!link_status) {
        GLint log_length;
        GLsizei out_log_length;
        char *log;

        GE(ctx, glGetProgramiv(gl_program, GL_INFO_LOG_LENGTH, &log_length));

        log = c_malloc(log_length);

        GE(ctx,
           glGetProgramInfoLog(gl_program, log_length, &out_log_length, log));

        c_warning("Failed to link GLSL program:\n%.*s\n", log_length, log);

        c_free(log);
    }
}

typedef struct {
    cg_context_t *ctx;
    int unit;
    GLuint gl_program;
    bool update_all;
    cg_pipeline_program_state_t *program_state;
} update_uniforms_state_t;

static bool
get_uniform_cb(cg_pipeline_t *pipeline, int layer_index, void *user_data)
{
    update_uniforms_state_t *state = user_data;
    cg_context_t *ctx = state->ctx;
    cg_pipeline_program_state_t *program_state = state->program_state;
    unit_state_t *unit_state = &program_state->unit_state[state->unit];
    GLint uniform_location;

    /* We can reuse the source buffer to create the uniform name because
       the program has now been linked */
    c_string_set_size(ctx->codegen_source_buffer, 0);
    c_string_append_printf(
        ctx->codegen_source_buffer, "cg_sampler%i", layer_index);

    GE_RET(uniform_location,
           ctx,
           glGetUniformLocation(state->gl_program,
                                ctx->codegen_source_buffer->str));

    /* We can set the uniform immediately because the samplers are the
       unit index not the texture object number so it will never
       change. Unfortunately GL won't let us use a constant instead of a
       uniform */
    if (uniform_location != -1)
        GE(ctx, glUniform1i(uniform_location, state->unit));

    c_string_set_size(ctx->codegen_source_buffer, 0);
    c_string_append_printf(
        ctx->codegen_source_buffer, "_cg_layer_constant_%i", layer_index);

    GE_RET(uniform_location,
           ctx,
           glGetUniformLocation(state->gl_program,
                                ctx->codegen_source_buffer->str));

    unit_state->combine_constant_uniform = uniform_location;

    state->unit++;

    return true;
}

static bool
update_constants_cb(cg_pipeline_t *pipeline, int layer_index, void *user_data)
{
    update_uniforms_state_t *state = user_data;
    cg_context_t *ctx = state->ctx;
    cg_pipeline_program_state_t *program_state = state->program_state;
    unit_state_t *unit_state = &program_state->unit_state[state->unit++];

    if (unit_state->combine_constant_uniform != -1 &&
        (state->update_all || unit_state->dirty_combine_constant)) {
        float constant[4];
        _cg_pipeline_get_layer_combine_constant(
            pipeline, layer_index, constant);
        GE(ctx,
           glUniform4fv(unit_state->combine_constant_uniform, 1, constant));
        unit_state->dirty_combine_constant = false;
    }

    return true;
}

static void
update_builtin_uniforms(cg_context_t *context,
                        cg_pipeline_t *pipeline,
                        GLuint gl_program,
                        cg_pipeline_program_state_t *program_state)
{
    int i;

    if (program_state->dirty_builtin_uniforms == 0)
        return;

    for (i = 0; i < C_N_ELEMENTS(builtin_uniforms); i++) {
        if (!_cg_has_private_feature(context,
                                     builtin_uniforms[i].feature_replacement) &&
            (program_state->dirty_builtin_uniforms & (1 << i)) &&
            program_state->builtin_uniform_locations[i] != -1) {
            builtin_uniforms[i]
            .update_func(context,
                         pipeline,
                         program_state->builtin_uniform_locations[i],
                         builtin_uniforms[i].getter_func);
        }
    }

    program_state->dirty_builtin_uniforms = 0;
}

typedef struct {
    cg_pipeline_program_state_t *program_state;
    unsigned long *uniform_differences;
    int n_differences;
    cg_context_t *ctx;
    const cg_boxed_value_t *values;
    int value_index;
} flush_uniforms_closure_t;

static bool
flush_uniform_cb(int uniform_num, void *user_data)
{
    flush_uniforms_closure_t *data = user_data;

    if (CG_FLAGS_GET(data->uniform_differences, uniform_num)) {
        c_array_t *uniform_locations;
        GLint uniform_location;

        if (data->program_state->uniform_locations == NULL)
            data->program_state->uniform_locations =
                c_array_new(false, false, sizeof(GLint));

        uniform_locations = data->program_state->uniform_locations;

        if (uniform_locations->len <= uniform_num) {
            unsigned int old_len = uniform_locations->len;

            c_array_set_size(uniform_locations, uniform_num + 1);

            while (old_len <= uniform_num) {
                c_array_index(uniform_locations, GLint, old_len) =
                    UNIFORM_LOCATION_UNKNOWN;
                old_len++;
            }
        }

        uniform_location = c_array_index(uniform_locations, GLint, uniform_num);

        if (uniform_location == UNIFORM_LOCATION_UNKNOWN) {
            const char *uniform_name =
                c_ptr_array_index(data->ctx->uniform_names, uniform_num);

            uniform_location = data->ctx->glGetUniformLocation(
                data->program_state->program, uniform_name);
            c_array_index(uniform_locations, GLint, uniform_num) =
                uniform_location;
        }

        if (uniform_location != -1)
            _cg_boxed_value_set_uniform(
                data->ctx, uniform_location, data->values + data->value_index);

        data->n_differences--;
        CG_FLAGS_SET(data->uniform_differences, uniform_num, false);
    }

    data->value_index++;

    return data->n_differences > 0;
}

static void
_cg_pipeline_progend_glsl_flush_uniforms(
    cg_context_t *ctx,
    cg_pipeline_t *pipeline,
    cg_pipeline_program_state_t *program_state,
    GLuint gl_program,
    bool program_changed)
{
    cg_pipeline_uniforms_state_t *uniforms_state;
    flush_uniforms_closure_t data;
    int n_uniform_longs;

    if (pipeline->differences & CG_PIPELINE_STATE_UNIFORMS)
        uniforms_state = &pipeline->big_state->uniforms_state;
    else
        uniforms_state = NULL;

    data.program_state = program_state;
    data.ctx = ctx;

    n_uniform_longs = CG_FLAGS_N_LONGS_FOR_SIZE(ctx->n_uniform_names);

    data.uniform_differences = c_newa(unsigned long, n_uniform_longs);

    /* Try to find a common ancestor for the values that were already
       flushed on the pipeline that this program state was last used for
       so we can avoid flushing those */

    if (program_changed || program_state->last_used_for_pipeline == NULL) {
        if (program_changed) {
            /* The program has changed so all of the uniform locations
               are invalid */
            if (program_state->uniform_locations)
                c_array_set_size(program_state->uniform_locations, 0);
        }

        /* We need to flush everything so mark all of the uniforms as
           dirty */
        memset(data.uniform_differences,
               0xff,
               n_uniform_longs * sizeof(unsigned long));
        data.n_differences = G_MAXINT;
    } else if (program_state->last_used_for_pipeline) {
        int i;

        memset(data.uniform_differences,
               0,
               n_uniform_longs * sizeof(unsigned long));
        _cg_pipeline_compare_uniform_differences(
            data.uniform_differences,
            program_state->last_used_for_pipeline,
            pipeline);

        /* We need to be sure to flush any uniforms that have changed
           since the last flush */
        if (uniforms_state)
            _cg_bitmask_set_flags(&uniforms_state->changed_mask,
                                  data.uniform_differences);

        /* Count the number of differences. This is so we can stop early
           when we've flushed all of them */
        data.n_differences = 0;

        for (i = 0; i < n_uniform_longs; i++)
            data.n_differences +=
                _cg_util_popcountl(data.uniform_differences[i]);
    }

    while (pipeline && data.n_differences > 0) {
        if (pipeline->differences & CG_PIPELINE_STATE_UNIFORMS) {
            const cg_pipeline_uniforms_state_t *parent_uniforms_state =
                &pipeline->big_state->uniforms_state;

            data.values = parent_uniforms_state->override_values;
            data.value_index = 0;

            _cg_bitmask_foreach(
                &parent_uniforms_state->override_mask, flush_uniform_cb, &data);
        }

        pipeline = _cg_pipeline_get_parent(pipeline);
    }

    if (uniforms_state)
        _cg_bitmask_clear_all(&uniforms_state->changed_mask);
}

static bool
_cg_pipeline_progend_glsl_start(cg_pipeline_t *pipeline)
{
    _CG_GET_CONTEXT(ctx, false);

    if (!cg_has_feature(ctx, CG_FEATURE_ID_GLSL))
        return false;

    return true;
}

static void
_cg_pipeline_progend_glsl_end(cg_pipeline_t *pipeline,
                              unsigned long pipelines_difference)
{
    cg_pipeline_program_state_t *program_state;
    GLuint gl_program;
    bool program_changed = false;
    update_uniforms_state_t state;
    cg_pipeline_cache_entry_t *cache_entry = NULL;

    _CG_GET_CONTEXT(ctx, NO_RETVAL);

    program_state = get_program_state(pipeline);

    if (program_state == NULL) {
        cg_pipeline_t *authority;

        /* Get the authority for anything affecting program state. This
           should include both fragment codegen state and vertex codegen
           state */
        authority = _cg_pipeline_find_equivalent_parent(
            pipeline,
            (_cg_pipeline_get_state_for_vertex_codegen(ctx) |
             _cg_pipeline_get_state_for_fragment_codegen(ctx)) &
            ~CG_PIPELINE_STATE_LAYERS,
            _cg_pipeline_get_layer_state_for_fragment_codegen(ctx) |
            CG_PIPELINE_LAYER_STATE_AFFECTS_VERTEX_CODEGEN);

        program_state = get_program_state(authority);

        if (program_state == NULL) {
            /* Check if there is already a similar cached pipeline whose
               program state we can share */
            if (C_LIKELY(
                    !(CG_DEBUG_ENABLED(CG_DEBUG_DISABLE_PROGRAM_CACHES)))) {
                cache_entry = _cg_pipeline_cache_get_combined_template(
                    ctx->pipeline_cache, authority);

                program_state = get_program_state(cache_entry->pipeline);
            }

            if (program_state)
                program_state->ref_count++;
            else
                program_state = program_state_new(
                    ctx, cg_pipeline_get_n_layers(authority), cache_entry);

            set_program_state(authority, program_state);

            program_state->ref_count--;

            if (cache_entry)
                set_program_state(cache_entry->pipeline, program_state);
        }

        if (authority != pipeline)
            set_program_state(pipeline, program_state);
    }

    if (program_state->program == 0) {
        GLuint backend_shader;

        GE_RET(program_state->program, ctx, glCreateProgram());

        /* Attach any shaders from the GLSL backends */
        if ((backend_shader = _cg_pipeline_fragend_glsl_get_shader(pipeline)))
            GE(ctx, glAttachShader(program_state->program, backend_shader));
        if ((backend_shader = _cg_pipeline_vertend_glsl_get_shader(pipeline)))
            GE(ctx, glAttachShader(program_state->program, backend_shader));

        /* XXX: OpenGL as a special case requires the vertex position to
         * be bound to generic attribute 0 so for simplicity we
         * unconditionally bind the cg_position_in attribute here...
         */
        GE(ctx,
           glBindAttribLocation(program_state->program, 0, "cg_position_in"));

        link_program(ctx, program_state->program);

        program_changed = true;
    }

    gl_program = program_state->program;

    _cg_gl_use_program(ctx, gl_program);

    state.ctx = ctx;
    state.unit = 0;
    state.gl_program = gl_program;
    state.program_state = program_state;

    if (program_changed) {
        cg_pipeline_foreach_layer(pipeline, get_uniform_cb, &state);
        clear_attribute_cache(program_state);

        GE_RET(program_state->flip_uniform,
               ctx,
               glGetUniformLocation(gl_program, "_cg_flip_vector"));
        program_state->flushed_flip_state = -1;
    }

    state.unit = 0;
    state.update_all =
        (program_changed || program_state->last_used_for_pipeline != pipeline);

    cg_pipeline_foreach_layer(pipeline, update_constants_cb, &state);

    if (program_changed) {
        int i;

        clear_flushed_matrix_stacks(program_state);

        for (i = 0; i < C_N_ELEMENTS(builtin_uniforms); i++)
            if (!_cg_has_private_feature(
                    ctx, builtin_uniforms[i].feature_replacement))
                GE_RET(program_state->builtin_uniform_locations[i],
                       ctx,
                       glGetUniformLocation(gl_program,
                                            builtin_uniforms[i].uniform_name));

        GE_RET(program_state->modelview_uniform,
               ctx,
               glGetUniformLocation(gl_program, "cg_modelview_matrix"));

        GE_RET(program_state->projection_uniform,
               ctx,
               glGetUniformLocation(gl_program, "cg_projection_matrix"));

        GE_RET(
            program_state->mvp_uniform,
            ctx,
            glGetUniformLocation(gl_program, "cg_modelview_projection_matrix"));
    }

    if (program_changed || program_state->last_used_for_pipeline != pipeline)
        program_state->dirty_builtin_uniforms = ~(unsigned long)0;

    update_builtin_uniforms(ctx, pipeline, gl_program, program_state);

    _cg_pipeline_progend_glsl_flush_uniforms(
        ctx, pipeline, program_state, gl_program, program_changed);

    /* We need to track the last pipeline that the program was used with
     * so know if we need to update all of the uniforms */
    program_state->last_used_for_pipeline = pipeline;
}

static void
_cg_pipeline_progend_glsl_pre_change_notify(cg_pipeline_t *pipeline,
                                            cg_pipeline_state_t change,
                                            const cg_color_t *new_color)
{
    _CG_GET_CONTEXT(ctx, NO_RETVAL);

    if ((change & (_cg_pipeline_get_state_for_vertex_codegen(ctx) |
                   _cg_pipeline_get_state_for_fragment_codegen(ctx)))) {
        dirty_program_state(pipeline);
    } else {
        int i;

        for (i = 0; i < C_N_ELEMENTS(builtin_uniforms); i++)
            if (!_cg_has_private_feature(
                    ctx, builtin_uniforms[i].feature_replacement) &&
                (change & builtin_uniforms[i].change)) {
                cg_pipeline_program_state_t *program_state =
                    get_program_state(pipeline);
                if (program_state)
                    program_state->dirty_builtin_uniforms |= 1 << i;
                return;
            }
    }
}

/* NB: layers are considered immutable once they have any dependants
 * so although multiple pipelines can end up depending on a single
 * static layer, we can guarantee that if a layer is being *changed*
 * then it can only have one pipeline depending on it.
 *
 * XXX: Don't forget this is *pre* change, we can't read the new value
 * yet!
 */
static void
_cg_pipeline_progend_glsl_layer_pre_change_notify(
    cg_pipeline_t *owner,
    cg_pipeline_layer_t *layer,
    cg_pipeline_layer_state_t change)
{
    _CG_GET_CONTEXT(ctx, NO_RETVAL);

    if ((change & (_cg_pipeline_get_layer_state_for_fragment_codegen(ctx) |
                   CG_PIPELINE_LAYER_STATE_AFFECTS_VERTEX_CODEGEN))) {
        dirty_program_state(owner);
    } else if (change & CG_PIPELINE_LAYER_STATE_COMBINE_CONSTANT) {
        cg_pipeline_program_state_t *program_state = get_program_state(owner);
        if (program_state) {
            int unit_index = _cg_pipeline_layer_get_unit_index(layer);
            program_state->unit_state[unit_index].dirty_combine_constant = true;
        }
    }
}

static void
_cg_pipeline_progend_glsl_pre_paint(cg_pipeline_t *pipeline,
                                    cg_framebuffer_t *framebuffer)
{
    bool needs_flip;
    cg_matrix_entry_t *projection_entry;
    cg_matrix_entry_t *modelview_entry;
    cg_pipeline_program_state_t *program_state;
    bool modelview_changed;
    bool projection_changed;
    bool need_modelview;
    bool need_projection;
    cg_matrix_t modelview, projection;

    _CG_GET_CONTEXT(ctx, NO_RETVAL);

    program_state = get_program_state(pipeline);

    projection_entry = ctx->current_projection_entry;
    modelview_entry = ctx->current_modelview_entry;

    /* An initial pipeline is flushed while creating the context. At
       this point there are no matrices selected so we can't do
       anything */
    if (modelview_entry == NULL || projection_entry == NULL)
        return;

    needs_flip = cg_is_offscreen(ctx->current_draw_buffer);

    projection_changed = _cg_matrix_entry_cache_maybe_update(
        &program_state->projection_cache,
        projection_entry,
        (needs_flip && program_state->flip_uniform == -1));

    modelview_changed =
        _cg_matrix_entry_cache_maybe_update(&program_state->modelview_cache,
                                            modelview_entry,
                                            /* never flip modelview */
                                            false);

    if (modelview_changed || projection_changed) {
        if (program_state->mvp_uniform != -1)
            need_modelview = need_projection = true;
        else {
            need_projection =
                (program_state->projection_uniform != -1 && projection_changed);
            need_modelview =
                (program_state->modelview_uniform != -1 && modelview_changed);
        }

        if (need_modelview)
            cg_matrix_entry_get(modelview_entry, &modelview);
        if (need_projection) {
            if (needs_flip && program_state->flip_uniform == -1) {
                cg_matrix_t tmp_matrix;
                cg_matrix_entry_get(projection_entry, &tmp_matrix);
                cg_matrix_multiply(
                    &projection, &ctx->y_flip_matrix, &tmp_matrix);
            } else
                cg_matrix_entry_get(projection_entry, &projection);
        }

        if (projection_changed && program_state->projection_uniform != -1)
            GE(ctx,
               glUniformMatrix4fv(program_state->projection_uniform,
                                  1, /* count */
                                  false, /* transpose */
                                  cg_matrix_get_array(&projection)));

        if (modelview_changed && program_state->modelview_uniform != -1)
            GE(ctx,
               glUniformMatrix4fv(program_state->modelview_uniform,
                                  1, /* count */
                                  false, /* transpose */
                                  cg_matrix_get_array(&modelview)));

        if (program_state->mvp_uniform != -1) {
            /* The journal usually uses an identity matrix for the
               modelview so we can optimise this common case by
               avoiding the matrix multiplication */
            if (cg_matrix_entry_is_identity(modelview_entry)) {
                GE(ctx,
                   glUniformMatrix4fv(program_state->mvp_uniform,
                                      1, /* count */
                                      false, /* transpose */
                                      cg_matrix_get_array(&projection)));
            } else {
                cg_matrix_t combined;

                cg_matrix_multiply(&combined, &projection, &modelview);
                GE(ctx,
                   glUniformMatrix4fv(program_state->mvp_uniform,
                                      1, /* count */
                                      false, /* transpose */
                                      cg_matrix_get_array(&combined)));
            }
        }
    }

    if (program_state->flip_uniform != -1 &&
        program_state->flushed_flip_state != needs_flip) {
        static const float do_flip[4] = { 1.0f, -1.0f, 1.0f, 1.0f };
        static const float dont_flip[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        GE(ctx,
           glUniform4fv(program_state->flip_uniform,
                        1, /* count */
                        needs_flip ? do_flip : dont_flip));
        program_state->flushed_flip_state = needs_flip;
    }
}

static void
update_float_uniform(cg_context_t *ctx,
                     cg_pipeline_t *pipeline,
                     int uniform_location,
                     void *getter_func)
{
    float (*float_getter_func)(cg_pipeline_t *) = getter_func;
    float value;

    value = float_getter_func(pipeline);
    GE(ctx, glUniform1f(uniform_location, value));
}

const cg_pipeline_progend_t _cg_pipeline_glsl_progend = {
    CG_PIPELINE_VERTEND_GLSL,
    CG_PIPELINE_FRAGEND_GLSL,
    _cg_pipeline_progend_glsl_start,
    _cg_pipeline_progend_glsl_end,
    _cg_pipeline_progend_glsl_pre_change_notify,
    _cg_pipeline_progend_glsl_layer_pre_change_notify,
    _cg_pipeline_progend_glsl_pre_paint
};

#endif /* CG_PIPELINE_PROGEND_GLSL */
