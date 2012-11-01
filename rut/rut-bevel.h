
#ifndef _RUT_BEVEL_H_
#define _RUT_BEVEL_H_

#include "rut.h"

extern RutType rut_bevel_type;
typedef struct _RutBevel RutBevel;
#define RUT_BEVEL(x) ((RutBevel *) x)

RutBevel *
rut_bevel_new (RutContext *context,
               float width,
               float height,
               const CoglColor *reference);

void
rut_bevel_set_size (RutObject *self,
                    float width,
                    float height);

void
rut_bevel_set_width (RutObject *bevel,
                     float width);

void
rut_bevel_set_height (RutObject *bevel,
                      float height);

void
rut_bevel_get_size (RutObject *self,
                    float *width,
                    float *height);

#endif /* _RUT_BEVEL_H_ */
