#ifndef _RUT_NUMBER_SLIDER_H_
#define _RUT_NUMBER_SLIDER_H_

#include <cogl/cogl.h>

#include "rut.h"

extern RutType rut_number_slider_type;

typedef struct _RutNumberSlider RutNumberSlider;

#define RUT_NUMBER_SLIDER(x) ((RutNumberSlider *) x)

RutNumberSlider *
rut_number_slider_new (RutContext *ctx);

void
rut_number_slider_set_name (RutNumberSlider *slider,
                            const char *name);

void
rut_number_slider_set_min_value (RutNumberSlider *slider,
                                 float min_value);

void
rut_number_slider_set_max_value (RutNumberSlider *slider,
                                 float max_value);

void
rut_number_slider_set_value (RutObject *slider,
                             float value);

float
rut_number_slider_get_value (RutNumberSlider *slider);

void
rut_number_slider_set_step (RutNumberSlider *slider,
                            float step);

void
rut_number_slider_set_decimal_places (RutNumberSlider *slider,
                                      int decimal_places);

int
rut_number_slider_get_decimal_places (RutNumberSlider *slider);

#endif /* _RUT_NUMBER_SLIDER_H_ */
