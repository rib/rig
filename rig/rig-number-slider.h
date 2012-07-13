#ifndef _RIG_NUMBER_SLIDER_H_
#define _RIG_NUMBER_SLIDER_H_

#include <cogl/cogl.h>

#include "rig.h"

extern RigType rig_number_slider_type;

typedef struct _RigNumberSlider RigNumberSlider;

#define RIG_NUMBER_SLIDER(x) ((RigNumberSlider *) x)

RigNumberSlider *
rig_number_slider_new (RigContext *ctx);

void
rig_number_slider_set_name (RigNumberSlider *slider,
                            const char *name);

void
rig_number_slider_set_min_value (RigNumberSlider *slider,
                                 float min_value);

void
rig_number_slider_set_max_value (RigNumberSlider *slider,
                                 float max_value);

void
rig_number_slider_set_value (RigNumberSlider *slider,
                             float value);

float
rig_number_slider_get_value (RigNumberSlider *slider);

void
rig_number_slider_set_step (RigNumberSlider *slider,
                            float step);

void
rig_number_slider_set_decimal_places (RigNumberSlider *slider,
                                      int decimal_places);

int
rig_number_slider_get_decimal_places (RigNumberSlider *slider);

#endif /* _RIG_NUMBER_SLIDER_H_ */
