
#ifndef _RIG_BEVEL_H_
#define _RIG_BEVEL_H_

#include "rig.h"

extern RigType rig_bevel_type;
typedef struct _RigBevel RigBevel;
#define RIG_BEVEL(x) ((RigBevel *) x)

RigBevel *
rig_bevel_new (RigContext *context,
               float width,
               float height,
               const RigColor *reference);

void
rig_bevel_set_size (RigBevel *bevel,
                    float width,
                    float height);

void
rig_bevel_set_width (RigBevel *bevel,
                     float width);

void
rig_bevel_set_height (RigBevel *bevel,
                      float height);

void
rig_bevel_get_size (RigBevel *bevel,
                    float *width,
                    float *height);

#endif /* _RIG_BEVEL_H_ */
