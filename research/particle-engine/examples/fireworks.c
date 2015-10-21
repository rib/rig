#include "config.h"

#include "particle-emitter.h"

#include <cglib/cglib.h>

#define WIDTH 1024
#define HEIGHT 768

#define TIME_MIN 125
#define TIME_MAX 1000

#define N_EMITTERS 10

struct demo {
    cg_device_t *dev;
    cg_framebuffer_t *fb;
    c_matrix_t view;
    int width, height;

    struct particle_emitter *emitter[N_EMITTERS];
    uv_timer_t deactivate_timer[N_EMITTERS];
    unsigned int last_active;

    uv_idle_t idle;
    uv_timer_t ignite_timer;
};

static void deactivate_firework_cb(uv_timer_t *timer)
{
    struct demo *demo = timer->data;

    demo->emitter[demo->last_active]->active = false;

    uv_timer_stop(timer);
}

static void ignite_firework(struct demo *demo, int i) {
    demo->last_active = i;

    c_return_if_fail(!demo->emitter[i]->active);

    demo->emitter[i]->new_particles_per_ms = c_random_int32_range(3000, 20000);
    demo->emitter[i]->particle_size = c_random_double_range(1, 3);

    /* Position */
    demo->emitter[i]->particle_position.value[0] = c_random_double_range(0, WIDTH);
    demo->emitter[i]->particle_position.value[1] = c_random_double_range(0, HEIGHT / 2);
    demo->emitter[i]->particle_position.type = VECTOR_VARIANCE_NONE;

    /* Lifespan */
    demo->emitter[i]->particle_lifespan.value = c_random_double_range(0.75, 2);
    demo->emitter[i]->particle_lifespan.variance = 1.5f;
    demo->emitter[i]->particle_lifespan.type = DOUBLE_VARIANCE_LINEAR;

    /* Direction */
    demo->emitter[i]->particle_direction.variance[0] = 1.0f;
    demo->emitter[i]->particle_direction.variance[1] = 1.0f;
    demo->emitter[i]->particle_direction.variance[2] = 1.0f;
    demo->emitter[i]->particle_direction.type = VECTOR_VARIANCE_LINEAR;

    /* Speed */
    demo->emitter[i]->particle_speed.value = c_random_double_range(4, 12);
    demo->emitter[i]->particle_speed.variance = 0.3f;
    demo->emitter[i]->particle_speed.type = FLOAT_VARIANCE_PROPORTIONAL;

    /* Color */
    demo->emitter[i]->particle_color.hue.value = c_random_double_range(0, 360.0f);
    demo->emitter[i]->particle_color.hue.variance = c_random_double_range(0, 240.0f);
    demo->emitter[i]->particle_color.hue.type = FLOAT_VARIANCE_LINEAR;

    demo->emitter[i]->particle_color.saturation.value = 1.0f;
    demo->emitter[i]->particle_color.saturation.variance = 0.0f;
    demo->emitter[i]->particle_color.saturation.type = FLOAT_VARIANCE_NONE;

    demo->emitter[i]->particle_color.luminance.value =
        c_random_double_range(0.5, 0.9);
    demo->emitter[i]->particle_color.luminance.variance = 0.1f;
    demo->emitter[i]->particle_color.luminance.type = FLOAT_VARIANCE_PROPORTIONAL;

    demo->emitter[i]->active = true;

    uv_timer_start(&demo->deactivate_timer[i], deactivate_firework_cb, 75, 75);
}

static void paint_cb(uv_idle_t *idle)
{
    struct demo *demo = idle->data;
    unsigned int i;

    cg_framebuffer_clear4f(demo->fb,
                           CG_BUFFER_BIT_COLOR | CG_BUFFER_BIT_DEPTH,
                           0.0f, 0.0f, 0.1f, 1);

    for (i = 0; i < N_EMITTERS; i++)
        particle_emitter_paint(demo->emitter[i]);

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

static void timeout_cb(uv_timer_t *timer)
{
    struct demo *demo = timer->data;
    unsigned int i;
    uint64_t timeout;

    for (i = 0; i < N_EMITTERS; i++) {
        if (i != demo->last_active) {
            ignite_firework(demo, i);
            demo->last_active = i;
            break;
        }
    }

    uv_timer_stop(&demo->ignite_timer);
    timeout = c_random_int32_range(TIME_MIN, TIME_MAX);
    uv_timer_start(&demo->ignite_timer, timeout_cb, timeout, timeout);
}

int main(int argc, char **argv)
{
    uv_loop_t *loop = uv_default_loop();
    cg_onscreen_t *onscreen;
    struct demo demo;
    float fovy, aspect, z_near, z_2d, z_far;
    unsigned int i;

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


    for (i = 0; i < N_EMITTERS; i++) {
        demo.emitter[i] = particle_emitter_new(demo.dev, demo.fb);
        demo.emitter[i]->active = false;
        demo.emitter[i]->particle_count = 10000;
        demo.emitter[i]->particle_size = 2.0f;
        demo.emitter[i]->acceleration[1] = 8;

        uv_timer_init(loop, &demo.deactivate_timer[i]);
        demo.deactivate_timer[i].data = &demo;
    }

    demo.last_active = -1;

    uv_timer_init(loop, &demo.ignite_timer);
    demo.ignite_timer.data = &demo;
    uv_timer_start(&demo.ignite_timer, timeout_cb, 0, 0);

    uv_idle_init(loop, &demo.idle);
    demo.idle.data = &demo;
    uv_idle_start(&demo.idle, paint_cb);

    cg_uv_set_mainloop(demo.dev, loop);
    uv_run(loop, UV_RUN_DEFAULT);

    return 0;
}
