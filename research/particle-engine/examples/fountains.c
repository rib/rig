#include "config.h"

#include "particle-emitter.h"

#include <cglib/cglib.h>

#define WIDTH 1024
#define HEIGHT 768

struct demo {
    cg_device_t *dev;
    cg_framebuffer_t *fb;
    c_matrix_t view;
    int width, height;

    struct particle_emitter *emitter[5];

    uv_idle_t idle;
    uv_timer_t timer;
};

static void paint_cb (uv_idle_t *idle)
{
    struct demo *demo = idle->data;
    unsigned int i;

    cg_framebuffer_clear4f(demo->fb,
                           CG_BUFFER_BIT_COLOR | CG_BUFFER_BIT_DEPTH,
                           0.15f, 0.15f, 0.3f, 1);

    for (i = 0; i < C_N_ELEMENTS(demo->emitter); i++) {
        particle_emitter_paint(demo->emitter[i]);
    }

    cg_onscreen_swap_buffers(demo->fb);

    uv_idle_stop(&demo->idle);
}

static void frame_event_cb(cg_onscreen_t *onscreen, cg_frame_event_t event,
                           cg_frame_info_t *info, void *data)
{
    if (event == CG_FRAME_EVENT_SYNC){
        struct demo *demo = data;
        uv_idle_start(&demo->idle, paint_cb);
    }
}

static void timeout_cb(uv_timer_t *timer)
{
    struct demo *demo = timer->data;
    unsigned int i;

    for (i = 0; i < 3; i++)
        demo->emitter[i]->active = !demo->emitter[i]->active;
}

static void init_particle_emitters(struct demo *demo)
{
    unsigned int i;

    for (i = 0; i < C_N_ELEMENTS(demo->emitter); i++) {
        demo->emitter[i] = particle_emitter_new(demo->dev, demo->fb);

        demo->emitter[i]->particle_count = 60000;
        demo->emitter[i]->particle_size = 2.0f;
        demo->emitter[i]->new_particles_per_ms = 10000;

        /* Lifespan */
        demo->emitter[i]->particle_lifespan.value = 2.0f;
        demo->emitter[i]->particle_lifespan.variance = 0.75f;
        demo->emitter[i]->particle_lifespan.type = DOUBLE_VARIANCE_PROPORTIONAL;

        /* Position variance */
        demo->emitter[i]->particle_position.type = VECTOR_VARIANCE_LINEAR;

        /* X position */
        demo->emitter[i]->particle_position.variance[0] = 10.0f;

        /* Y position */
        demo->emitter[i]->particle_position.value[1] = (float)HEIGHT + 5;
        demo->emitter[i]->particle_position.variance[1] = 10.0f;

        /* Z position */
        demo->emitter[i]->particle_position.value[2] = 0.0f;
        demo->emitter[i]->particle_position.variance[2] = 10.0f;

        /* Color */
        demo->emitter[i]->particle_color.hue.value = 236.0f;
        demo->emitter[i]->particle_color.hue.variance = 0.05f;
        demo->emitter[i]->particle_color.hue.type = FLOAT_VARIANCE_PROPORTIONAL;

        demo->emitter[i]->particle_color.saturation.value = 1.0f;
        demo->emitter[i]->particle_color.saturation.type = FLOAT_VARIANCE_NONE;

        demo->emitter[i]->particle_color.luminance.value = 0.9f;
        demo->emitter[i]->particle_color.luminance.variance = 0.15f;
        demo->emitter[i]->particle_color.luminance.type = FLOAT_VARIANCE_PROPORTIONAL;

        /* Direction */
        demo->emitter[i]->particle_direction.value[1] = -1.0f;
        demo->emitter[i]->particle_direction.variance[0] = 0.5f;
        demo->emitter[i]->particle_direction.variance[1] = 0.5f;
        demo->emitter[i]->particle_direction.variance[2] = 0.5f;
        demo->emitter[i]->particle_direction.type = VECTOR_VARIANCE_IRWIN_HALL;

        /* Speed */
        demo->emitter[i]->particle_speed.value = 14;
        demo->emitter[i]->particle_speed.variance = 5;
        demo->emitter[i]->particle_speed.type = FLOAT_VARIANCE_IRWIN_HALL;

        demo->emitter[i]->acceleration[0] = 0.2;
        demo->emitter[i]->acceleration[1] = 14;
        demo->emitter[i]->acceleration[2] = 0.0f;
    }

    /* Fountain X positions */
    demo->emitter[0]->particle_position.value[0] = (float)WIDTH / 2;
    demo->emitter[1]->particle_position.value[0] = (float)WIDTH / 4;
    demo->emitter[2]->particle_position.value[0] = ((float)WIDTH / 4) * 3;
    demo->emitter[3]->particle_position.value[0] = 0.0f;
    demo->emitter[4]->particle_position.value[0] = (float)WIDTH;

    /* Central fountain */
    demo->emitter[0]->active = false;
    demo->emitter[0]->particle_speed.value = 16.0f;
    demo->emitter[0]->particle_speed.type = FLOAT_VARIANCE_LINEAR;
    demo->emitter[0]->particle_direction.variance[0] = 0.3f;
    demo->emitter[0]->particle_direction.variance[1] = 0.3f;
    demo->emitter[0]->particle_direction.variance[2] = 0.3f;

    /* Side fountains */
    demo->emitter[3]->particle_count = 5000;
    demo->emitter[4]->particle_count = 5000;
    demo->emitter[4]->new_particles_per_ms = 2000;
    demo->emitter[4]->new_particles_per_ms = 2000;
    demo->emitter[3]->particle_speed.value = 12;
    demo->emitter[4]->particle_speed.value = 12;
    demo->emitter[3]->particle_speed.variance = 0.05f;
    demo->emitter[4]->particle_speed.variance = 0.05f;
    demo->emitter[3]->particle_direction.value[0] = 0.5f;  /* X component */
    demo->emitter[4]->particle_direction.value[0] = -0.5f; /* X component */
    demo->emitter[3]->particle_direction.value[1] = -0.7f; /* Y component */
    demo->emitter[4]->particle_direction.value[1] = -0.7f; /* Y component */
}

int main(int argc, char **argv)
{
    uv_loop_t *loop = uv_default_loop();
    cg_onscreen_t *onscreen;
    struct demo demo;
    float fovy, aspect, z_near, z_2d, z_far;

    demo.dev = cg_device_new();

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

    init_particle_emitters(&demo);

    uv_timer_init(loop, &demo.timer);
    demo.timer.data = &demo;
    uv_timer_start(&demo.timer, timeout_cb, 5000, 5000);

    uv_idle_init(loop, &demo.idle);
    demo.idle.data = &demo;
    uv_idle_start(&demo.idle, paint_cb);

    cg_uv_set_mainloop(demo.dev, loop);
    uv_run(loop, UV_RUN_DEFAULT);

    return 0;
}
