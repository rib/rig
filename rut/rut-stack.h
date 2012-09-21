
#ifndef _RUT_STACK_H_
#define _RUT_STACK_H_

#include "rut.h"

extern RutType rut_stack_type;
typedef struct _RutStack RutStack;
#define RUT_STACK(x) ((RutStack *) x)

RutStack *
rut_stack_new (RutContext *context,
               float width,
               float height,
               ...);

void
rut_stack_set_size (RutStack *stack,
                    float width,
                    float height);

void
rut_stack_set_width (RutStack *stack,
                     float width);

void
rut_stack_set_height (RutStack *stack,
                      float height);

void
rut_stack_get_size (RutStack *stack,
                    float *width,
                    float *height);

void
rut_stack_append_child (RutStack *stack,
                        RutObject *child);

#endif /* _RUT_STACK_H_ */
