/*
 *         catherine-wheel.c -- A Catherine wheel.
 *
 */
#include "config.h"

#include "particle-emitter.h"

#include <cglib/cglib.h>
#include <math.h>

#define WIDTH 1024
#define HEIGHT 768

#define WHEEL_RADIUS 250

#define RATE_MAX 3
#define RATE_INC 0.002

struct demo {
    cg_device_t *dev;
    cg_framebuffer_t *fb;
    c_matrix_t view;
    int width, height;

    struct particle_emitter *emitter[3];

    uv_idle_t idle;
    c_timer_t *timer;
    double spin_rate;
    double angle_between_emitters;

    bool swap_ready;
};

static void update_catherine_wheel(struct demo *demo)
{
    unsigned int i;
    double elapsed_time = c_timer_elapsed(demo->timer, NULL);
    double spin = demo->spin_rate * elapsed_time;

    if (demo->spin_rate < RATE_MAX)
        demo->spin_rate += RATE_INC;

    for (i = 0; i < C_N_ELEMENTS(demo->emitter); i++) {
        double x = i * demo->angle_between_emitters + spin;
        double x_cos = cos(x);
        double x_sin = sin(x);


        demo->emitter[i]->particle_position.value[0] =
            WIDTH / 2 + WHEEL_RADIUS * x_cos;
        demo->emitter[i]->particle_position.value[1] =
            HEIGHT / 2 - WHEEL_RADIUS * x_sin;

        demo->emitter[i]->particle_direction.value[0] = x_sin;
        demo->emitter[i]->particle_direction.value[1] = x_cos;
    }
}

static void paint_cb(uv_idle_t *idle)
{
    struct demo *demo = idle->data;
    unsigned int i;

    update_catherine_wheel(demo);

    cg_framebuffer_clear4f(demo->fb,
                           CG_BUFFER_BIT_COLOR | CG_BUFFER_BIT_DEPTH,
                           0, 0, 0, 1);

    for (i = 0; i < C_N_ELEMENTS(demo->emitter); i++)
        particle_emitter_paint(demo->emitter[i]);

    cg_onscreen_swap_buffers(demo->fb);

    uv_idle_stop(&demo->idle);
}

static void frame_event_cb(cg_onscreen_t *onscreen, cg_frame_event_t event,
                           cg_frame_info_t *info, void *data) {
    if (event == CG_FRAME_EVENT_SYNC) {
        struct demo *demo = data;
        uv_idle_start(&demo->idle, paint_cb);
    }
}

static void init_particle_emitters(struct demo *demo)
{
    unsigned int i;

    for (i = 0; i < C_N_ELEMENTS(demo->emitter); i++) {
        demo->emitter[i] = particle_emitter_new(demo->dev, demo->fb);
        demo->emitter[i]->particle_count = 80000;
        demo->emitter[i]->particle_size = 1.0f;
        demo->emitter[i]->new_particles_per_ms = demo->emitter[i]->particle_count / 2;

        /* Global force */
        demo->emitter[i]->acceleration[1] = 14;

        /* Particle position */
        demo->emitter[i]->particle_position.value[0] = WIDTH / 2;
        demo->emitter[i]->particle_position.value[1] = HEIGHT / 2;
        demo->emitter[i]->particle_position.type = VECTOR_VARIANCE_NONE;

        /* Particle speed */
        demo->emitter[i]->particle_speed.value = 22;
        demo->emitter[i]->particle_speed.variance = 0.6;
        demo->emitter[i]->particle_speed.type = FLOAT_VARIANCE_PROPORTIONAL;

        /* Direction */
        demo->emitter[i]->particle_direction.variance[0] = 0.7;
        demo->emitter[i]->particle_direction.variance[1] = 0.7;
        demo->emitter[i]->particle_direction.type = VECTOR_VARIANCE_IRWIN_HALL;

        /* Lifespan */
        demo->emitter[i]->particle_lifespan.value = 1.5;
        demo->emitter[i]->particle_lifespan.variance = 0.95;
        demo->emitter[i]->particle_lifespan.type = DOUBLE_VARIANCE_PROPORTIONAL;

        /* Color */
        demo->emitter[i]->particle_color.hue.value = 32;
        demo->emitter[i]->particle_color.hue.variance = 20;
        demo->emitter[i]->particle_color.hue.type = FLOAT_VARIANCE_LINEAR;

        demo->emitter[i]->particle_color.saturation.value = 1;
        demo->emitter[i]->particle_color.luminance.value = 0.6;
        demo->emitter[i]->particle_color.luminance.variance = 0.4;
        demo->emitter[i]->particle_color.luminance.type = FLOAT_VARIANCE_LINEAR;
    }
}

int main(int argc, char **argv)
{
    cg_onscreen_t *onscreen;
    cg_error_t *error = NULL;
    struct demo demo;
    float fovy, aspect, z_near, z_2d, z_far;
    uv_loop_t *loop = uv_default_loop();

    demo.dev = cg_device_new ();
    if (!demo.dev || error != NULL)
        c_error("Failed to create Cogl context\n");

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
    demo.swap_ready = true;

    cg_onscreen_add_frame_callback(demo.fb, frame_event_cb, &demo, NULL);

    init_particle_emitters(&demo);

    demo.timer = c_timer_new();
    demo.spin_rate = 0;
    demo.angle_between_emitters = 2 * M_PI / C_N_ELEMENTS(demo.emitter);

    uv_idle_init(loop, &demo.idle);
    demo.idle.data = &demo;
    uv_idle_start(&demo.idle, paint_cb);

    cg_uv_set_mainloop(demo.dev, loop);
    uv_run(loop, UV_RUN_DEFAULT);

    return 0;
}
