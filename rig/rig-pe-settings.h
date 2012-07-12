#ifndef _RIG_PE_SETTINGS_H_
#define _RIG_PE_SETTINGS_H_

#include <cogl/cogl.h>

#include "rig.h"
#include "rig-particle-engine.h"

extern RigType rig_pe_settings_type;

typedef struct _RigPeSettings RigPeSettings;

RigPeSettings *
rig_pe_settings_new (RigContext *ctx,
                     RigParticleEngine *engine);

void
rig_pe_settings_set_size (RigPeSettings *settings,
                          int width,
                          int height);

void
rig_pe_settings_get_preferred_width (RigPeSettings *settings,
                                     float for_height,
                                     float *min_width_p,
                                     float *natural_width_p);

void
rig_pe_settings_get_preferred_height (RigPeSettings *settings,
                                      float for_width,
                                      float *min_height_p,
                                      float *natural_height_p);

#endif /* _RIG_PE_SETTINGS_H_ */
