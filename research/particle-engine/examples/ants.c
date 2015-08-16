/*
 *         ants.c -- A particle flocking demo.
 */
#include "config.h"

#include <clib.h>

#include "particle-swarm.h"

#define WIDTH 1024
#define HEIGHT 768

struct demo {
    cg_device_t *dev;
    cg_framebuffer_t *fb;
    c_matrix_t view;
    int width, height;

    struct particle_swarm *swarm;

    uv_idle_t idle;
    c_timer_t *timer;

    bool swap_ready;
};

static void paint_cb(uv_idle_t *idle)
{
    struct demo *demo = idle->data;

    cg_framebuffer_clear4f(demo->fb,
                           CG_BUFFER_BIT_COLOR | CG_BUFFER_BIT_DEPTH,
                           1, 1, 1, 1);

    cg_framebuffer_push_matrix(demo->fb);

    particle_swarm_paint(demo->swarm);

    cg_framebuffer_pop_matrix(demo->fb);

    cg_onscreen_swap_buffers(demo->fb);

    uv_idle_stop(idle);
}

static void frame_event_cb(cg_onscreen_t *onscreen, cg_frame_event_t event,
                           cg_frame_info_t *info, void *data)
{
    if (event == CG_FRAME_EVENT_SYNC) {
        struct demo *demo = data;
        uv_idle_start(&demo->idle, paint_cb);
    }
}

static void init_particle_swarm(struct demo *demo)
{
    struct particle_swarm *swarm;

    demo->swarm = swarm = particle_swarm_new(demo->dev, demo->fb);

    swarm->particle_count = 1250;

    swarm->type = SWARM_TYPE_FLOCK;
    swarm->particle_sight = 40;

    swarm->agility = 0.1;

    swarm->speed_limits.max = 25;
    swarm->speed_limits.min = 5;
    swarm->particle_size = 3.0;

    swarm->particle_cohesion_rate = 0.030;
    swarm->particle_velocity_consistency = 0.002;

    swarm->particle_distance = 15;
    swarm->particle_repulsion_rate = 0.002;

    /* Boundaries */
    swarm->width = demo->width;
    swarm->height = demo->height;
    swarm->depth = 100;
    swarm->boundary_threshold = 0.05;
    swarm->boundary_repulsion_rate = 1.5;

    /* Color */
    swarm->particle_color.saturation.value = 0;
    swarm->particle_color.luminance.value = 0.5;
    swarm->particle_color.luminance.variance = 0.05;
    swarm->particle_color.luminance.type = FLOAT_VARIANCE_PROPORTIONAL;
}

int main(int argc, char **argv)
{
    cg_onscreen_t *onscreen;
    cg_error_t *error = NULL;
    struct demo demo;
    float fovy, aspect, z_near, z_2d, z_far;
    uv_loop_t *loop = uv_default_loop();

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
    demo.swap_ready = true;

    cg_onscreen_add_frame_callback(demo.fb, frame_event_cb, &demo, NULL);

    init_particle_swarm(&demo);

    demo.timer = c_timer_new();

    uv_idle_init(loop, &demo.idle);
    demo.idle.data = &demo;
    uv_idle_start(&demo.idle, paint_cb);

    cg_uv_set_mainloop(demo.dev, loop);
    uv_run(loop, UV_RUN_DEFAULT);

    return 0;
}
