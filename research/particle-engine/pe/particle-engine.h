#ifndef _PARTICLE_ENGINE_H_
#define _PARTICLE_ENGINE_H_

#include <cogl/cogl.h>

/*
 * The particle engine is an opaque data structure
 */
struct particle_engine;

/*
 * Create and return a new particle engine.
 */
struct particle_engine *particle_engine_new(CoglContext *ctx,
					    CoglFramebuffer *fb,
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
inline void particle_engine_push_buffer(struct particle_engine *engine,
					CoglBufferAccess access,
					CoglBufferMapHint hints);

/*
 * This unmaps the internal attribute buffer, writing out any changes made to
 * the particle vertices.
 */
inline void particle_engine_pop_buffer(struct particle_engine *engine);

/*
 * Returns a pointer to the given particle's position as an array of floats [x, y, z].
 */
inline float *particle_engine_get_particle_position(struct particle_engine *engine, int index);

/*
 * Returns a pointer to the given particle's color.
 */
inline CoglColor *particle_engine_get_particle_color(struct particle_engine *engine, int index);

/*
 * Paint function.
 */
void particle_engine_paint(struct particle_engine *engine);

#endif /* _PARTICLE_ENGINE_H */
