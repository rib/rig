#ifndef _PARTICLE_SWARM_H_
#define _PARTICLE_SWARM_H_

#include "fuzzy.h"

/* <priv> */
struct particle_swarm_priv;

/*
 * A particle swarm
 */
struct particle_swarm {

	/* The number of particles in the swarm. */
	int particle_count;

	/* The size (in pixels) of particles. Each particle is represented by a
	 * rectangular point of dimensions particle_size × particle_size. */
	float particle_size;

	float width;
	float height;
	float depth;
	/* The threshold at which particles are repelled from the boundaries. */
	float boundary_threshold;
	/* The rate at which particles are repelled from the boundaries */
	float boundary_repulsion_rate;

	/* The minimum and maximum speeds at which particles may move */
	struct {
		float min;
		float max;
	} speed_limits;

	/* The behaviour of the particle swarm.
	 *
	 * HIVE
	 *  This swarm behaves as a single entity, with each particle sharing an
	 *  apparent 'hive mind' mentality to make them move and behave in
	 *  unison.
	 *
	 * FLOCK
	 *  This swarm exhibits flocking patterns, where particles are aware of
	 *  only a limited range of the surrounding particles, meaning that they
	 *  can flock together into small groups which behave independently and
	 *  interact with one another.
	 */
	enum {
		SWARM_TYPE_HIVE,
		SWARM_TYPE_FLOCK
	} type;

	/* The distance (in pixels) that particles can detect other particles in
	 * the surrounding area. Only used for swarms with SWARM_TYPE_FLOCK
	 * behaviour. */
	float particle_sight;

	/* The rate at which particles are attracted to each-other */
	float particle_cohesion_rate;

	/* The rate of consistency between particle velocities */
	float particle_velocity_consistency;

	/* The distance at which particles begin to repel each-other */
	float particle_distance;
	/* The rate at which particles are repelled from each-other */
	float particle_repulsion_rate;

	/* The rate at which particles can manoeuvre (higher value means more
	 * agile particles): */
	float agility;

	float acceleration[3];

	/* Particle color. */
	struct fuzzy_color particle_color;

	/* <priv> */
	struct particle_swarm_priv *priv;
};

struct particle_swarm *particle_swarm_new(CoglContext *ctx, CoglFramebuffer *fb);

void particle_swarm_free(struct particle_swarm *swarm);

void particle_swarm_paint(struct particle_swarm *swarm);

#endif /* _PARTICLE_SWARM_H_ */
