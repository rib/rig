#ifndef _RUT_ROTATION_INSPECTOR_H_
#define _RUT_ROTATION_INSPECTOR_H_

#include <cogl/cogl.h>

#include "rut-object.h"
#include "rut-context.h"

extern RutType rut_rotation_inspector_type;
typedef struct _RutRotationInspector RutRotationInspector;


RutRotationInspector *
rut_rotation_inspector_new (RutContext *ctx);

void
rut_rotation_inspector_set_value (RutObject *slider,
                                  const CoglQuaternion *value);

void
rut_rotation_inspector_set_step (RutRotationInspector *slider,
                                 float step);

void
rut_rotation_inspector_set_decimal_places (RutRotationInspector *slider,
                                           int decimal_places);

int
rut_rotation_inspector_get_decimal_places (RutRotationInspector *slider);

#endif /* _RUT_ROTATION_INSPECTOR_H_ */
