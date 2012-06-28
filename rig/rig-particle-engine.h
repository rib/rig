#ifndef _RIG_PARTICLE_ENGINE_H_
#define _RIG_PARTICLE_ENGINE_H_

#include <cogl/cogl.h>

#include "rig.h"

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

#endif /* _RIG_PARTICLE_ENGINE_H_ */
