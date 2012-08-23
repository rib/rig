
#ifndef _RIG_ENTRY_H_
#define _RIG_ENTRY_H_

#include "rig.h"

extern RigType rig_entry_type;
typedef struct _RigEntry RigEntry;
#define RIG_ENTRY(x) ((RigEntry *) x)

RigEntry *
rig_entry_new (RigContext *context);

void
rig_entry_set_size (RigEntry *entry,
                    float width,
                    float height);

void
rig_entry_set_width (RigEntry *entry,
                     float width);

void
rig_entry_set_height (RigEntry *entry,
                      float height);

void
rig_entry_get_size (RigEntry *entry,
                    float *width,
                    float *height);

RigText *
rig_entry_get_text (RigEntry *entry);

#endif /* _RIG_ENTRY_H_ */
