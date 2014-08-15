/*
 *         catherine-wheel.c -- A Catherine wheel.
 *
 */
#include "config.h"

#include "particle-emitter.h"

#include <cogl/cogl.h>
#include <math.h>

#define WIDTH 1024
#define HEIGHT 768

#define WHEEL_RADIUS 250

#define RATE_MAX 3
#define RATE_INC 0.002

struct demo {
	CoglContext *ctx;
	CoglFramebuffer *fb;
	CoglMatrix view;
	int width, height;

	struct particle_emitter *emitter[3];

	c_timer_t *timer;
	gdouble spin_rate;
	gdouble angle_between_emitters;

	bool swap_ready;
	GMainLoop *main_loop;
};

static void paint_cb(struct demo *demo) {
	unsigned int i;

	cogl_framebuffer_clear4f(demo->fb,
				 COGL_BUFFER_BIT_COLOR | COGL_BUFFER_BIT_DEPTH,
				 0, 0, 0, 1);

	for (i = 0; i < C_N_ELEMENTS(demo->emitter); i++)
		particle_emitter_paint(demo->emitter[i]);
}

static void frame_event_cb(CoglOnscreen *onscreen, CoglFrameEvent event,
			   CoglFrameInfo *info, void *data) {
	struct demo *demo = data;

	if (event == COGL_FRAME_EVENT_SYNC)
		demo->swap_ready = TRUE;
}

static void update_catherine_wheel(struct demo *demo) {
	unsigned int i;
	gdouble elapsed_time = c_timer_elapsed(demo->timer, NULL);
	gdouble spin = demo->spin_rate * elapsed_time;

	if (demo->spin_rate < RATE_MAX)
		demo->spin_rate += RATE_INC;

	for (i = 0; i < C_N_ELEMENTS(demo->emitter); i++) {
		gdouble x = i * demo->angle_between_emitters + spin;
		gdouble x_cos = cos(x);
		gdouble x_sin = sin(x);


		demo->emitter[i]->particle_position.value[0] =
			WIDTH / 2 + WHEEL_RADIUS * x_cos;
		demo->emitter[i]->particle_position.value[1] =
			HEIGHT / 2 - WHEEL_RADIUS * x_sin;

		demo->emitter[i]->particle_direction.value[0] = x_sin;
		demo->emitter[i]->particle_direction.value[1] = x_cos;
	}
}

static gboolean update_cb(gpointer data)
{
	struct demo *demo = data;
	CoglPollFD *poll_fds;
	int n_poll_fds;
	int64_t timeout;

	update_catherine_wheel(demo);

	if (demo->swap_ready) {
		paint_cb(demo);
		cogl_onscreen_swap_buffers(COGL_ONSCREEN(demo->fb));
	}

	cogl_poll_renderer_get_info(cogl_context_get_renderer(demo->ctx),
				    &poll_fds, &n_poll_fds, &timeout);

	g_poll ((GPollFD *)poll_fds, n_poll_fds,
		timeout == -1 ? -1 : timeout / 1000);

	cogl_poll_renderer_dispatch(cogl_context_get_renderer(demo->ctx),
				    poll_fds, n_poll_fds);

	return TRUE;
}

static void init_particle_emitters(struct demo *demo)
{
	unsigned int i;

	for (i = 0; i < C_N_ELEMENTS(demo->emitter); i++) {
		demo->emitter[i] = particle_emitter_new(demo->ctx, demo->fb);
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
	GMainLoop *loop;
	CoglOnscreen *onscreen;
	CoglError *error = NULL;
	struct demo demo;
	float fovy, aspect, z_near, z_2d, z_far;

	demo.ctx = cogl_context_new (NULL, &error);
	if (!demo.ctx || error != NULL)
		g_error("Failed to create Cogl context\n");

	onscreen = cogl_onscreen_new(demo.ctx, WIDTH, HEIGHT);

	demo.fb = onscreen;
	demo.width = cogl_framebuffer_get_width(demo.fb);
	demo.height = cogl_framebuffer_get_height(demo.fb);

	cogl_onscreen_show(onscreen);
	cogl_framebuffer_set_viewport(demo.fb, 0, 0, demo.width, demo.height);

	fovy = 45;
	aspect = (float)demo.width / (float)demo.height;
	z_near = 0.1;
	z_2d = 1000;
	z_far = 2000;

	cogl_framebuffer_perspective(demo.fb, fovy, aspect, z_near, z_far);
	cogl_matrix_init_identity(&demo.view);
	cogl_matrix_view_2d_in_perspective(&demo.view, fovy, aspect, z_near, z_2d,
					   demo.width, demo.height);
	cogl_framebuffer_set_modelview_matrix(demo.fb, &demo.view);
	demo.swap_ready = TRUE;

	cogl_onscreen_add_frame_callback(COGL_ONSCREEN(demo.fb),
					 frame_event_cb, &demo, NULL);

	init_particle_emitters(&demo);

	demo.timer = c_timer_new();
	demo.spin_rate = 0;
	demo.angle_between_emitters = 2 * M_PI / C_N_ELEMENTS(demo.emitter);

	g_idle_add(update_cb, &demo);
	loop = g_main_loop_new (NULL, TRUE);
	g_main_loop_run (loop);

	return 0;
}
