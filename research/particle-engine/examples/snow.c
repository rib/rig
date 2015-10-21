/*
 *         snow.c -- A snowy night scene.
 *
 * This simple demo consists of a single particle emitter which emits a steady
 * stream of snowflakes into a light breeze. This demonstrates the support for
 * changing the global acceleration force and particle creation rate in real
 * time.
 */
#include "config.h"

#include "particle-emitter.h"

#include <cglib/cglib.h>
#include <math.h>

#define WIDTH 1024
#define HEIGHT 768

struct demo {
    cg_device_t *dev;
    cg_framebuffer_t *fb;
    c_matrix_t view;
    int width, height;

    struct particle_emitter *emitter;

    c_timer_t *timer;

    double snow_rate;

    uv_idle_t idle;
};

static void paint_cb(uv_idle_t *idle)
{
    struct demo *demo = idle->data;

    /* Change the direction and velocity of wind over time */
    demo->emitter->acceleration[0] = 0.3 *
        sin(0.25 * c_timer_elapsed(demo->timer, NULL));

    /* Change the rate at which new snow appears over time */
    demo->snow_rate += c_random_double_range(0, 0.005);
    demo->emitter->new_particles_per_ms = 30 +
        fabs(300 * sin(demo->snow_rate));

    cg_framebuffer_clear4f(demo->fb,
                           CG_BUFFER_BIT_COLOR | CG_BUFFER_BIT_DEPTH,
                           0.0f, 0.0f, 0.1f, 1);

    particle_emitter_paint(demo->emitter);

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

static void init_particle_emitter(struct demo *demo)
{
    demo->emitter = particle_emitter_new(demo->dev, demo->fb);
    demo->emitter->particle_count = 2000;
    demo->emitter->particle_size = 4.0f;
    demo->emitter->new_particles_per_ms = 250;

    /* Global force */
    demo->emitter->acceleration[1] = 0.6;

    /* Particle position */
    demo->emitter->particle_position.value[0] = WIDTH / 2;
    demo->emitter->particle_position.variance[0] = WIDTH + WIDTH / 2;
    demo->emitter->particle_position.value[1] = -80;
    demo->emitter->particle_position.type = VECTOR_VARIANCE_LINEAR;

    /* Particle speed */
    demo->emitter->particle_speed.value = 0.06;
    demo->emitter->particle_speed.variance = 0.02;
    demo->emitter->particle_speed.type = FLOAT_VARIANCE_PROPORTIONAL;

    /* Direction */
    demo->emitter->particle_direction.value[1] = 0.5f;
    demo->emitter->particle_direction.variance[0] = 0.8;
    demo->emitter->particle_direction.type = VECTOR_VARIANCE_IRWIN_HALL;

    /* Lifespan */
    demo->emitter->particle_lifespan.value = 6.5f;
    demo->emitter->particle_lifespan.variance = 1.5f;
    demo->emitter->particle_lifespan.type = DOUBLE_VARIANCE_LINEAR;

    /* Color */
    demo->emitter->particle_color.saturation.value = 1.0f;
    demo->emitter->particle_color.luminance.value = 1.0f;
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
    aspect = (float)demo.width / (float)demo.height;
    z_near = 0.1;
    z_2d = 1000;
    z_far = 2000;

    cg_framebuffer_perspective(demo.fb, fovy, aspect, z_near, z_far);
    c_matrix_init_identity(&demo.view);
    c_matrix_view_2d_in_perspective(&demo.view, fovy, aspect, z_near, z_2d,
                                     demo.width, demo.height);
    cg_framebuffer_set_modelview_matrix(demo.fb, &demo.view);

    cg_onscreen_add_frame_callback(demo.fb, frame_event_cb, &demo, NULL);

    init_particle_emitter(&demo);

    demo.timer = c_timer_new();
    demo.snow_rate = 0;

    uv_idle_init(loop, &demo.idle);
    demo.idle.data = &demo;
    uv_idle_start(&demo.idle, paint_cb);

    cg_uv_set_mainloop(demo.dev, loop);
    uv_run(loop, UV_RUN_DEFAULT);

    return 0;
}
