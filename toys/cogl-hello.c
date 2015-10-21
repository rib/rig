#include <config.h>


#include <stdio.h>

#include <uv.h>

#include <cglib/cglib.h>
#include <clib.h>

struct data {
    cg_device_t *dev;
    uv_idle_t idle;
    cg_framebuffer_t *fb;
    cg_primitive_t *triangle;
    cg_pipeline_t *pipeline;

    bool is_dirty;
    bool draw_ready;
};

static void
paint_cb(uv_idle_t *idle)
{
    struct data *data = idle->data;

    data->is_dirty = false;
    data->draw_ready = false;

    cg_framebuffer_clear4f(data->fb, CG_BUFFER_BIT_COLOR, 0, 0, 0, 1);
    cg_primitive_draw(data->triangle, data->fb, data->pipeline);
    cg_onscreen_swap_buffers(data->fb);

    uv_idle_stop(&data->idle);
}

static void
maybe_redraw(struct data *data)
{
    if (data->is_dirty && data->draw_ready) {
        /* We'll draw on idle instead of drawing immediately so that
         * if Cogl reports multiple dirty rectangles we won't
         * redundantly draw multiple frames */
        uv_idle_start(&data->idle, paint_cb);
    }
}

static void
frame_event_cb(cg_onscreen_t *onscreen,
               cg_frame_event_t event,
               cg_frame_info_t *info,
               void *user_data)
{
    struct data *data = user_data;

    if (event == CG_FRAME_EVENT_SYNC) {
        data->draw_ready = true;
        maybe_redraw(data);
    }
}

static void
dirty_cb(cg_onscreen_t *onscreen,
         const cg_onscreen_dirty_info_t *info,
         void *user_data)
{
    struct data *data = user_data;

    data->is_dirty = true;
    maybe_redraw(data);
}

int
main(int argc, char **argv)
{
    struct data data;
    cg_onscreen_t *onscreen;
    cg_error_t *error = NULL;
    cg_vertex_p2c4_t triangle_vertices[] = {
        { 0, 0.7, 0xff, 0x00, 0x00, 0xff },
        { -0.7, -0.7, 0x00, 0xff, 0x00, 0xff },
        { 0.7, -0.7, 0x00, 0x00, 0xff, 0xff }
    };
    uv_loop_t *loop = uv_default_loop();

    data.is_dirty = false;
    data.draw_ready = true;

    data.dev = cg_device_new();
    if (!cg_device_connect(data.dev, &error)) {
        cg_object_unref(data.dev);
        fprintf(stderr, "Failed to create device: %s\n", error->message);
        return 1;
    }

    onscreen = cg_onscreen_new(data.dev, 640, 480);
    cg_onscreen_show(onscreen);
    data.fb = onscreen;

    cg_onscreen_set_resizable(onscreen, true);

    data.triangle = cg_primitive_new_p2c4(
        data.dev, CG_VERTICES_MODE_TRIANGLES, 3, triangle_vertices);
    data.pipeline = cg_pipeline_new(data.dev);

    cg_onscreen_add_frame_callback(
        data.fb, frame_event_cb, &data, NULL); /* destroy notify */
    cg_onscreen_add_dirty_callback(
        data.fb, dirty_cb, &data, NULL); /* destroy notify */

    uv_idle_init(loop, &data.idle);
    data.idle.data = &data;
    uv_idle_start(&data.idle, paint_cb);

    cg_uv_set_mainloop(data.dev, loop);
    uv_run(loop, UV_RUN_DEFAULT);

    return 0;
}
