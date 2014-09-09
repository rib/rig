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

#ifndef _PARTICLE_SYSTEM_H_
#define _PARTICLE_SYSTEM_H_

#include "fuzzy.h"

/* <priv> */
struct particle_system_priv;

/*
 * A particle system
 */
struct particle_system {

	/* The type of system. */
	enum {
	  	SYSTEM_TYPE_CIRCULAR_ORBIT,
	} type;

	/* The position of the center of gravity of the system. */
	float cog[3];

	/* The standard gravitational parameter of the system center. This is the
	 * product of the gravitational constant (G) and the mass (M) of the
	 * body at the center of gravity.
	 *
	 *      μ = GM
	 */
	float u;

	/* The radius of the system. */
	struct fuzzy_float radius;

	/* The inclination of particle orbits, as an angle in radians relative
	 * to the equatorial (reference) plane. */
	struct fuzzy_float inclination;

	/* The number of particles in the system. */
	int particle_count;

	/* The size (in pixels) of particles. Each particle is represented by a
	 * rectangular point of dimensions particle_size × particle_size. */
	float particle_size;

	/* Particle color. */
	struct fuzzy_color particle_color;

	/* <priv> */
	struct particle_system_priv *priv;
};

struct particle_system *particle_system_new(cg_device_t *dev, cg_framebuffer_t *fb);

void particle_system_free(struct particle_system *system);

void particle_system_paint(struct particle_system *system);

#endif /* _PARTICLE_SYSTEM_H_ */
