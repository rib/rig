#ifndef _PARTICLE_EMITTER_H_
#define _PARTICLE_EMITTER_H_

#include "fuzzy.h"

#include <cogl/cogl.h>

/* <priv> */
struct particle_emitter_priv;

/*
 * A particle emitter
 */
struct particle_emitter {

	/*
	 * Controls whether the particle emitter is active. If false, no new
	 * particles are created.
	 */
	CoglBool active;

	/*
	 * The maximum number of particles that can exist at any given moment in
	 * time. When this number of particles has been generated, then new
	 * particles will only be created as and when old particles are
	 * destroyed.
	 */
	int particle_count;

	/*
	 * This controls the rate at which new particles are generated.
	 */
	int new_particles_per_ms;

	/*
	 * The size (in pixels) of particles. Each particle is represented by a
	 * rectangular point of dimensions particle_size × particle_size.
	 */
	float particle_size;

	/*
	 * The length of time (in seconds) that a particle exists for.
	 */
	struct fuzzy_double particle_lifespan;

	/*
	 * The starting position for particles.
	 */
	struct fuzzy_vector particle_position;

	/*
	 * A unit vector to describe particle starting direction.
	 */
	struct fuzzy_vector particle_direction;

	/*
	 * The initial particle speed.
	 */
	struct fuzzy_float particle_speed;

	/*
	 * The initial particle color. Once created, a particle maintains the
	 * same color for the duration of it's lifespan, but it's opacity is
	 * related to it's age, so a particle begins opaque and fades into
	 * transparency.
	 */
	struct fuzzy_color particle_color;

	/*
	 * A uniform global force which is applied to every particle. Can be
	 * used to model gravity, wind etc.
	 */
	float acceleration[3];

	/* <priv> */
	struct particle_emitter_priv *priv;
};

struct particle_emitter *particle_emitter_new(CoglContext *ctx, CoglFramebuffer *fb);

void particle_emitter_free(struct particle_emitter *emitter);

void particle_emitter_paint(struct particle_emitter *emitter);

#endif /* _PARTICLE_EMITTER_H_ */
