
#ifndef _RUT_STACK_H_
#define _RUT_STACK_H_

#include "rut.h"

extern RutType rut_stack_type;
typedef struct _RutStack RutStack;
#define RUT_STACK(x) ((RutStack *) x)

RutStack *
rut_stack_new (RutContext *context,
               float width,
               float height);

void
rut_stack_add (RutStack *stack,
               RutObject *child);

void
rut_stack_set_size (RutStack *stack,
                    float width,
                    float height);

void
rut_stack_set_width (RutObject *stack,
                     float width);

void
rut_stack_set_height (RutObject *stack,
                      float height);

void
rut_stack_get_size (RutStack *stack,
                    float *width,
                    float *height);

#endif /* _RUT_STACK_H_ */
