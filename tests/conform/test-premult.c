#include <config.h>

#include <cglib/cglib.h>

#include <string.h>

#include "test-cg-fixtures.h"

#define QUAD_WIDTH 32

#define RED 0
#define GREEN 1
#define BLUE 2
#define ALPHA 3

#define MASK_RED(COLOR)   ((COLOR & 0xff000000) >> 24)
#define MASK_GREEN(COLOR) ((COLOR & 0xff0000) >> 16)
#define MASK_BLUE(COLOR)  ((COLOR & 0xff00) >> 8)
#define MASK_ALPHA(COLOR) (COLOR & 0xff)

typedef enum _make_texture_flags_t {
    TEXTURE_FLAG_SET_PREMULTIPLIED = 1,
    TEXTURE_FLAG_SET_UNPREMULTIPLIED = 1<<1,
} make_texture_flags_t;

static uint8_t *
gen_tex_data(uint32_t color)
{
    uint8_t *tex_data, *p;
    uint8_t r = MASK_RED(color);
    uint8_t g = MASK_GREEN(color);
    uint8_t b = MASK_BLUE(color);
    uint8_t a = MASK_ALPHA(color);

    tex_data = c_malloc(QUAD_WIDTH * QUAD_WIDTH * 4);

    for(p = tex_data + QUAD_WIDTH * QUAD_WIDTH * 4; p > tex_data; ) {
        *(--p) = a;
        *(--p) = b;
        *(--p) = g;
        *(--p) = r;
    }

    return tex_data;
}

static cg_texture_t *
make_texture(uint32_t color,
             cg_pixel_format_t src_format,
             make_texture_flags_t flags)
{
    cg_texture_2d_t *tex_2d;
    uint8_t *tex_data = gen_tex_data(color);
    cg_bitmap_t *bmp = cg_bitmap_new_for_data(test_dev,
                                              QUAD_WIDTH,
                                              QUAD_WIDTH,
                                              src_format,
                                              QUAD_WIDTH * 4,
                                              tex_data);

    tex_2d = cg_texture_2d_new_from_bitmap(bmp);

    if(flags & TEXTURE_FLAG_SET_PREMULTIPLIED)
        cg_texture_set_premultiplied(tex_2d, true);
    else if(flags & TEXTURE_FLAG_SET_UNPREMULTIPLIED)
        cg_texture_set_premultiplied(tex_2d, false);

    cg_object_unref(bmp);
    c_free(tex_data);

    return tex_2d;
}

static void
set_region(cg_texture_t *tex,
           uint32_t color,
           cg_pixel_format_t format)
{
    uint8_t *tex_data = gen_tex_data(color);

    cg_texture_set_region(tex,
                          QUAD_WIDTH, QUAD_WIDTH, /* height */
                          format,
                          0, /* auto compute row stride */
                          tex_data,
                          0, 0, /* dst x,y */
                          0, /* level */
                          NULL); /* don't catch errors */
}

static void
check_texture(cg_pipeline_t *pipeline,
              int x,
              int y,
              cg_texture_t *tex,
              uint32_t expected_result)
{
    cg_pipeline_set_layer_texture(pipeline, 0, tex);
    cg_framebuffer_draw_rectangle(test_fb, pipeline,
                                  x * QUAD_WIDTH,
                                  y * QUAD_WIDTH,
                                  x * QUAD_WIDTH + QUAD_WIDTH,
                                  y * QUAD_WIDTH + QUAD_WIDTH);
    test_cg_check_pixel(test_fb,
                        x * QUAD_WIDTH + QUAD_WIDTH / 2,
                        y * QUAD_WIDTH + QUAD_WIDTH / 2,
                        expected_result);
}

void
test_premult(void)
{
    cg_pipeline_t *pipeline;
    cg_texture_t *tex;
    cg_snippet_t *snippet;

    cg_framebuffer_orthographic(test_fb, 0, 0,
                                cg_framebuffer_get_width(test_fb),
                                cg_framebuffer_get_height(test_fb),
                                -1,
                                100);

    cg_framebuffer_clear4f(test_fb,
                           CG_BUFFER_BIT_COLOR,
                           1.0f, 1.0f, 1.0f, 1.0f);

    pipeline = cg_pipeline_new(test_dev);
    cg_pipeline_set_blend(pipeline, "RGBA = ADD(SRC_COLOR, 0)", NULL);

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_LAYER_FRAGMENT, NULL, NULL);
    cg_snippet_set_replace(snippet, "frag = cg_texel0;\n");
    cg_pipeline_add_layer_snippet(pipeline, 0, snippet);
    cg_object_unref(snippet);

    /* If the user explicitly specifies an unmultiplied internal format then
     * Cogl shouldn't automatically premultiply the given texture data... */
    if(test_verbose())
        c_print("make_texture(0xff00ff80, "
                "src = RGBA_8888, internal = RGBA_8888)\n");
    tex = make_texture(0xff00ff80,
                       CG_PIXEL_FORMAT_RGBA_8888, /* src format */
                       TEXTURE_FLAG_SET_UNPREMULTIPLIED);
    check_texture(pipeline, 0, 0, /* position */
                  tex,
                  0xff00ff80); /* expected */

    /* If the user explicitly requests a premultiplied internal format and
     * gives unmultiplied src data then Cogl should always premultiply that
     * for us */
    if(test_verbose())
        c_print("make_texture(0xff00ff80, "
                "src = RGBA_8888, internal = RGBA_8888_PRE)\n");
    tex = make_texture(0xff00ff80,
                       CG_PIXEL_FORMAT_RGBA_8888, /* src format */
                       TEXTURE_FLAG_SET_PREMULTIPLIED);
    check_texture(pipeline, 1, 0, /* position */
                  tex,
                  0x80008080); /* expected */

    /* If the user doesn't explicitly declare that the texture is premultiplied
     * then Cogl should assume it is by default should premultiply
     * unpremultiplied texture data...
     */
    if(test_verbose())
        c_print("make_texture(0xff00ff80, "
                "src = RGBA_8888, internal = ANY)\n");
    tex = make_texture(0xff00ff80,
                       CG_PIXEL_FORMAT_RGBA_8888, /* src format */
                       0); /* default premultiplied status */
    check_texture(pipeline, 2, 0, /* position */
                  tex,
                  0x80008080); /* expected */

    /* If the user requests a premultiplied internal texture format and supplies
     * premultiplied source data, Cogl should never modify that source data...
     */
    if(test_verbose())
        c_print("make_texture(0x80008080, "
                "src = RGBA_8888_PRE, "
                "internal = RGBA_8888_PRE)\n");
    tex = make_texture(0x80008080,
                       CG_PIXEL_FORMAT_RGBA_8888_PRE, /* src format */
                       TEXTURE_FLAG_SET_PREMULTIPLIED);
    check_texture(pipeline, 3, 0, /* position */
                  tex,
                  0x80008080); /* expected */

    /* If the user requests an unmultiplied internal texture format, but
     * supplies premultiplied source data, then Cogl should always
     * un-premultiply the source data... */
    if(test_verbose())
        c_print("make_texture(0x80008080, "
                "src = RGBA_8888_PRE, internal = RGBA_8888)\n");
    tex = make_texture(0x80008080,
                       CG_PIXEL_FORMAT_RGBA_8888_PRE, /* src format */
                       TEXTURE_FLAG_SET_UNPREMULTIPLIED);
    check_texture(pipeline, 4, 0, /* position */
                  tex,
                  0xff00ff80); /* expected */

    /* If the user allows any internal texture format and provides premultipled
     * source data then by default Cogl shouldn't modify the source data...
     *(In the future there will be additional Cogl API to control this
     *  behaviour) */
    if(test_verbose())
        c_print("make_texture(0x80008080, "
                "src = RGBA_8888_PRE, internal = ANY)\n");
    tex = make_texture(0x80008080,
                       CG_PIXEL_FORMAT_RGBA_8888_PRE, /* src format */
                       0); /* default premultiplied status */
    check_texture(pipeline, 5, 0, /* position */
                  tex,
                  0x80008080); /* expected */

    /*
     * Test cg_texture_set_region() ....
     */

    if(test_verbose())
        c_print("make_texture(0xDEADBEEF, "
                "src = RGBA_8888, internal = RGBA_8888)\n");
    tex = make_texture(0xDEADBEEF,
                       CG_PIXEL_FORMAT_RGBA_8888, /* src format */
                       TEXTURE_FLAG_SET_UNPREMULTIPLIED);
    if(test_verbose())
        c_print("set_region(0xff00ff80, RGBA_8888)\n");
    set_region(tex, 0xff00ff80, CG_PIXEL_FORMAT_RGBA_8888);
    check_texture(pipeline, 6, 0, /* position */
                  tex,
                  0xff00ff80); /* expected */

    /* Updating a texture region for an unmultiplied texture using premultiplied
     * region data should result in Cogl unmultiplying the given region data...
     */
    if(test_verbose())
        c_print("make_texture(0xDEADBEEF, "
                "src = RGBA_8888, internal = RGBA_8888)\n");
    tex = make_texture(0xDEADBEEF,
                       CG_PIXEL_FORMAT_RGBA_8888, /* src format */
                       TEXTURE_FLAG_SET_UNPREMULTIPLIED);
    if(test_verbose())
        c_print("set_region(0x80008080, RGBA_8888_PRE)\n");
    set_region(tex, 0x80008080, CG_PIXEL_FORMAT_RGBA_8888_PRE);
    check_texture(pipeline, 7, 0, /* position */
                  tex,
                  0xff00ff80); /* expected */


    if(test_verbose())
        c_print("make_texture(0xDEADBEEF, "
                "src = RGBA_8888_PRE, "
                "internal = RGBA_8888_PRE)\n");
    tex = make_texture(0xDEADBEEF,
                       CG_PIXEL_FORMAT_RGBA_8888_PRE, /* src format */
                       TEXTURE_FLAG_SET_PREMULTIPLIED);
    if(test_verbose())
        c_print("set_region(0x80008080, RGBA_8888_PRE)\n");
    set_region(tex, 0x80008080, CG_PIXEL_FORMAT_RGBA_8888_PRE);
    check_texture(pipeline, 8, 0, /* position */
                  tex,
                  0x80008080); /* expected */


    /* Updating a texture region for a premultiplied texture using unmultiplied
     * region data should result in Cogl premultiplying the given region data...
     */
    if(test_verbose())
        c_print("make_texture(0xDEADBEEF, "
                "src = RGBA_8888_PRE, "
                "internal = RGBA_8888_PRE)\n");
    tex = make_texture(0xDEADBEEF,
                       CG_PIXEL_FORMAT_RGBA_8888_PRE, /* src format */
                       TEXTURE_FLAG_SET_PREMULTIPLIED);
    if(test_verbose())
        c_print("set_region(0xff00ff80, RGBA_8888)\n");
    set_region(tex, 0xff00ff80, CG_PIXEL_FORMAT_RGBA_8888);
    check_texture(pipeline, 9, 0, /* position */
                  tex,
                  0x80008080); /* expected */


    if(test_verbose())
        c_print("OK\n");
}

