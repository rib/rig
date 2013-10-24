#ifndef _RUT_VEC3_SLIDER_H_
#define _RUT_VEC3_SLIDER_H_

#include "rut-object.h"
#include "rut-context.h"

extern RutType rut_vec3_slider_type;
typedef struct _RutVec3Slider RutVec3Slider;

RutVec3Slider *
rut_vec3_slider_new (RutContext *ctx);

void
rut_vec3_slider_set_min_value (RutVec3Slider *slider,
                               float min_value);

void
rut_vec3_slider_set_max_value (RutVec3Slider *slider,
                               float max_value);

void
rut_vec3_slider_set_value (RutObject *slider,
                           const float *value);

void
rut_vec3_slider_set_step (RutVec3Slider *slider,
                          float step);

void
rut_vec3_slider_set_decimal_places (RutVec3Slider *slider,
                                    int decimal_places);

int
rut_vec3_slider_get_decimal_places (RutVec3Slider *slider);

#endif /* _RUT_VEC3_SLIDER_H_ */
