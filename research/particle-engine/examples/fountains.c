#include "config.h"

#include "particle-emitter.h"

#include <cogl/cogl.h>

#define WIDTH 1024
#define HEIGHT 768

struct demo {
	CoglContext *ctx;
	CoglFramebuffer *fb;
	CoglMatrix view;
	int width, height;

	struct particle_emitter *emitter[5];

	guint timeout_id;

	bool swap_ready;
	GMainLoop *main_loop;
};

static void paint_cb (struct demo *demo) {
	unsigned int i;

	cogl_framebuffer_clear4f(demo->fb,
				 COGL_BUFFER_BIT_COLOR | COGL_BUFFER_BIT_DEPTH,
				 0.15f, 0.15f, 0.3f, 1);

	for (i = 0; i < G_N_ELEMENTS(demo->emitter); i++) {
		particle_emitter_paint(demo->emitter[i]);
	}
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

	for (i = 0; i < 3; i++)
		demo->emitter[i]->active = !demo->emitter[i]->active;

	return TRUE;
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

static void init_particle_emitters(struct demo *demo)
{
	unsigned int i;

	for (i = 0; i < G_N_ELEMENTS(demo->emitter); i++) {
		demo->emitter[i] = particle_emitter_new(demo->ctx, demo->fb);

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
	demo->emitter[0]->active = FALSE;
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
	demo.timeout_id = g_timeout_add(5000, timeout_cb, &demo);

	init_particle_emitters(&demo);

	g_idle_add(update_cb, &demo);

	loop = g_main_loop_new (NULL, TRUE);
	g_main_loop_run (loop);

	return 0;
}
