#include <config.h>

#include <cglib/cglib.h>
#include <glib.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
    cg_onscreen_template_t *onscreen_template;
    cg_display_t *display;
    cg_device_t *dev;
    cg_onscreen_t *onscreen;
    cg_framebuffer_t *fb;
    cg_error_t *error = NULL;
    cg_vertex_p2c4_t triangle_vertices[] = {
        { 0, 0.7, 0xff, 0x00, 0x00, 0xff },
        { -0.7, -0.7, 0x00, 0xff, 0x00, 0xff },
        { 0.7, -0.7, 0x00, 0x00, 0xff, 0xff }
    };
    cg_primitive_t *triangle;
    cg_texture_t *tex;
    cg_offscreen_t *offscreen;
    cg_framebuffer_t *offscreen_fb;
    cg_pipeline_t *pipeline;

    onscreen_template = cg_onscreen_template_new();
    cg_onscreen_template_set_samples_per_pixel(onscreen_template, 4);
    display = cg_display_new(NULL, onscreen_template);

    if (!cg_display_setup(display, &error)) {
        fprintf(stderr,
                "Platform doesn't support onscreen 4x msaa rendering: %s\n",
                error->message);
        return 1;
    }

    dev = cg_device_new();
    if (!cg_device_connect(dev, &error)) {
        fprintf(stderr, "Failed to create context: %s\n", error->message);
        return 1;
    }

    onscreen = cg_onscreen_new(dev, 640, 480);
    fb = onscreen;

    cg_framebuffer_set_samples_per_pixel(fb, 4);

    if (!cg_framebuffer_allocate(fb, &error)) {
        fprintf(stderr,
                "Failed to allocate 4x msaa offscreen framebuffer, "
                "disabling msaa for onscreen rendering: %s\n",
                error->message);
        cg_error_free(error);
        cg_framebuffer_set_samples_per_pixel(fb, 0);

        error = NULL;
        if (!cg_framebuffer_allocate(fb, &error)) {
            fprintf(
                stderr, "Failed to allocate framebuffer: %s\n", error->message);
            return 1;
        }
    }

    cg_onscreen_show(onscreen);

    tex = cg_texture_2d_new_with_size(dev, 320, 480);
    offscreen = cg_offscreen_new_with_texture(tex);
    offscreen_fb = offscreen;
    cg_framebuffer_set_samples_per_pixel(offscreen_fb, 4);
    if (!cg_framebuffer_allocate(offscreen_fb, &error)) {
        cg_error_free(error);
        error = NULL;
        fprintf(stderr,
                "Failed to allocate 4x msaa offscreen framebuffer, "
                "disabling msaa for offscreen rendering");
        cg_framebuffer_set_samples_per_pixel(offscreen_fb, 0);
    }

    triangle = cg_primitive_new_p2c4(dev, CG_VERTICES_MODE_TRIANGLES, 3,
                                     triangle_vertices);
    pipeline = cg_pipeline_new(dev);

    for (;; ) {
        cg_poll_fd_t *poll_fds;
        int n_poll_fds;
        int64_t timeout;
        cg_pipeline_t *texture_pipeline;

        cg_framebuffer_clear4f(fb, CG_BUFFER_BIT_COLOR, 0, 0, 0, 1);

        cg_framebuffer_push_matrix(fb);
        cg_framebuffer_scale(fb, 0.5, 1, 1);
        cg_framebuffer_translate(fb, -1, 0, 0);
        cg_primitive_draw(triangle, fb, pipeline);
        cg_framebuffer_pop_matrix(fb);

        cg_primitive_draw(triangle, fb, pipeline);
        cg_framebuffer_resolve_samples(offscreen_fb);

        texture_pipeline = cg_pipeline_new(dev);
        cg_pipeline_set_layer_texture(texture_pipeline, 0, tex);
        cg_framebuffer_draw_rectangle(fb, texture_pipeline, 0, 1, 1, -1);
        cg_object_unref(texture_pipeline);

        cg_onscreen_swap_buffers(onscreen);

        cg_loop_get_info(
            cg_device_get_renderer(dev), &poll_fds, &n_poll_fds, &timeout);
        g_poll((GPollFD *)poll_fds, n_poll_fds, 0);
        cg_loop_dispatch(
            cg_device_get_renderer(dev), poll_fds, n_poll_fds);
    }

    return 0;
}
