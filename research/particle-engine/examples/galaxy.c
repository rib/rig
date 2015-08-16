/*
 *         galaxy.c -- A particle system demo.
 */
#include "config.h"

#include "particle-system.h"

#include <math.h>

#define WIDTH 1024
#define HEIGHT 768

struct demo {
    cg_device_t *dev;
    cg_framebuffer_t *fb;
    c_matrix_t view;
    int width, height;

    struct particle_system *system;

    c_timer_t *timer;

    uv_idle_t idle;
};

static void paint_cb(uv_idle_t *idle)
{
    struct demo *demo = idle->data;
    float rotation;

    rotation = c_timer_elapsed(demo->timer, NULL) * 2.0f;

    cg_framebuffer_clear4f(demo->fb,
                           CG_BUFFER_BIT_COLOR | CG_BUFFER_BIT_DEPTH,
                           0.0f, 0.0f, 0.0f, 1);

    cg_framebuffer_push_matrix(demo->fb);

    cg_framebuffer_translate(demo->fb, WIDTH / 2, HEIGHT / 2, 0);

    cg_framebuffer_rotate (demo->fb, 70, 1, 0, 0);
    cg_framebuffer_rotate (demo->fb, rotation, 0, 0.4, 1);

    particle_system_paint(demo->system);

    cg_framebuffer_pop_matrix(demo->fb);

    cg_onscreen_swap_buffers(demo->fb);

    uv_idle_stop(&demo->idle);
}

static void frame_event_cb(cg_onscreen_t *onscreen, cg_frame_event_t event,
                           cg_frame_info_t *info, void *data)
{
    if (event == CG_FRAME_EVENT_SYNC) {
        struct demo *demo = data;
        uv_idle_start(&demo->idle, paint_cb);
    }
}

static void init_particle_system(struct demo *demo)
{
    demo->system = particle_system_new(demo->dev, demo->fb);

    demo->system->type = SYSTEM_TYPE_CIRCULAR_ORBIT;
    demo->system->particle_count = 50000;
    demo->system->particle_size = 1.0f;

    /* Center of gravity */
    demo->system->u = 14;

    /* Particle radius */
    demo->system->radius.value = 0;
    demo->system->radius.variance = 3500;
    demo->system->radius.type = FLOAT_VARIANCE_IRWIN_HALL;

    /* Orbital inclination */
    demo->system->inclination.value = 0;
    demo->system->inclination.variance = M_PI / 2;
    demo->system->inclination.type = FLOAT_VARIANCE_LINEAR;

    /* Color */
    demo->system->particle_color.hue.value = 28;
    demo->system->particle_color.hue.variance = 360;
    demo->system->particle_color.hue.type = FLOAT_VARIANCE_LINEAR;
    demo->system->particle_color.saturation.value = 1;
    demo->system->particle_color.luminance.value = 0.85;
    demo->system->particle_color.luminance.variance = 0.2;
    demo->system->particle_color.luminance.type = FLOAT_VARIANCE_PROPORTIONAL;
}

int main(int argc, char **argv)
{
    uv_loop_t *loop = uv_default_loop();
    cg_onscreen_t *onscreen;
    cg_error_t *error = NULL;
    struct demo demo;
    float fovy, aspect, z_near, z_2d, z_far;

    demo.dev = cg_device_new ();

    onscreen = cg_onscreen_new(demo.dev, WIDTH, HEIGHT);

    demo.fb = onscreen;
    demo.width = cg_framebuffer_get_width(demo.fb);
    demo.height = cg_framebuffer_get_height(demo.fb);

    cg_onscreen_show(onscreen);
    cg_framebuffer_set_viewport(demo.fb, 0, 0, demo.width, demo.height);

    fovy = 45;
    aspect = demo.width / demo.height;
    z_near = 0.1;
    z_2d = 1000;
    z_far = 2000;

    cg_framebuffer_perspective(demo.fb, fovy, aspect, z_near, z_far);
    c_matrix_init_identity(&demo.view);
    c_matrix_view_2d_in_perspective(&demo.view, fovy, aspect, z_near, z_2d,
                                     demo.width, demo.height);
    cg_framebuffer_set_modelview_matrix(demo.fb, &demo.view);

    cg_onscreen_add_frame_callback(demo.fb, frame_event_cb, &demo, NULL);

    init_particle_system(&demo);

    demo.timer = c_timer_new();

    uv_idle_init(loop, &demo.idle);
    demo.idle.data = &demo;
    uv_idle_start(&demo.idle, paint_cb);

    cg_uv_set_mainloop(demo.dev, loop);
    uv_run(loop, UV_RUN_DEFAULT);

    return 0;
}
