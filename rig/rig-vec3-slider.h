#ifndef _RIG_VEC3_SLIDER_H_
#define _RIG_VEC3_SLIDER_H_

#include <cogl/cogl.h>

#include "rig.h"

extern RigType rig_vec3_slider_type;

typedef struct _RigVec3Slider RigVec3Slider;

RigVec3Slider *
rig_vec3_slider_new (RigContext *ctx);

void
rig_vec3_slider_set_name (RigVec3Slider *slider,
                          const char *name);

void
rig_vec3_slider_set_min_value (RigVec3Slider *slider,
                               float min_value);

void
rig_vec3_slider_set_max_value (RigVec3Slider *slider,
                               float max_value);

void
rig_vec3_slider_set_value (RigVec3Slider *slider,
                           const float *value);

void
rig_vec3_slider_set_step (RigVec3Slider *slider,
                          float step);

void
rig_vec3_slider_set_size (RigVec3Slider *slider,
                          int width,
                          int height);

void
rig_vec3_slider_set_decimal_places (RigVec3Slider *slider,
                                    int decimal_places);

int
rig_vec3_slider_get_decimal_places (RigVec3Slider *slider);

void
rig_vec3_slider_get_preferred_width (RigVec3Slider *slider,
                                     float for_height,
                                     float *min_width_p,
                                     float *natural_width_p);

void
rig_vec3_slider_get_preferred_height (RigVec3Slider *slider,
                                      float for_width,
                                      float *min_height_p,
                                      float *natural_height_p);

#endif /* _RIG_VEC3_SLIDER_H_ */
