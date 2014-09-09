/*
 * Copyright © 2013 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

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

struct particle_swarm *particle_swarm_new(cg_device_t *dev, cg_framebuffer_t *fb);

void particle_swarm_free(struct particle_swarm *swarm);

void particle_swarm_paint(struct particle_swarm *swarm);

#endif /* _PARTICLE_SWARM_H_ */
