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

#ifndef _PARTICLE_EMITTER_H_
#define _PARTICLE_EMITTER_H_

#include "fuzzy.h"

#include <cglib/cglib.h>

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
	bool active;

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

struct particle_emitter *particle_emitter_new(cg_device_t *dev, cg_framebuffer_t *fb);

void particle_emitter_free(struct particle_emitter *emitter);

void particle_emitter_paint(struct particle_emitter *emitter);

#endif /* _PARTICLE_EMITTER_H_ */
