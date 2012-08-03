#ifndef _RIG_PARTICLE_ENGINE_H_
#define _RIG_PARTICLE_ENGINE_H_

#include <cogl/cogl.h>

#include "rig.h"
#include "rig-ease.h"

extern RigType rig_particle_engine_type;

typedef struct _RigParticleEngine RigParticleEngine;

RigParticleEngine *
rig_particle_engine_new (RigContext *ctx);

/**
 * rig_particle_engine_set_time:
 * @engine: A pointer to the particle engine
 * @msecs: The time in milliseconds
 *
 * Sets the relative time in milliseconds at which to draw the
 * animation for this particle engine. The time does need to be
 * related to wall time and it doesn't need to start from zero. This
 * should be called before every paint to update the animation.
 *
 * Adjusting the time backwards will give undefined results.
 */
void
rig_particle_engine_set_time (RigParticleEngine *engine,
                              int32_t msecs);

/**
 * rig_particle_engine_add_color:
 * @engine: A pointer to the particle engine
 * @color: A color to add as 4 bytes representing RGBA
 *
 * Adds a color to the selection of colors that will be initially
 * chosen for a particle. The colors will be selected randomly and
 * distributed evenly for each new particle.
 */
void
rig_particle_engine_add_color (RigParticleEngine *engine,
                               const uint8_t color[4]);

/**
 * rig_particle_engine_remove_color:
 * @engine: A pointer to the particle engine
 * @color: A color to remove as 4 bytes representing RGBA
 *
 * Removes a color from the selection of colors that will be initially
 * chosen for a particle.
 */
void
rig_particle_engine_remove_color (RigParticleEngine *engine,
                                  const uint8_t color[4]);

void
rig_particle_engine_set_texture (RigParticleEngine *engine,
                                 CoglTexture *texture);

void
rig_particle_engine_set_max_particles (RigParticleEngine *engine,
                                       int max_particles);

void
rig_particle_engine_set_min_initial_velocity (RigParticleEngine *engine,
                                              const float *value);

void
rig_particle_engine_set_max_initial_velocity (RigParticleEngine *engine,
                                              const float *value);

void
rig_particle_engine_set_min_initial_position (RigParticleEngine *engine,
                                              const float *value);

void
rig_particle_engine_set_max_initial_position (RigParticleEngine *engine,
                                              const float *value);

#endif /* _RIG_PARTICLE_ENGINE_H_ */
