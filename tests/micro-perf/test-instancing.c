#include <config.h>

#include <clib.h>
#include <cglib/cglib.h>
#include <math.h>

#include <clib.h>
#include <uv.h>

#include <cglib/cg-profile.h>

#define FRAMEBUFFER_WIDTH 800
#define FRAMEBUFFER_HEIGHT 500

#define RECT_WIDTH 5
#define RECT_HEIGHT 5

#define MAX_RECTS ((FRAMEBUFFER_WIDTH / RECT_WIDTH) * (FRAMEBUFFER_HEIGHT / RECT_HEIGHT))

enum attrib_id {
    ATTRIB_POS,
    ATTRIB_MV_ROW0,
    ATTRIB_MV_ROW1,
    ATTRIB_MV_ROW2,
    ATTRIB_MV_ROW3,
    ATTRIB_COLOR,
    N_ATTRIBS
};

typedef struct _Data
{
    cg_device_t *dev;
    cg_framebuffer_t *fb;
    cg_pipeline_t *pipeline;
    cg_pipeline_t *alpha_pipeline;
    cg_attribute_buffer_t *attrib_buffer;
    cg_attribute_t *attribs[N_ATTRIBS];
    cg_primitive_t *rect_prim;
    c_timer_t *timer;
    int frame;
} Data;

struct rect_attribs {
    float col0[3];
    float col1[3];
    float col2[3];
    float col3[3];
    uint8_t color[4];
} attrib_data[MAX_RECTS];

static uv_idle_t idle_handle;

static void
test_rectangles(Data *data)
{
    int x;
    int y;
    int n_rects;

    cg_framebuffer_clear4f(data->fb, CG_BUFFER_BIT_COLOR, 1, 1, 1, 1);

    cg_framebuffer_push_rectangle_clip(data->fb,
                                       10,
                                       10,
                                       FRAMEBUFFER_WIDTH - 10,
                                       FRAMEBUFFER_HEIGHT - 10);

    n_rects = 0;
    for (y = 0; y < FRAMEBUFFER_HEIGHT; y += RECT_HEIGHT) {
        for (x = 0; x < FRAMEBUFFER_WIDTH; x += RECT_WIDTH) {
            c_matrix_t mv;
            struct rect_attribs *attrib = &attrib_data[n_rects++];

            cg_framebuffer_push_matrix(data->fb);
            cg_framebuffer_translate(data->fb, x, y, 0);
            cg_framebuffer_rotate(data->fb, 45, 0, 0, 1);

            cg_framebuffer_get_modelview_matrix(data->fb, &mv);

            cg_framebuffer_pop_matrix(data->fb);

            attrib->col0[0] = mv.xx;
            attrib->col0[1] = mv.yx;
            attrib->col0[2] = mv.zx;

            attrib->col1[0] = mv.xy;
            attrib->col1[1] = mv.yy;
            attrib->col1[2] = mv.zy;

            attrib->col2[0] = mv.xz;
            attrib->col2[1] = mv.yz;
            attrib->col2[2] = mv.zz;

            attrib->col3[0] = mv.xw;
            attrib->col3[1] = mv.yw;
            attrib->col3[2] = mv.zw;

            attrib->color[0] = 0xff;
            attrib->color[1] = (255 * y/FRAMEBUFFER_WIDTH);
            attrib->color[2] = (255 * x/FRAMEBUFFER_WIDTH);
            attrib->color[3] = 0xff;
        }
    }

    c_warn_if_fail(n_rects <= MAX_RECTS);

    cg_buffer_set_data(data->attrib_buffer, 0, attrib_data, sizeof(attrib_data), NULL);
    cg_primitive_draw_instances(data->rect_prim, data->fb, data->pipeline, n_rects);

    n_rects = 0;
    for (y = 0; y < FRAMEBUFFER_HEIGHT; y += RECT_HEIGHT) {
        for (x = 0; x < FRAMEBUFFER_WIDTH; x += RECT_WIDTH) {
            struct rect_attribs *attrib = &attrib_data[n_rects++];
            c_matrix_t mv;

            cg_framebuffer_push_matrix(data->fb);
            cg_framebuffer_translate(data->fb, x, y, 0);

            cg_framebuffer_get_modelview_matrix(data->fb, &mv);

            cg_framebuffer_pop_matrix(data->fb);

            attrib->col0[0] = mv.xx;
            attrib->col0[1] = mv.yx;
            attrib->col0[2] = mv.zx;

            attrib->col1[0] = mv.xy;
            attrib->col1[1] = mv.yy;
            attrib->col1[2] = mv.zy;

            attrib->col2[0] = mv.xz;
            attrib->col2[1] = mv.yz;
            attrib->col2[2] = mv.zz;

            attrib->col3[0] = mv.xw;
            attrib->col3[1] = mv.yw;
            attrib->col3[2] = mv.zw;

            attrib->color[0] = 0xff;
            attrib->color[1] = (255 * y/FRAMEBUFFER_WIDTH);
            attrib->color[2] = (255 * x/FRAMEBUFFER_WIDTH);
            attrib->color[3] = (255 * x/FRAMEBUFFER_WIDTH);
        }
    }

    c_warn_if_fail(n_rects <= MAX_RECTS);

    cg_buffer_set_data(data->attrib_buffer, 0, attrib_data, sizeof(attrib_data), NULL);
    cg_primitive_draw_instances(data->rect_prim, data->fb,
                                data->alpha_pipeline, n_rects);

    cg_framebuffer_pop_clip(data->fb);
}

static void
paint_cb(uv_idle_t *handle)
{
    Data *data = handle->data;
    double elapsed;

    data->frame++;

    test_rectangles(data);

    cg_onscreen_swap_buffers(data->fb);

    elapsed = c_timer_elapsed(data->timer, NULL);
    if (elapsed > 1.0) {
        c_print("fps = %f\n", data->frame / elapsed);
        c_timer_start(data->timer);
        data->frame = 0;
    }

    uv_idle_stop(handle);
}

static void
frame_event_cb(cg_onscreen_t *onscreen,
               cg_frame_event_t event,
               cg_frame_info_t *info,
               void *user_data)
{
    if (event == CG_FRAME_EVENT_SYNC)
        paint_cb(&idle_handle);
}

int
main(int argc, char **argv)
{
    Data data;
    cg_onscreen_t *onscreen;
    cg_vertex_p2_t rect_pos_attr[] = {
        { 0,          0 },
        { 0,          RECT_HEIGHT },
        { RECT_WIDTH, RECT_HEIGHT },
        { 0,          0 },
        { RECT_WIDTH, RECT_HEIGHT },
        { RECT_WIDTH, 0 }
    };
    cg_buffer_t *buf;
    cg_snippet_t *snippet;
    CG_STATIC_TIMER(mainloop_timer,
                    NULL, //no parent
                    "Mainloop",
                    "The time spent in the glib mainloop",
                    0);  // no application private data

    data.dev = cg_device_new();

    onscreen = cg_onscreen_new(data.dev,
                               FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT);
    cg_onscreen_set_swap_throttled(onscreen, false);
    cg_onscreen_show(onscreen);

    data.fb = onscreen;
    cg_framebuffer_orthographic(data.fb,
                                0, 0,
                                FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT,
                                -1,
                                100);

    data.pipeline = cg_pipeline_new(data.dev);
    cg_pipeline_set_color4f(data.pipeline, 1, 1, 1, 1);
    data.alpha_pipeline = cg_pipeline_new(data.dev);
    cg_pipeline_set_color4f(data.alpha_pipeline, 1, 1, 1, 0.5);

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_VERTEX_TRANSFORM,
                             "in vec3 mv_col0;\n"
                             "in vec3 mv_col1;\n"
                             "in vec3 mv_col2;\n"
                             "in vec3 mv_col3;\n",
                             NULL); /* pre */

    cg_snippet_set_replace(snippet,
                           "mat4x3 mv = mat4x3(mv_col0, mv_col1, mv_col2, mv_col3);\n"
                           "vec4 pos;\n"
                           "pos.xyz = mv * cg_position_in;\n"
                           "pos.w = 1.0;\n"
                           "cg_position_out = cg_projection_matrix * pos;\n"
                           );
    cg_pipeline_add_snippet(data.pipeline, snippet);

    cg_pipeline_add_snippet(data.alpha_pipeline, snippet);

    data.attrib_buffer =
        cg_attribute_buffer_new_with_size(data.dev, sizeof(attrib_data));

    data.attribs[ATTRIB_MV_ROW0] = cg_attribute_new(data.attrib_buffer,
                                                    "mv_col0",
                                                    sizeof(struct rect_attribs),
                                                    offsetof(struct rect_attribs, col0),
                                                    3, /* n components */
                                                    CG_ATTRIBUTE_TYPE_FLOAT);
    cg_attribute_set_instance_stride(data.attribs[ATTRIB_MV_ROW0], 1);

    data.attribs[ATTRIB_MV_ROW1]= cg_attribute_new(data.attrib_buffer,
                                                   "mv_col1",
                                                   sizeof(struct rect_attribs),
                                                   offsetof(struct rect_attribs, col1),
                                                   3, /* n components */
                                                   CG_ATTRIBUTE_TYPE_FLOAT);
    cg_attribute_set_instance_stride(data.attribs[ATTRIB_MV_ROW1], 1);

    data.attribs[ATTRIB_MV_ROW2] = cg_attribute_new(data.attrib_buffer,
                                                    "mv_col2",
                                                    sizeof(struct rect_attribs),
                                                    offsetof(struct rect_attribs, col2),
                                                    3, /* n components */
                                                    CG_ATTRIBUTE_TYPE_FLOAT);
    cg_attribute_set_instance_stride(data.attribs[ATTRIB_MV_ROW2], 1);

    data.attribs[ATTRIB_MV_ROW3] = cg_attribute_new(data.attrib_buffer,
                                                    "mv_col3",
                                                    sizeof(struct rect_attribs),
                                                    offsetof(struct rect_attribs, col3),
                                                    3, /* n components */
                                                    CG_ATTRIBUTE_TYPE_FLOAT);
    cg_attribute_set_instance_stride(data.attribs[ATTRIB_MV_ROW3], 1);

    data.attribs[ATTRIB_COLOR]= cg_attribute_new(data.attrib_buffer,
                                                 "cg_color_in",
                                                 sizeof(struct rect_attribs),
                                                 offsetof(struct rect_attribs, color),
                                                 4, /* n components */
                                                 CG_ATTRIBUTE_TYPE_UNSIGNED_BYTE);
    cg_attribute_set_instance_stride(data.attribs[ATTRIB_COLOR], 1);

    buf = cg_attribute_buffer_new(data.dev, sizeof(rect_pos_attr), rect_pos_attr);
    data.attribs[ATTRIB_POS] = cg_attribute_new(buf,
                                                "cg_position_in",
                                                0, /* packed */
                                                0, /* offset */
                                                2, /* n components */
                                                CG_ATTRIBUTE_TYPE_FLOAT);
    cg_object_unref(buf);

    data.rect_prim = cg_primitive_new_with_attributes(CG_VERTICES_MODE_TRIANGLES,
                                                      6, data.attribs, N_ATTRIBS);

    cg_uv_set_mainloop(data.dev, uv_default_loop());

    cg_onscreen_add_frame_callback(data.fb,
                                   frame_event_cb,
                                   &data,
                                   NULL); /* destroy notify */

    uv_idle_init(uv_default_loop(), &idle_handle);
    idle_handle.data = &data;
    uv_idle_start(&idle_handle, paint_cb);

    data.frame = 0;
    data.timer = c_timer_new();
    c_timer_start(data.timer);

    CG_TIMER_START(uprof_get_mainloop_context(), mainloop_timer);
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    CG_TIMER_STOP(uprof_get_mainloop_context(), mainloop_timer);

    return 0;
}

