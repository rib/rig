/*
 * Rig
 *
 * UI Engine & Editor
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

#include <rig-config.h>

#include <math.h>

#include <rut.h>

#include "rig-dof-effect.h"
#include "rig-downsampler.h"
#include "rig-engine.h"

struct _rig_depth_of_field_t {
    rig_engine_t *engine;

    /* The size of our depth_pass and normal_pass textuers */
    int width;
    int height;

    /* A texture to hold depth-of-field blend factors based
     * on the distance of the geometry from the focal plane.
     */
    cg_texture_t *depth_pass;
    cg_framebuffer_t *depth_pass_fb;

    /* This is our normal, pristine render of the color buffer */
    cg_texture_t *color_pass;
    cg_framebuffer_t *color_pass_fb;

    /* This is our color buffer reduced in size and blurred */
    cg_texture_t *blur_pass;

    cg_pipeline_t *pipeline;

    rig_downsampler_t *downsampler;
    rut_gaussian_blurrer_t *blurrer;
};

rig_depth_of_field_t *
rig_dof_effect_new(rig_engine_t *engine)
{
    rig_depth_of_field_t *dof = c_slice_new0(rig_depth_of_field_t);
    cg_pipeline_t *pipeline;
    cg_snippet_t *snippet;

    dof->engine = engine;

    pipeline = cg_pipeline_new(engine->shell->cg_device);
    dof->pipeline = pipeline;

    cg_pipeline_set_layer_texture(pipeline, 0, NULL); /* depth */
    cg_pipeline_set_layer_texture(pipeline, 1, NULL); /* blurred */
    cg_pipeline_set_layer_texture(pipeline, 2, NULL); /* color */

    /* disable blending */
    cg_pipeline_set_blend(pipeline, "RGBA=ADD(SRC_COLOR, 0)", NULL);

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT,
                             NULL, /* definitions */
                             NULL /* post */);

    cg_snippet_set_replace(
        snippet,
        "#if __VERSION__ >= 130\n"
        "cg_texel0 = texture (cg_sampler0, cg_tex_coord0_in.st);\n"
        "cg_texel1 = texture (cg_sampler1, cg_tex_coord1_in.st);\n"
        "cg_texel2 = texture (cg_sampler2, cg_tex_coord2_in.st);\n"
        "#else\n"
        "cg_texel0 = texture2D (cg_sampler0, cg_tex_coord0_in.st);\n"
        "cg_texel1 = texture2D (cg_sampler1, cg_tex_coord1_in.st);\n"
        "cg_texel2 = texture2D (cg_sampler2, cg_tex_coord2_in.st);\n"
        "#endif\n"
        "cg_color_out = mix (cg_texel1, cg_texel2, cg_texel0.a);\n"
        "cg_color_out.a = 1.0;\n");

    cg_pipeline_add_snippet(pipeline, snippet);
    cg_object_unref(snippet);

    dof->downsampler = rig_downsampler_new(engine);
    dof->blurrer = rut_gaussian_blurrer_new(engine->shell, 7);

    return dof;
}

void
rig_dof_effect_free(rig_depth_of_field_t *dof)
{
    rig_downsampler_free(dof->downsampler);
    rut_gaussian_blurrer_free(dof->blurrer);
    cg_object_unref(dof->pipeline);

    c_slice_free(rig_depth_of_field_t, dof);
}

void
rig_dof_effect_set_framebuffer_size(rig_depth_of_field_t *dof,
                                    int width,
                                    int height)
{
    if (dof->width == width && dof->height == height)
        return;

    if (dof->color_pass_fb) {
        cg_object_unref(dof->color_pass_fb);
        dof->color_pass_fb = NULL;
        cg_object_unref(dof->color_pass);
        dof->color_pass = NULL;
    }

    if (dof->depth_pass_fb) {
        cg_object_unref(dof->depth_pass_fb);
        dof->depth_pass_fb = NULL;
        cg_object_unref(dof->depth_pass);
        dof->depth_pass = NULL;
    }

    dof->width = width;
    dof->height = height;
}

cg_framebuffer_t *
rig_dof_effect_get_depth_pass_fb(rig_depth_of_field_t *dof)
{
    if (!dof->depth_pass) {
        /*
         * Offscreen render for post-processing
         */
        dof->depth_pass = cg_texture_2d_new_with_size(
            dof->engine->shell->cg_device, dof->width, dof->height);

        dof->depth_pass_fb = cg_offscreen_new_with_texture(dof->depth_pass);
    }

    return dof->depth_pass_fb;
}

cg_framebuffer_t *
rig_dof_effect_get_color_pass_fb(rig_depth_of_field_t *dof)
{
    if (!dof->color_pass) {
        /*
         * Offscreen render for post-processing
         */
        dof->color_pass = cg_texture_2d_new_with_size(
            dof->engine->shell->cg_device, dof->width, dof->height);

        dof->color_pass_fb = cg_offscreen_new_with_texture(dof->color_pass);
    }

    return dof->color_pass_fb;
}

void
rig_dof_effect_draw_rectangle(rig_depth_of_field_t *dof,
                              cg_framebuffer_t *fb,
                              float x1,
                              float y1,
                              float x2,
                              float y2)
{
    cg_texture_t *downsampled =
        rig_downsampler_downsample(dof->downsampler, dof->color_pass, 4, 4);

    cg_texture_t *blurred =
        rut_gaussian_blurrer_blur(dof->blurrer, downsampled);

    cg_pipeline_t *pipeline = cg_pipeline_copy(dof->pipeline);

    cg_pipeline_set_layer_texture(pipeline, 0, dof->depth_pass);
    cg_pipeline_set_layer_texture(pipeline, 1, blurred);
    cg_pipeline_set_layer_texture(pipeline, 2, dof->color_pass);

    cg_framebuffer_draw_rectangle(fb, pipeline, x1, y1, x2, y2);

    cg_object_unref(blurred);
    cg_object_unref(downsampled);
}
