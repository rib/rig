#include "config.h"

#include "particle-emitter.h"

#include <cogl/cogl.h>

#define WIDTH 1024
#define HEIGHT 768

#define TIME_MIN 125
#define TIME_MAX 1000

struct demo {
	CoglContext *ctx;
	CoglFramebuffer *fb;
	CoglMatrix view;
	int width, height;

	struct particle_emitter *emitter[10];
	unsigned int last_active;

	guint timeout_id;

	CoglBool swap_ready;
	GMainLoop *main_loop;
};

static gboolean deactivate_firework(gpointer data)
{
	struct demo *demo = data;

	demo->emitter[demo->last_active]->active = FALSE;

	return FALSE;
}

static void ignite_firework(struct demo *demo, int i) {
	demo->last_active = i;

	demo->emitter[i]->new_particles_per_ms = g_random_int_range(3000, 20000);
	demo->emitter[i]->particle_size = g_random_double_range(1, 3);

	/* Position */
	demo->emitter[i]->particle_position.value[0] = g_random_double_range(0, WIDTH);
	demo->emitter[i]->particle_position.value[1] = g_random_double_range(0, HEIGHT / 2);
	demo->emitter[i]->particle_position.type = VECTOR_VARIANCE_NONE;

	/* Lifespan */
	demo->emitter[i]->particle_lifespan.value = g_random_double_range(0.75, 2);
	demo->emitter[i]->particle_lifespan.variance = 1.5f;
	demo->emitter[i]->particle_lifespan.type = DOUBLE_VARIANCE_LINEAR;

	/* Direction */
	demo->emitter[i]->particle_direction.variance[0] = 1.0f;
	demo->emitter[i]->particle_direction.variance[1] = 1.0f;
	demo->emitter[i]->particle_direction.variance[2] = 1.0f;
	demo->emitter[i]->particle_direction.type = VECTOR_VARIANCE_LINEAR;

	/* Speed */
	demo->emitter[i]->particle_speed.value = g_random_double_range(4, 12);
	demo->emitter[i]->particle_speed.variance = 0.3f;
	demo->emitter[i]->particle_speed.type = FLOAT_VARIANCE_PROPORTIONAL;

	/* Color */
	demo->emitter[i]->particle_color.hue.value = g_random_double_range(0, 360.0f);
	demo->emitter[i]->particle_color.hue.variance = g_random_double_range(0, 240.0f);
	demo->emitter[i]->particle_color.hue.type = FLOAT_VARIANCE_LINEAR;

	demo->emitter[i]->particle_color.saturation.value = 1.0f;
	demo->emitter[i]->particle_color.saturation.variance = 0.0f;
	demo->emitter[i]->particle_color.saturation.type = FLOAT_VARIANCE_NONE;

	demo->emitter[i]->particle_color.luminance.value =
		g_random_double_range(0.5, 0.9);
	demo->emitter[i]->particle_color.luminance.variance = 0.1f;
	demo->emitter[i]->particle_color.luminance.type = FLOAT_VARIANCE_PROPORTIONAL;

	demo->emitter[i]->active = TRUE;
	g_timeout_add(75, deactivate_firework, demo);
}

static void paint_cb(struct demo *demo) {
	unsigned int i;

	cogl_framebuffer_clear4f(demo->fb,
				 COGL_BUFFER_BIT_COLOR | COGL_BUFFER_BIT_DEPTH,
				 0.0f, 0.0f, 0.1f, 1);

	for (i = 0; i < G_N_ELEMENTS(demo->emitter); i++)
		particle_emitter_paint(demo->emitter[i]);
}

static void frame_event_cb(CoglOnscreen *onscreen, CoglFrameEvent event,
			   CoglFrameInfo *info, void *data) {
	struct demo *demo = data;

	if (event == COGL_FRAME_EVENT_SYNC)
		demo->swap_ready = TRUE;
}

static gboolean timeout_cb(gpointer data)
{
	struct demo *demo = data;
	unsigned int i;

	for (i = 0; i < G_N_ELEMENTS(demo->emitter); i++) {
		if (i != demo->last_active) {
			ignite_firework(demo, i);
			demo->last_active = i;
			break;
		}
	}

	g_timeout_add(g_random_int_range(TIME_MIN, TIME_MAX),
		      timeout_cb, demo);

	return FALSE;
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

int main(int argc, char **argv)
{
	GMainLoop *loop;
	CoglOnscreen *onscreen;
	CoglError *error = NULL;
	struct demo demo;
	float fovy, aspect, z_near, z_2d, z_far;
	unsigned int i;

	demo.ctx = cogl_context_new (NULL, &error);
	if (!demo.ctx || error != NULL)
		g_error("Failed to create Cogl context\n");

	onscreen = cogl_onscreen_new(demo.ctx, WIDTH, HEIGHT);

	demo.fb = COGL_FRAMEBUFFER(onscreen);
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


	for (i = 0; i < G_N_ELEMENTS(demo.emitter); i++) {
		demo.emitter[i] = particle_emitter_new(demo.ctx, demo.fb);
		demo.emitter[i]->active = FALSE;
		demo.emitter[i]->particle_count = 10000;
		demo.emitter[i]->particle_size = 2.0f;
		demo.emitter[i]->acceleration[1] = 8;
	}

	demo.last_active = -1;

	timeout_cb(&demo);

	g_idle_add(update_cb, &demo);

	loop = g_main_loop_new (NULL, TRUE);
	g_main_loop_run (loop);

	return 0;
}
