#include <config.h>

#include <cglib/cglib.h>
#include <string.h>

#include "test-cg-fixtures.h"

#define TEX_SIZE 4

typedef struct _test_state_t
{
    int width;
    int height;
    cg_texture_t *texture;
} test_state_t;

static cg_texture_t *
create_texture(test_cg_texture_flag_t flags)
{
    uint8_t *data = c_malloc(TEX_SIZE * TEX_SIZE * 4), *p = data;
    cg_texture_t *tex;
    int x, y;

    for (y = 0; y < TEX_SIZE; y++)
        for (x = 0; x < TEX_SIZE; x++) {
            *(p++) = 0;
            *(p++) = (x & 1) * 255;
            *(p++) = (y & 1) * 255;
            *(p++) = 255;
        }

    tex = test_cg_texture_new_from_data(test_dev,
                                        TEX_SIZE, TEX_SIZE, flags,
                                        CG_PIXEL_FORMAT_RGBA_8888_PRE,
                                        TEX_SIZE * 4,
                                        data);
    c_free(data);

    return tex;
}

static cg_pipeline_t *
create_pipeline(test_state_t *state,
                cg_pipeline_wrap_mode_t wrap_mode_s,
                cg_pipeline_wrap_mode_t wrap_mode_t)
{
    cg_pipeline_t *pipeline;

    pipeline = cg_pipeline_new(test_dev);
    cg_pipeline_set_layer_texture(pipeline, 0, state->texture);
    cg_pipeline_set_layer_filters(pipeline, 0,
                                  CG_PIPELINE_FILTER_NEAREST,
                                  CG_PIPELINE_FILTER_NEAREST);
    cg_pipeline_set_layer_wrap_mode_s(pipeline, 0, wrap_mode_s);
    cg_pipeline_set_layer_wrap_mode_t(pipeline, 0, wrap_mode_t);

    return pipeline;
}

static cg_pipeline_wrap_mode_t
wrap_modes[] = {
    CG_PIPELINE_WRAP_MODE_REPEAT,
    CG_PIPELINE_WRAP_MODE_REPEAT,

    CG_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE,
    CG_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE,

    CG_PIPELINE_WRAP_MODE_REPEAT,
    CG_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE,

    CG_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE,
    CG_PIPELINE_WRAP_MODE_REPEAT,
};

static void
draw_tests (test_state_t *state)
{
    int i;

    for (i = 0; i < C_N_ELEMENTS (wrap_modes); i += 2) {
        cg_pipeline_wrap_mode_t wrap_mode_s, wrap_mode_t;
        cg_pipeline_t *pipeline;

        /* Create a separate pipeline for each pair of wrap modes so
           that we can verify whether the batch splitting works */
        wrap_mode_s = wrap_modes[i];
        wrap_mode_t = wrap_modes[i + 1];
        pipeline = create_pipeline(state, wrap_mode_s, wrap_mode_t);
        /* Render the pipeline at four times the size of the texture */
        cg_framebuffer_draw_textured_rectangle(test_fb,
                                               pipeline,
                                               i * TEX_SIZE,
                                               0,
                                               (i + 2) * TEX_SIZE,
                                               TEX_SIZE * 2,
                                               0, 0, 2, 2);
        cg_object_unref(pipeline);
    }
}

static void
validate_set (test_state_t *state, int offset)
{
    uint8_t data[TEX_SIZE * 2 * TEX_SIZE * 2 * 4], *p;
    int x, y, i;

    for (i = 0; i < C_N_ELEMENTS (wrap_modes); i += 2) {
        cg_pipeline_wrap_mode_t wrap_mode_s, wrap_mode_t;

        wrap_mode_s = wrap_modes[i];
        wrap_mode_t = wrap_modes[i + 1];

        cg_framebuffer_read_pixels(test_fb, i * TEX_SIZE, offset * TEX_SIZE * 2,
                                   TEX_SIZE * 2, TEX_SIZE * 2,
                                   CG_PIXEL_FORMAT_RGBA_8888,
                                   data);

        p = data;

        for (y = 0; y < TEX_SIZE * 2; y++)
            for (x = 0; x < TEX_SIZE * 2; x++) {
                uint8_t green, blue;

                if (x < TEX_SIZE || wrap_mode_s == CG_PIPELINE_WRAP_MODE_REPEAT)
                    green = (x & 1) * 255;
                else
                    green = ((TEX_SIZE - 1) & 1) * 255;

                if (y < TEX_SIZE || wrap_mode_t == CG_PIPELINE_WRAP_MODE_REPEAT)
                    blue = (y & 1) * 255;
                else
                    blue = ((TEX_SIZE - 1) & 1) * 255;

                c_assert_cmpint(p[0], ==, 0);
                c_assert_cmpint(p[1], ==, green);
                c_assert_cmpint(p[2], ==, blue);

                p += 4;
            }
    }
}

static void
validate_result(test_state_t *state)
{
    validate_set(state, 0); /* non-atlased rectangle */
#if 0 /* this doesn't currently work */
    validate_set (state, 1); /* atlased rectangle */
#endif
}

static void
paint(test_state_t *state)
{
    /* Draw the tests first with a non atlased texture */
    state->texture = create_texture(TEST_CG_TEXTURE_NO_ATLAS);
    draw_tests(state);
    cg_object_unref(state->texture);

    /* Draw the tests again with a possible atlased texture. This should
       end up testing software repeats */
    state->texture = create_texture(TEST_CG_TEXTURE_NONE);
    cg_framebuffer_push_matrix(test_fb);
    cg_framebuffer_translate(test_fb, 0.0f, TEX_SIZE * 2.0f, 0.0f);
    draw_tests(state);
    cg_framebuffer_pop_matrix(test_fb);
    cg_object_unref(state->texture);

    validate_result(state);
}

void
test_wrap_modes (void)
{
    test_state_t state;

    state.width = cg_framebuffer_get_width(test_fb);
    state.height = cg_framebuffer_get_height(test_fb);

    cg_framebuffer_orthographic(test_fb,
                                0, 0,
                                state.width,
                                state.height,
                                -1,
                                100);

    paint(&state);

    if (test_verbose())
        c_print ("OK\n");
}
