
#ifndef _RIG_STACK_H_
#define _RIG_STACK_H_

#include "rig.h"

extern RigType rig_stack_type;
typedef struct _RigStack RigStack;
#define RIG_STACK(x) ((RigStack *) x)

RigStack *
rig_stack_new (RigContext *context,
               float width,
               float height,
               ...);

void
rig_stack_set_size (RigStack *stack,
                    float width,
                    float height);

void
rig_stack_set_width (RigStack *stack,
                     float width);

void
rig_stack_set_height (RigStack *stack,
                      float height);

void
rig_stack_get_size (RigStack *stack,
                    float *width,
                    float *height);

void
rig_stack_append_child (RigStack *stack,
                        RigObject *child);

#endif /* _RIG_STACK_H_ */
