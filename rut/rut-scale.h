/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013  Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef __RUT_SCALE_H__
#define __RUT_SCALE_H__

#include <stdbool.h>

#include <rut-context.h>
#include "rut-paintable.h"
#include "rut-rectangle.h"

extern RutType rut_scale_type;

enum {
  RUT_SCALE_PROP_LENGTH,
  RUT_SCALE_PROP_USER_SCALE,
  RUT_SCALE_PROP_OFFSET,
  RUT_SCALE_PROP_FOCUS,
  RUT_SCALE_PROP_PIXEL_SCALE,
  RUT_SCALE_N_PROPS
};

typedef struct _RutScale
{
  RutObjectBase _base;

  RutContext *ctx;

  float width;
  float height;

  RutGraphableProps graphable;
  RutPaintableProps paintable;

  RutList preferred_size_cb_list;

  float natural_length;
  float length;
  float default_scale;
  float user_scale;
  float pixel_scale;

  float start_offset;
  float focus_offset;

  float current_range;
  float current_first_label;

  cg_pipeline_t *pipeline;
  RutRectangle *bg;

  RutTransform *select_transform;
  RutRectangle *select_rect;

  RutInputRegion *input_region;

  c_array_t *labels;
  int n_visible_labels;

  bool initial_view;
  bool changed;

  RutList select_cb_list;

  RutIntrospectableProps introspectable;
  RutProperty properties[RUT_SCALE_N_PROPS];

} RutScale;

RutScale *
rut_scale_new (RutContext *ctx,
               float length, /* actual length */
               float natural_length); /* initial visual length */

/* The visual length used when the scale is first created, before
 * any user interaction to scale or pan the view
 */
void
rut_scale_set_natural_length (RutScale *scale,
                              float natural_length);

void
rut_scale_set_length (RutObject *object, float length);

float
rut_scale_get_length (RutScale *scale);

void
rut_scale_set_offset (RutObject *object, float offset);

float
rut_scale_get_offset (RutScale *scale);

void
rut_scale_set_focus (RutObject *object, float offset);

float
rut_scale_get_focus (RutScale *scale);

float
rut_scale_get_pixel_scale (RutScale *scale);

float
rut_scale_pixel_to_offset (RutScale *scale, float pixel);

typedef void (*RutScaleSelectCallback) (RutScale *scale,
                                        float start_t,
                                        float end_t,
                                        void *user_data);

RutClosure *
rut_scale_add_select_callback (RutScale *scale,
                               RutScaleSelectCallback callback,
                               void *user_data,
                               RutClosureDestroyCallback destroy_cb);

void
rut_scale_user_zoom_in (RutScale *scale);

void
rut_scale_user_zoom_out (RutScale *scale);

void
rut_scale_user_zoom_reset (RutScale *scale);

#endif /* __RUT_SCALE_H__ */
