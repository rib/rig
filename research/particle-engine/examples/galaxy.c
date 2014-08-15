/*
 *         galaxy.c -- A particle system demo.
 */
#include "config.h"

#include "particle-system.h"

#include <math.h>

#define WIDTH 1024
#define HEIGHT 768

struct demo {
	CoglContext *ctx;
	CoglFramebuffer *fb;
	CoglMatrix view;
	int width, height;

	struct particle_system *system;

	c_timer_t *timer;

	bool swap_ready;
	GMainLoop *main_loop;
};

static void paint_cb(struct demo *demo) {
	float rotation;

	rotation = c_timer_elapsed (demo->timer, NULL) * 2.0f;

	cogl_framebuffer_clear4f(demo->fb,
				 COGL_BUFFER_BIT_COLOR | COGL_BUFFER_BIT_DEPTH,
				 0.0f, 0.0f, 0.0f, 1);

	cogl_framebuffer_push_matrix(demo->fb);

	cogl_framebuffer_translate(demo->fb, WIDTH / 2, HEIGHT / 2, 0);

	cogl_framebuffer_rotate (demo->fb, 70, 1, 0, 0);
	cogl_framebuffer_rotate (demo->fb, rotation, 0, 0.4, 1);

	particle_system_paint(demo->system);

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

static void init_particle_system(struct demo *demo)
{
	demo->system = particle_system_new(demo->ctx, demo->fb);

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

	init_particle_system(&demo);

	demo.timer = c_timer_new();

	g_idle_add(update_cb, &demo);
	loop = g_main_loop_new (NULL, TRUE);
	g_main_loop_run (loop);

	return 0;
}
