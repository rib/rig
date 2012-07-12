#ifndef _RIG_NUMBER_SLIDER_H_
#define _RIG_NUMBER_SLIDER_H_

#include <cogl/cogl.h>

#include "rig.h"

extern RigType rig_number_slider_type;

typedef struct _RigNumberSlider RigNumberSlider;

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

void
rig_number_slider_set_step (RigNumberSlider *slider,
                            float step);

void
rig_number_slider_set_size (RigNumberSlider *slider,
                            int width,
                            int height);

void
rig_number_slider_set_decimal_places (RigNumberSlider *slider,
                                      int decimal_places);

int
rig_number_slider_get_decimal_places (RigNumberSlider *slider);

void
rig_number_slider_get_preferred_width (RigNumberSlider *slider,
                                       float for_height,
                                       float *min_width_p,
                                       float *natural_width_p);

void
rig_number_slider_get_preferred_height (RigNumberSlider *slider,
                                        float for_width,
                                        float *min_height_p,
                                        float *natural_height_p);

#endif /* _RIG_NUMBER_SLIDER_H_ */
