/*
 *         ants.c -- A particle flocking demo.
 */
#include "config.h"

#include "particle-swarm.h"

#define WIDTH 1024
#define HEIGHT 768

struct demo {
	CoglContext *ctx;
	CoglFramebuffer *fb;
	CoglMatrix view;
	int width, height;

	struct particle_swarm *swarm;

	GTimer *timer;

	bool swap_ready;
	GMainLoop *main_loop;
};

static void paint_cb(struct demo *demo) {
	cogl_framebuffer_clear4f(demo->fb,
				 COGL_BUFFER_BIT_COLOR | COGL_BUFFER_BIT_DEPTH,
				 1, 1, 1, 1);

	cogl_framebuffer_push_matrix(demo->fb);

	particle_swarm_paint(demo->swarm);

	cogl_framebuffer_pop_matrix(demo->fb);
}

static void frame_event_cb(CoglOnscreen *onscreen, CoglFrameEvent event,
			   CoglFrameInfo *info, void *data) {
	struct demo *demo = data;

	if (event == COGL_FRAME_EVENT_SYNC)
		demo->swap_ready = TRUE;
}

static gboolean update_cb(gpointer data)
{
	struct demo *demo = data;
	CoglPollFD *poll_fds;
	int n_poll_fds;
	int64_t timeout;

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

static void init_particle_swarm(struct demo *demo)
{
	struct particle_swarm *swarm;

	demo->swarm = swarm = particle_swarm_new(demo->ctx, demo->fb);

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
	aspect = demo.width / demo.height;
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

	init_particle_swarm(&demo);

	demo.timer = g_timer_new();

	g_idle_add(update_cb, &demo);
	loop = g_main_loop_new (NULL, TRUE);
	g_main_loop_run (loop);

	return 0;
}
