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

#ifndef _PARTICLE_ENGINE_H_
#define _PARTICLE_ENGINE_H_

#include <cglib/cglib.h>

/*
 * The particle engine is an opaque data structure
 */
struct particle_engine;

/*
 * Create and return a new particle engine.
 */
struct particle_engine *particle_engine_new(cg_device_t *dev,
					    cg_framebuffer_t *fb,
					    int particle_count,
					    float particle_size);

/*
 * Destroy a particle engine and free associated resources.
 */
void particle_engine_free(struct particle_engine *engine);

/*
 * This maps the particle's vertices buffer according to the given access
 * flags.
 */
void particle_engine_push_buffer(struct particle_engine *engine,
                                 cg_buffer_access_t access,
                                 cg_buffer_map_hint_t hints);

/*
 * This unmaps the internal attribute buffer, writing out any changes made to
 * the particle vertices.
 */
void particle_engine_pop_buffer(struct particle_engine *engine);

/*
 * Returns a pointer to the given particle's position as an array of floats [x, y, z].
 */
float *particle_engine_get_particle_position(struct particle_engine *engine, int index);

/*
 * Returns a pointer to the given particle's color.
 */
cg_color_t *particle_engine_get_particle_color(struct particle_engine *engine, int index);

/*
 * Paint function.
 */
void particle_engine_paint(struct particle_engine *engine);

#endif /* _PARTICLE_ENGINE_H */
