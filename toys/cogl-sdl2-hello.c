#include <config.h>

#include <cglib/cglib.h>
#include <cogl/cogl-sdl.h>
#include <stdio.h>
#include <SDL.h>

/* This short example is just to demonstrate mixing SDL with Cogl as a
   simple way to get portable support for events */

typedef struct Data {
    cg_primitive_t *triangle;
    cg_pipeline_t *pipeline;
    float center_x, center_y;
    cg_framebuffer_t *fb;
    bool quit;
    bool redraw_queued;
    bool ready_to_draw;
} Data;

static void
redraw(Data *data)
{
    cg_framebuffer_t *fb = data->fb;

    cg_framebuffer_clear4f(fb, CG_BUFFER_BIT_COLOR, 0, 0, 0, 1);

    cg_framebuffer_push_matrix(fb);
    cg_framebuffer_translate(fb, data->center_x, -data->center_y, 0.0f);

    cg_primitive_draw(data->triangle, fb, data->pipeline);
    cg_framebuffer_pop_matrix(fb);

    cg_onscreen_swap_buffers(CG_ONSCREEN(fb));
}

static void
dirty_cb(cg_onscreen_t *onscreen,
         const cg_onscreen_dirty_info_t *info,
         void *user_data)
{
    Data *data = user_data;

    data->redraw_queued = true;
}

static void
handle_event(Data *data, SDL_Event *event)
{
    switch (event->type) {
    case SDL_WINDOWEVENT:
        switch (event->window.event) {
        case SDL_WINDOWEVENT_CLOSE:
            data->quit = true;
            break;
        }
        break;

    case SDL_MOUSEMOTION: {
        int width = cg_framebuffer_get_width(data->fb);
        int height = cg_framebuffer_get_height(data->fb);

        data->center_x = event->motion.x * 2.0f / width - 1.0f;
        data->center_y = event->motion.y * 2.0f / height - 1.0f;

        data->redraw_queued = true;
    } break;

    case SDL_QUIT:
        data->quit = true;
        break;
    }
}

static void
frame_cb(cg_onscreen_t *onscreen,
         cg_frame_event_t event,
         cg_frame_info_t *info,
         void *user_data)
{
    Data *data = user_data;

    if (event == CG_FRAME_EVENT_SYNC)
        data->ready_to_draw = true;
}

int
main(int argc, char **argv)
{
    cg_renderer_t *renderer;
    cg_device_t *dev;
    cg_onscreen_t *onscreen;
    cg_error_t *error = NULL;
    cg_vertex_p2c4_t triangle_vertices[] = {
        { 0, 0.7, 0xff, 0x00, 0x00, 0xff },
        { -0.7, -0.7, 0x00, 0xff, 0x00, 0xff },
        { 0.7, -0.7, 0x00, 0x00, 0xff, 0xff }
    };
    Data data;
    SDL_Event event;

    renderer = cg_renderer_new();
    dev = cg_device_new();

    cg_renderer_set_winsys_id(renderer, CG_WINSYS_ID_SDL);
    if (cg_renderer_connect(renderer, &error))
        cg_device_set_renderer(dev, renderer);
    else {
        cg_error_free(error);
        fprintf(stderr, "Failed to create device: %s\n", error->message);
        return 1;
    }

    onscreen = cg_onscreen_new(dev, 800, 600);
    data.fb = onscreen;

    cg_onscreen_add_frame_callback(
        onscreen, frame_cb, &data, NULL /* destroy callback */);
    cg_onscreen_add_dirty_callback(
        onscreen, dirty_cb, &data, NULL /* destroy callback */);

    data.center_x = 0.0f;
    data.center_y = 0.0f;
    data.quit = false;

    /* In SDL2, setting resizable only works before allocating the
     * onscreen */
    cg_onscreen_set_resizable(onscreen, true);

    cg_onscreen_show(onscreen);

    data.triangle = cg_primitive_new_p2c4(dev, CG_VERTICES_MODE_TRIANGLES, 3,
                                          triangle_vertices);
    data.pipeline = cg_pipeline_new(dev);

    data.redraw_queued = false;
    data.ready_to_draw = true;

    while (!data.quit) {
        if (!SDL_PollEvent(&event)) {
            if (data.redraw_queued && data.ready_to_draw) {
                redraw(&data);
                data.redraw_queued = false;
                data.ready_to_draw = false;
                continue;
            }

            cg_sdl_idle(dev);
            if (!SDL_WaitEvent(&event)) {
                fprintf(stderr, "Error waiting for SDL events");
                return 1;
            }
        }

        handle_event(&data, &event);
        cg_sdl_handle_event(dev, &event);
    }

    cg_object_unref(dev);

    return 0;
}
