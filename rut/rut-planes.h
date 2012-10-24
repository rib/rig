
#ifndef _RUT_PLANES_H_
#define _RUT_PLANES_H_

#include "rut-types.h"

typedef struct _RutPlane
{
  float v0[3];
  float n[3];
} RutPlane;

void
rut_get_eye_planes_for_screen_poly (float *polygon,
                                    int n_vertices,
                                    float *viewport,
                                    const CoglMatrix *projection,
                                    const CoglMatrix *inverse_project,
                                    RutPlane *planes);

#endif /* _RUT_PLANES_H_ */
