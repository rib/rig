/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012  Intel Corporation
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

#include <rut-config.h>

#include <math.h>

#include "rut-gaussian-blurrer.h"

/*
 * RutGaussianBlur
 *
 * What would make sense if you want to animate the bluriness is to give
 * sigma to _init() and compute the number of taps based on that (as
 * discussed with Neil)
 *
 * Being lazy and having already written the code with th number of taps
 * as input, I'll stick with the number of taps, let's say it's because you
 * get a good idea of the performance of the shader this way.
 */

static float
gaussian(float sigma, float x)
{
    return 1 / (sigma * sqrtf(2 * C_PI)) *
           powf(C_E, -(x * x) / (2 * sigma * sigma));
}

/* http://theinstructionlimit.com/gaussian-blur-revisited-part-two */
static float
n_taps_to_sigma(int n_taps)
{
    static const float sigma[7] = { 1.35, 1.55, 1.8, 2.18, 2.49, 2.85, 3.66 };

    return sigma[n_taps / 2 - 2];
}

static cg_pipeline_t *
create_1d_gaussian_blur_pipeline(rut_shell_t *shell,
                                 int n_taps)
{
    static c_hash_table_t *pipeline_cache = NULL;
    cg_pipeline_t *pipeline;
    cg_snippet_t *snippet;
    c_string_t *shader;
    cg_depth_state_t depth_state;
    int i;

    /* initialize the pipeline cache. The shaders are only dependent on the
     * number of taps, not the sigma, so we cache the corresponding pipelines
     * in a hash table 'n_taps' => 'pipeline' */
    if (C_UNLIKELY(pipeline_cache == NULL)) {
        pipeline_cache = c_hash_table_new_full(c_direct_hash,
                                               c_direct_equal,
                                               NULL, /* key destroy notify */
                                               (c_destroy_func_t)cg_object_unref);
    }

    pipeline = c_hash_table_lookup(pipeline_cache, C_INT_TO_POINTER(n_taps));
    if (pipeline)
        return cg_object_ref(pipeline);

    shader = c_string_new(NULL);

    c_string_append_printf(shader,
                           "uniform vec2 pixel_step;\n"
                           "uniform float factors[%i];\n",
                           n_taps);

    snippet = cg_snippet_new(
        CG_SNIPPET_HOOK_TEXTURE_LOOKUP, shader->str, NULL /* post */);

    c_string_set_size(shader, 0);

    pipeline = cg_pipeline_new(shell->cg_device);
    cg_pipeline_set_layer_null_texture(pipeline,
                                       0, /* layer_num */
                                       CG_TEXTURE_TYPE_2D);
    cg_pipeline_set_layer_wrap_mode(pipeline,
                                    0, /* layer_num */
                                    CG_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);
    cg_pipeline_set_layer_filters(pipeline,
                                  0, /* layer_num */
                                  CG_PIPELINE_FILTER_NEAREST,
                                  CG_PIPELINE_FILTER_NEAREST);

    for (i = 0; i < n_taps; i++) {
        c_string_append(shader, "cg_texel ");

        if (i == 0)
            c_string_append(shader, "=");
        else
            c_string_append(shader, "+=");

        c_string_append_printf(shader,
                               " texture2D (cg_sampler, "
                               "cg_tex_coord.st");
        if (i != (n_taps - 1) / 2)
            c_string_append_printf(
                shader, " + pixel_step * %f", (float)(i - ((n_taps - 1) / 2)));
        c_string_append_printf(shader, ") * factors[%i];\n", i);
    }

    cg_snippet_set_replace(snippet, shader->str);

    c_string_free(shader, true);

    cg_pipeline_add_layer_snippet(pipeline, 0, snippet);

    cg_object_unref(snippet);

    cg_pipeline_set_blend(pipeline, "RGBA=ADD(SRC_COLOR, 0)", NULL);

    cg_depth_state_init(&depth_state);
    cg_depth_state_set_write_enabled(&depth_state, false);
    cg_depth_state_set_test_enabled(&depth_state, false);
    cg_pipeline_set_depth_state(pipeline, &depth_state, NULL);

    c_hash_table_insert(pipeline_cache, C_INT_TO_POINTER(n_taps), pipeline);

    return pipeline;
}

static void
set_blurrer_pipeline_factors(cg_pipeline_t *pipeline, int n_taps)
{
    int i, radius;
    float *factors, sigma;
    int location;
    float sum;
    float scale;

    radius = n_taps / 2; /* which is (n_taps - 1) / 2 as well */

    factors = c_alloca(n_taps * sizeof(float));
    sigma = n_taps_to_sigma(n_taps);

    sum = 0;
    for (i = -radius; i <= radius; i++) {
        factors[i + radius] = gaussian(sigma, i);
        sum += factors[i + radius];
    }

    /* So that we don't loose any brightness when blurring, we
     * normalized the factors... */
    scale = 1.0 / sum;
    for (i = -radius; i <= radius; i++)
        factors[i + radius] *= scale;

    location = cg_pipeline_get_uniform_location(pipeline, "factors");
    cg_pipeline_set_uniform_float(
        pipeline, location, 1 /* n_components */, n_taps /* count */, factors);
}

static void
set_blurrer_pipeline_texture(cg_pipeline_t *pipeline,
                             cg_texture_t *source,
                             float x_pixel_step,
                             float y_pixel_step)
{
    float pixel_step[2];
    int pixel_step_location;

    /* our input in the source texture */
    cg_pipeline_set_layer_texture(pipeline,
                                  0, /* layer_num */
                                  source);

    pixel_step[0] = x_pixel_step;
    pixel_step[1] = y_pixel_step;
    pixel_step_location =
        cg_pipeline_get_uniform_location(pipeline, "pixel_step");
    c_assert(pixel_step_location);
    cg_pipeline_set_uniform_float(pipeline,
                                  pixel_step_location,
                                  2, /* n_components */
                                  1, /* count */
                                  pixel_step);
}

rut_gaussian_blurrer_t *
rut_gaussian_blurrer_new(rut_shell_t *shell, int n_taps)
{
    rut_gaussian_blurrer_t *blurrer = c_slice_new0(rut_gaussian_blurrer_t);
    cg_pipeline_t *base_pipeline;

    /* validation */
    if (n_taps < 5 || n_taps > 17 || n_taps % 2 == 0) {
        c_critical("blur: the numbers of taps must belong to the {5, 7, 9, "
                   "11, 13, 14, 17, 19} set");
        c_assert_not_reached();
        return NULL;
    }

    blurrer->shell = shell;
    blurrer->n_taps = n_taps;

    base_pipeline = create_1d_gaussian_blur_pipeline(shell, n_taps);

    blurrer->x_pass_pipeline = cg_pipeline_copy(base_pipeline);
    set_blurrer_pipeline_factors(blurrer->x_pass_pipeline, n_taps);
    blurrer->y_pass_pipeline = cg_pipeline_copy(base_pipeline);
    set_blurrer_pipeline_factors(blurrer->x_pass_pipeline, n_taps);

    cg_object_unref(base_pipeline);

    return blurrer;
}

static void
_rut_gaussian_blurrer_free_buffers(rut_gaussian_blurrer_t *blurrer)
{
    if (blurrer->x_pass) {
        cg_object_unref(blurrer->x_pass);
        blurrer->x_pass = NULL;
    }
    if (blurrer->x_pass_fb) {
        cg_object_unref(blurrer->x_pass_fb);
        blurrer->x_pass_fb = NULL;
    }

    if (blurrer->y_pass) {
        cg_object_unref(blurrer->y_pass);
        blurrer->y_pass = NULL;
    }
    if (blurrer->y_pass_fb) {
        cg_object_unref(blurrer->y_pass_fb);
        blurrer->y_pass_fb = NULL;
    }
}

void
rut_gaussian_blurrer_free(rut_gaussian_blurrer_t *blurrer)
{
    _rut_gaussian_blurrer_free_buffers(blurrer);
    c_slice_free(rut_gaussian_blurrer_t, blurrer);
}

cg_texture_t *
rut_gaussian_blurrer_blur(rut_gaussian_blurrer_t *blurrer,
                          cg_texture_t *source)
{
    int src_w, src_h;
    cg_texture_components_t components;
    cg_offscreen_t *offscreen;

    /* create the first FBO to render the x pass */
    src_w = cg_texture_get_width(source);
    src_h = cg_texture_get_height(source);
    components = cg_texture_get_components(source);

    if (blurrer->width != src_w || blurrer->height != src_h ||
        blurrer->components != components) {
        _rut_gaussian_blurrer_free_buffers(blurrer);
    }

    if (!blurrer->x_pass) {
        cg_error_t *error = NULL;
        cg_texture_2d_t *texture_2d =
            cg_texture_2d_new_with_size(blurrer->shell->cg_device, src_w, src_h);

        cg_texture_set_components(texture_2d, components);

        cg_texture_allocate(texture_2d, &error);
        if (error) {
            c_warning("blurrer: could not create x pass texture: %s",
                      error->message);
        }
        blurrer->x_pass = texture_2d;
        blurrer->width = src_w;
        blurrer->height = src_h;
        blurrer->components = components;

        offscreen = cg_offscreen_new_with_texture(blurrer->x_pass);
        blurrer->x_pass_fb = offscreen;
        cg_framebuffer_orthographic(
            blurrer->x_pass_fb, 0, 0, src_w, src_h, -1, 100);
    }

    if (!blurrer->y_pass) {
        /* create the second FBO (final destination) to render the y pass */
        cg_texture_2d_t *texture_2d =
            cg_texture_2d_new_with_size(blurrer->shell->cg_device, src_w, src_h);

        cg_texture_set_components(texture_2d, components);

        blurrer->destination = texture_2d;
        blurrer->y_pass = blurrer->destination;

        offscreen = cg_offscreen_new_with_texture(blurrer->destination);
        blurrer->y_pass_fb = offscreen;
        cg_framebuffer_orthographic(
            blurrer->y_pass_fb, 0, 0, src_w, src_h, -1, 100);
    }

    set_blurrer_pipeline_texture(
        blurrer->x_pass_pipeline, source, 1.0f / src_w, 0);
    set_blurrer_pipeline_texture(
        blurrer->y_pass_pipeline, blurrer->x_pass, 0, 1.0f / src_h);

    /* x pass */
    cg_framebuffer_draw_rectangle(blurrer->x_pass_fb,
                                  blurrer->x_pass_pipeline,
                                  0,
                                  0,
                                  blurrer->width,
                                  blurrer->height);

    /* y pass */
    cg_framebuffer_draw_rectangle(blurrer->y_pass_fb,
                                  blurrer->y_pass_pipeline,
                                  0,
                                  0,
                                  blurrer->width,
                                  blurrer->height);

    return cg_object_ref(blurrer->destination);
}
