#ifndef _RIG_DROP_DOWN_H_
#define _RIG_DROP_DOWN_H_

#include <cogl/cogl.h>

#include "rig.h"

extern RigType rig_drop_down_type;

typedef struct _RigDropDown RigDropDown;

#define RIG_DROP_DOWN(x) ((RigDropDown *) x)

typedef struct
{
  const char *name;
  int value;
} RigDropDownValue;

RigDropDown *
rig_drop_down_new (RigContext *ctx);

void
rig_drop_down_set_value (RigDropDown *slider,
                         int value);

int
rig_drop_down_get_value (RigDropDown *slider);

void
rig_drop_down_set_values (RigDropDown *drop,
                          ...) G_GNUC_NULL_TERMINATED;

void
rig_drop_down_set_values_array (RigDropDown *drop,
                                const RigDropDownValue *values,
                                int n_values);

#endif /* _RIG_DROP_DOWN_H_ */
