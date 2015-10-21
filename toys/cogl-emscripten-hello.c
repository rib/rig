#include <config.h>

#include <stdio.h>

#include <emscripten.h>

#include <clib.h>
#include <cglib/cglib.h>

#include "emscripten-example-js.h"

struct data {
    cg_device_t *dev;
    cg_renderer_t *renderer;
    cg_framebuffer_t *fb;
    cg_primitive_t *triangle;
    cg_pipeline_t *pipeline;

    bool paint_queued;
};

static void
paint(void *user_data)
{
    struct data *data = user_data;

    data->paint_queued = false;

    cg_framebuffer_clear4f(data->fb, CG_BUFFER_BIT_COLOR, 0, 0, 0, 1);
    cg_primitive_draw(data->triangle, data->fb, data->pipeline);
    cg_onscreen_swap_buffers(data->fb);

    c_debug("paint");
}

static void
frame_event_cb(cg_onscreen_t *onscreen,
               cg_frame_event_t event,
               cg_frame_info_t *info,
               void *user_data)
{
    struct data *data = user_data;

    if (event == CG_FRAME_EVENT_SYNC) {
        data->paint_queued = true;
        emscripten_resume_main_loop();
    }
}

static void
paint_loop(void *user_data)
{
    struct data *data = user_data;

    paint(data);

    /* NB: The loop will be automatically resumed if user input is received */
    if (!data->paint_queued)
        emscripten_pause_main_loop();
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

    data.paint_queued = true;

    data.dev = cg_device_new();
    if (!cg_device_connect(data.dev, &error)) {
        cg_object_unref(data.dev);
        fprintf(stderr, "Failed to create device: %s\n", error->message);
        return 1;
    }
    data.renderer = cg_device_get_renderer(data.dev);

    onscreen = cg_onscreen_new(data.dev, 640, 480);
    cg_onscreen_show(onscreen);
    data.fb = onscreen;

    cg_onscreen_set_resizable(onscreen, true);

    data.triangle = cg_primitive_new_p2c4(
        data.dev, CG_VERTICES_MODE_TRIANGLES, 3, triangle_vertices);
    data.pipeline = cg_pipeline_new(data.dev);

    cg_onscreen_add_frame_callback(
        data.fb, frame_event_cb, &data, NULL); /* destroy notify */

    /* XXX: emscripten "main loop" is really just for driving
     * throttled, rendering based on requestAnimationFrame(). This
     * mainloop isn't event driven, it's periodic and so we aim to
     * pause the emscripten mainlooop whenever we don't have a redraw
     * queued. What we do instead is hook into the real browser
     * mainloop using this javascript binding api to add an input
     * event listener that will resume the emscripten mainloop
     * whenever input is received.
     */
    example_js_add_input_listener();
    emscripten_set_main_loop_arg(paint_loop, &data, -1, true);

    cg_object_unref(data.dev);

    return 0;
}
