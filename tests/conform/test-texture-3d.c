#include <config.h>

#include <cglib/cglib.h>
#include <string.h>

#include "test-cg-fixtures.h"

#define TEX_WIDTH       4
#define TEX_HEIGHT      8
#define TEX_DEPTH       16
/* Leave four bytes of padding between each row */
#define TEX_ROWSTRIDE       (TEX_WIDTH * 4 + 4)
/* Leave four rows of padding between each image */
#define TEX_IMAGE_STRIDE    ((TEX_HEIGHT + 4) * TEX_ROWSTRIDE)

typedef struct _test_state_t {
    int fb_width;
    int fb_height;
} test_state_t;

static cg_texture_3d_t *
create_texture_3d(cg_device_t *dev)
{
    int x, y, z;
    uint8_t *data = c_malloc(TEX_IMAGE_STRIDE * TEX_DEPTH);
    uint8_t *p = data;
    cg_texture_3d_t *tex;
    cg_error_t *error = NULL;

    for(z = 0; z < TEX_DEPTH; z++) {
        for(y = 0; y < TEX_HEIGHT; y++) {
            for(x = 0; x < TEX_WIDTH; x++) {
                /* Set red, green, blue to values based on x, y, z */
                *(p++) = 255 - x * 8;
                *(p++) = y * 8;
                *(p++) = 255 - z * 8;
                /* Fully opaque */
                *(p++) = 0xff;
            }

            /* Set the padding between rows to 0xde */
            memset(p, 0xde, TEX_ROWSTRIDE -(TEX_WIDTH * 4));
            p += TEX_ROWSTRIDE -(TEX_WIDTH * 4);
        }
        /* Set the padding between images to 0xad */
        memset(p, 0xba, TEX_IMAGE_STRIDE -(TEX_HEIGHT * TEX_ROWSTRIDE));
        p += TEX_IMAGE_STRIDE -(TEX_HEIGHT * TEX_ROWSTRIDE);
    }

    tex = cg_texture_3d_new_from_data(dev,
                                      TEX_WIDTH, TEX_HEIGHT, TEX_DEPTH,
                                      CG_PIXEL_FORMAT_RGBA_8888,
                                      TEX_ROWSTRIDE,
                                      TEX_IMAGE_STRIDE,
                                      data,
                                      &error);

    if(tex == NULL) {
        c_assert(error != NULL);
        c_warning("Failed to create 3D texture: %s", error->message);
        c_assert_not_reached();
    }

    c_free(data);

    return tex;
}

static void
draw_frame(test_state_t *state)
{
    cg_texture_t *tex = create_texture_3d(test_dev);
    cg_pipeline_t *pipeline = cg_pipeline_new(test_dev);
    typedef struct { float x, y, s, t, r; } Vert;
    cg_primitive_t *primitive;
    cg_attribute_buffer_t *attribute_buffer;
    cg_attribute_t *attributes[2];
    Vert *verts, *v;
    int i;

    cg_pipeline_set_layer_texture(pipeline, 0, tex);
    cg_object_unref(tex);
    cg_pipeline_set_layer_filters(pipeline, 0,
                                  CG_PIPELINE_FILTER_NEAREST,
                                  CG_PIPELINE_FILTER_NEAREST);

    /* Render the texture repeated horizontally twice using a regular
       cogl rectangle. This should end up with the r texture coordinates
       as zero */
    cg_framebuffer_draw_textured_rectangle(test_fb, pipeline,
                                           0.0f, 0.0f, TEX_WIDTH * 2, TEX_HEIGHT,
                                           0.0f, 0.0f, 2.0f, 1.0f);

    /* Render all of the images in the texture using coordinates from a
       cg_primitive_t */
    v = verts = c_new(Vert, 4 * TEX_DEPTH);
    for(i = 0; i < TEX_DEPTH; i++) {
        float r =(i + 0.5f) / TEX_DEPTH;

        v->x = i * TEX_WIDTH;
        v->y = TEX_HEIGHT;
        v->s = 0;
        v->t = 0;
        v->r = r;
        v++;

        v->x = i * TEX_WIDTH;
        v->y = TEX_HEIGHT * 2;
        v->s = 0;
        v->t = 1;
        v->r = r;
        v++;

        v->x = i * TEX_WIDTH + TEX_WIDTH;
        v->y = TEX_HEIGHT * 2;
        v->s = 1;
        v->t = 1;
        v->r = r;
        v++;

        v->x = i * TEX_WIDTH + TEX_WIDTH;
        v->y = TEX_HEIGHT;
        v->s = 1;
        v->t = 0;
        v->r = r;
        v++;
    }

    attribute_buffer = cg_attribute_buffer_new(test_dev,
                                               4 * TEX_DEPTH * sizeof(Vert),
                                               verts);
    attributes[0] = cg_attribute_new(attribute_buffer,
                                     "cg_position_in",
                                     sizeof(Vert),
                                     C_STRUCT_OFFSET(Vert, x),
                                     2, /* n_components */
                                     CG_ATTRIBUTE_TYPE_FLOAT);
    attributes[1] = cg_attribute_new(attribute_buffer,
                                     "cg_tex_coord_in",
                                     sizeof(Vert),
                                     C_STRUCT_OFFSET(Vert, s),
                                     3, /* n_components */
                                     CG_ATTRIBUTE_TYPE_FLOAT);
    primitive = cg_primitive_new_with_attributes(CG_VERTICES_MODE_TRIANGLES,
                                                 6 * TEX_DEPTH,
                                                 attributes,
                                                 2 /* n_attributes */);

    cg_primitive_set_indices(primitive,
                             cg_get_rectangle_indices(test_dev,
                                                      TEX_DEPTH),
                             6 * TEX_DEPTH);

    cg_primitive_draw(primitive, test_fb, pipeline);

    c_free(verts);

    cg_object_unref(primitive);
    cg_object_unref(attributes[0]);
    cg_object_unref(attributes[1]);
    cg_object_unref(attribute_buffer);
    cg_object_unref(pipeline);
}

static void
validate_block(int block_x, int block_y, int z)
{
    int x, y;

    for(y = 0; y < TEX_HEIGHT; y++)
        for(x = 0; x < TEX_WIDTH; x++)
            test_cg_check_pixel_rgb(test_fb,
                                    block_x * TEX_WIDTH + x,
                                    block_y * TEX_HEIGHT + y,
                                    255 - x * 8,
                                    y * 8,
                                    255 - z * 8);
}

static void
validate_result(void)
{
    int i;

    validate_block(0, 0, 0);

    for(i = 0; i < TEX_DEPTH; i++)
        validate_block(i, 1, i);
}

static void
test_multi_texture(test_state_t *state)
{
    cg_pipeline_t *pipeline;
    cg_texture_3d_t *tex_3d;
    cg_texture_2d_t *tex_2d;
    uint8_t tex_data[4];
    cg_snippet_t *snippet;

    cg_framebuffer_clear4f(test_fb, CG_BUFFER_BIT_COLOR, 0, 0, 0, 1);

    /* Tests a pipeline that is using multi-texturing to combine a 3D
       texture with a 2D texture. The texture from another layer is
       sampled with TEXTURE_? just to pick up a specific bug that was
       happening with the ARBfp fragend */

    pipeline = cg_pipeline_new(test_dev);

    tex_data[0] = 0xff;
    tex_data[1] = 0x00;
    tex_data[2] = 0x00;
    tex_data[3] = 0xff;
    tex_2d = cg_texture_2d_new_from_data(test_dev,
                                         1, 1, /* width/height */
                                         CG_PIXEL_FORMAT_RGBA_8888_PRE,
                                         4, /* rowstride */
                                         tex_data,
                                         NULL);
    cg_pipeline_set_layer_texture(pipeline, 0, tex_2d);

    tex_data[0] = 0x00;
    tex_data[1] = 0xff;
    tex_data[2] = 0x00;
    tex_data[3] = 0xff;
    tex_3d = cg_texture_3d_new_from_data(test_dev,
                                         1, 1, 1, /* width/height/depth */
                                         CG_PIXEL_FORMAT_RGBA_8888_PRE,
                                         4, /* rowstride */
                                         4, /* image_stride */
                                         tex_data,
                                         NULL);
    cg_pipeline_set_layer_texture(pipeline, 1, tex_3d);

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_LAYER_FRAGMENT, NULL, NULL);
    cg_snippet_set_replace(snippet, "");
    cg_pipeline_add_layer_snippet(pipeline, 0, snippet);
    cg_object_unref(snippet);

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_LAYER_FRAGMENT, NULL, NULL);
    cg_snippet_set_replace(snippet, "frag = cg_texel0 + cg_texel1;\n");
    cg_pipeline_add_layer_snippet(pipeline, 1, snippet);
    cg_object_unref(snippet);

    cg_framebuffer_draw_rectangle(test_fb, pipeline, 0, 0, 10, 10);

    test_cg_check_pixel(test_fb, 5, 5, 0xffff00ff);

    cg_object_unref(tex_2d);
    cg_object_unref(tex_3d);
    cg_object_unref(pipeline);
}

void
test_texture_3d(void)
{
    test_state_t state;

    state.fb_width = cg_framebuffer_get_width(test_fb);
    state.fb_height = cg_framebuffer_get_height(test_fb);

    cg_framebuffer_orthographic(test_fb,
                                0, 0, /* x_1, y_1 */
                                state.fb_width, /* x_2 */
                                state.fb_height /* y_2 */,
                                -1, 100 /* near/far */);

    draw_frame(&state);
    validate_result();

    test_multi_texture(&state);

    if(test_verbose())
        c_print("OK\n");
}
