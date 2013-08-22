#include "particle-system.h"

#include "particle-engine.h"

#include <cogl/cogl.h>
#include <math.h>
#include <string.h>

struct particle {
	/* The radius of the orbit */
	float radius;

	/* The angular velocity in the orbital plane (in radians per
	 * millisecond). */
	float speed;

	/* The orbital period offset, in seconds. */
	gdouble t_offset;

	/* Longitude of ascending node, in radians. */
	float ascending_node;

	/* Inclination in radians from equatorial plane. If inclination is >
	 * pi/2, orbit is retrograde. */
	float inclination;
};

struct particle_system_priv {
	GTimer *timer;
	gdouble current_time;
	gdouble last_update_time;

	GRand *rand;

	struct particle *particles;

	CoglContext *ctx;
	CoglFramebuffer *fb;
	struct particle_engine *engine;
};

struct particle_system* particle_system_new(CoglContext *ctx,
					    CoglFramebuffer *fb)
{
	struct particle_system *system = g_slice_new0(struct particle_system);
	struct particle_system_priv *priv = g_slice_new0(struct particle_system_priv);

	priv->ctx = cogl_object_ref(ctx);
	priv->fb = cogl_object_ref(fb);

	priv->timer = g_timer_new();
	priv->rand = g_rand_new();

	system->priv = priv;

	return system;
}

void particle_system_free(struct particle_system *system)
{
	struct particle_system_priv *priv = system->priv;

	cogl_object_unref(priv->ctx);
	cogl_object_unref(priv->fb);

	g_rand_free(priv->rand);
	g_timer_destroy(priv->timer);

	particle_engine_free(priv->engine);

	g_slice_free(struct particle_system_priv, priv);
	g_slice_free(struct particle_system, system);
}

static void create_particle(struct particle_system *system,
			    int index)
{
	struct particle_system_priv *priv = system->priv;
	struct particle *particle = &priv->particles[index];
	float period;
	CoglColor *color;

	color = particle_engine_get_particle_color(priv->engine, index);

	/* Get angle of inclination */
	particle->inclination = fuzzy_float_get_real_value(&system->inclination, priv->rand);

	/* Get the ascending node */
	particle->ascending_node = g_random_double_range(0, M_PI * 2);

	/* Particle color. */
	fuzzy_color_get_cogl_color(&system->particle_color, priv->rand, color);

	switch (system->type) {
	case SYSTEM_TYPE_CIRCULAR_ORBIT:
		/* Get orbital radius */
		particle->radius = fuzzy_float_get_real_value(&system->radius,
							      priv->rand);

		/* Orbital velocity */
		particle->speed = system->u / particle->radius;

		/* In a circular orbit, the orbital period is:
		 *
		 *    T = 2pi * sqrt(r^3 / μ)
		 *
		 * r - radius
		 * μ - standard gravitational parameter
		 */
		period = 2 * M_PI * sqrt(powf(particle->radius, 3) / system->u);

		/* Start the orbit at a random point around it's circumference. */
		particle->t_offset = g_rand_double_range(priv->rand, 0, period);

		break;
	}
}

static void create_resources(struct particle_system *system)
{
	struct particle_system_priv *priv = system->priv;
	int i;

	priv->engine = particle_engine_new(priv->ctx, priv->fb,
					   system->particle_count,
					   system->particle_size);

	priv->particles = g_new0(struct particle, system->particle_count);

	particle_engine_push_buffer(priv->engine,
				    COGL_BUFFER_ACCESS_READ_WRITE, 0);

	for (i = 0; i < system->particle_count; i++)
		create_particle(system, i);

	particle_engine_pop_buffer(priv->engine);
}

static void update_particle(struct particle_system *system,
			    int index)
{
	struct particle_system_priv *priv = system->priv;
	struct particle *particle = &priv->particles[index];
	float *position, time, x, y, z;

	position = particle_engine_get_particle_position(priv->engine, index);

	/* Get the particle age. */
	time = particle->t_offset + priv->current_time;

	switch (system->type) {
	case SYSTEM_TYPE_CIRCULAR_ORBIT:
	{
		float theta;

		/* Get the angular position. */
		theta = fmod(time * particle->speed, M_PI * 2);

		/* Object space coordinates. */
		x = cosf(theta) * particle->radius;
		y = sinf(theta) * particle->radius;
		z = 0;

		break;
	}
	default:
		g_warning(G_STRLOC "Unrecognised particle system type %d",
			  system->type);
		x = y = z = 0;
		break;
	}

	/* FIXME: Rotate around Z axis to the ascending node */
	/* x = x * cosf(particle->ascending_node) - y * sinf(particle->ascending_node); */
	/* y = x * sinf(particle->ascending_node) + y * cosf(particle->ascending_node); */

	/* FIXME: Rotate about X axis to the inclination */
	/* y = y * cosf(particle->inclination) - z * sinf(particle->inclination); */
	/* z = y * sinf(particle->inclination) + z * cosf(particle->inclination); */

	/* Update the new position. */
	position[0] = system->cog[0] + x;
	position[1] = system->cog[1] + y;
	position[2] = system->cog[2] + z;
}

static void tick(struct particle_system *system)
{
	struct particle_system_priv *priv = system->priv;
	struct particle_engine *engine = priv->engine;
	int i;

	/* Create resources as necessary */
	if (!engine)
		create_resources(system);

	/* Update the clocks */
	priv->last_update_time = priv->current_time;
	priv->current_time = g_timer_elapsed(priv->timer, NULL);

	/* Map the particle engine's buffer before reading or writing particle
	 * data.
	 */
	particle_engine_push_buffer(priv->engine,
				    COGL_BUFFER_ACCESS_READ_WRITE, 0);

	/* Iterate over every particle and update them. */
	for (i = 0; i < system->particle_count; i++)
		update_particle(system, i);

	/* Unmap the modified particle buffer. */
	particle_engine_pop_buffer(priv->engine);
}

void particle_system_paint(struct particle_system *system)
{
	tick(system);
	particle_engine_paint(system->priv->engine);
}
