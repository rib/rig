#include <config.h>

#include <clib.h>
#include <rut.h>
#include <cglib/cglib.h>

struct data {
    rut_shell_t *shell;
    cg_device_t *dev;

    rut_shell_onscreen_t *shell_onscreen;
    cg_framebuffer_t *fb;

    cg_primitive_t *triangle;
    cg_pipeline_t *pipeline;
};


static void
shell_redraw_cb(rut_shell_t *shell, void *user_data)
{
    //struct data *data = idle->data;
    struct data *data = user_data;
    c_matrix_t identity;
    c_matrix_init_identity(&identity);

    c_print("Paint\n");

    rut_shell_start_redraw(shell);

#warning "update redraw loop"
    rut_shell_run_pre_paint_callbacks(shell);

    rut_shell_run_start_paint_callbacks(shell);

    rut_shell_dispatch_input_events(shell);

    cg_framebuffer_identity_matrix(data->fb);
    cg_framebuffer_set_projection_matrix(data->fb, &identity);

    cg_framebuffer_clear4f(data->fb, CG_BUFFER_BIT_COLOR, 0, 0, 0, 1);
    cg_primitive_draw(data->triangle, data->fb, data->pipeline);

    cg_onscreen_swap_buffers(data->fb);

    rut_shell_run_post_paint_callbacks(shell);

    rut_shell_end_redraw(shell);

    /* FIXME: we should hook into an asynchronous notification of
     * when rendering has finished for determining when a frame is
     * finished. */
    rut_shell_finish_frame(shell);
}

static void
on_run_cb(rut_shell_t *shell, void *user_data)
{
    struct data *data = user_data;
    cg_vertex_p2c4_t triangle_vertices[] = {
        { 0, 0.7, 0xff, 0x00, 0x00, 0xff },
        { -0.7, -0.7, 0x00, 0xff, 0x00, 0xff },
        { 0.7, -0.7, 0x00, 0x00, 0xff, 0xff }
    };

    data->dev = data->shell->cg_device;

    data->triangle = cg_primitive_new_p2c4(
        data->dev, CG_VERTICES_MODE_TRIANGLES, 3, triangle_vertices);
    data->pipeline = cg_pipeline_new(data->dev);

    data->shell_onscreen = rut_shell_onscreen_new(shell, 640, 480);
    rut_shell_onscreen_allocate(data->shell_onscreen);
    rut_shell_onscreen_set_resizable(data->shell_onscreen, true);
    rut_shell_onscreen_show(data->shell_onscreen);
    data->fb = data->shell_onscreen->cg_onscreen;
}

static rut_input_event_status_t
input_handler(rut_input_event_t *event,
              void *user_data)
{
    struct data *data = user_data;

    c_print("Event\n");

    return RUT_INPUT_EVENT_STATUS_HANDLED;
}

int
main(int argc, char **argv)
{
    struct data data;

    rut_init();

    data.shell = rut_shell_new(NULL, shell_redraw_cb, &data);
    rut_shell_set_on_run_callback(data.shell, on_run_cb, &data);

    rut_shell_add_input_callback(data.shell,
                                 input_handler,
                                 &data,
                                 NULL);

    rut_shell_main(data.shell);

    return 0;
}
