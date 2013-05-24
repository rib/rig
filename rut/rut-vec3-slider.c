/*
 * Rut
 *
 * Copyright (C) 2012 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

#include <config.h>

#include <cogl/cogl.h>

#include "rut.h"
#include "rut-number-slider.h"
#include "rut-vec3-slider.h"
#include "rut-text.h"

/* The padding between the border and the controls */
#define RUT_VEC3_SLIDER_BORDER_GAP 2
/* The padding between controls */
#define RUT_VEC3_SLIDER_CONTROL_GAP 5
/* Thickness of the border path */
#define RUT_VEC3_SLIDER_BORDER_THICKNESS 1

enum {
  RUT_VEC3_SLIDER_PROP_VALUE,
  RUT_VEC3_SLIDER_N_PROPS
};

typedef struct
{
  RutNumberSlider *slider;
  RutTransform *transform;
  RutProperty *property;
} RutVec3SliderControl;

struct _RutVec3Slider
{
  RutObjectProps _parent;

  RutContext *context;

  RutGraphableProps graphable;
  RutPaintableProps paintable;

  CoglPipeline *border_pipeline;

  int width, height;

  int ref_count;

  RutText *label;
  RutTransform *label_transform;
  RutVec3SliderControl controls[3];

  bool in_set_value;
  float value[3];

  RutSimpleIntrospectableProps introspectable;
  RutProperty properties[RUT_VEC3_SLIDER_N_PROPS];
};

RutType rut_vec3_slider_type;

static RutPropertySpec
_rut_vec3_slider_prop_specs[] =
  {
    {
      .name = "value",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_VEC3,
      .data_offset = offsetof (RutVec3Slider, value),
      .setter.vec3_type = rut_vec3_slider_set_value,
    },
    { 0 } /* XXX: Needed for runtime counting of the number of properties */
  };

static void
_rut_vec3_slider_free (void *object)
{
  RutVec3Slider *slider = object;
  int i;

  rut_graphable_remove_child (slider->label);
  rut_refable_unref (slider->label);
  rut_graphable_remove_child (slider->label_transform);
  rut_refable_unref (slider->label_transform);

  for (i = 0; i < 3; i++)
    {
      RutVec3SliderControl *control = slider->controls + i;

      rut_graphable_remove_child (control->slider);
      rut_refable_unref (control->slider);

      rut_graphable_remove_child (control->transform);
      rut_refable_unref (control->transform);
    }

  cogl_object_unref (slider->border_pipeline);

  rut_refable_unref (slider->context);

  rut_simple_introspectable_destroy (slider);
  rut_graphable_destroy (slider);

  g_slice_free (RutVec3Slider, slider);
}

RutRefCountableVTable _rut_vec3_slider_ref_countable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rut_vec3_slider_free
};

static void
_rut_vec3_slider_paint (RutObject *object,
                        RutPaintContext *paint_ctx)
{
  RutVec3Slider *slider = (RutVec3Slider *) object;
  RutCamera *camera = paint_ctx->camera;
  CoglFramebuffer *fb = rut_camera_get_framebuffer (camera);
  float rectangles[16] =
    {
      /* Top rectangle */
      RUT_VEC3_SLIDER_BORDER_THICKNESS,
      0.0f,
      slider->width - RUT_VEC3_SLIDER_BORDER_THICKNESS,
      RUT_VEC3_SLIDER_BORDER_THICKNESS,

      /* Bottom rectangle */
      RUT_VEC3_SLIDER_BORDER_THICKNESS,
      slider->height - RUT_VEC3_SLIDER_BORDER_THICKNESS,
      slider->width - RUT_VEC3_SLIDER_BORDER_THICKNESS,
      slider->height,

      /* Left rectangle */
      0.0f,
      0.0f,
      RUT_VEC3_SLIDER_BORDER_THICKNESS,
      slider->height,

      /* Rutht rectangle */
      slider->width - RUT_VEC3_SLIDER_BORDER_THICKNESS,
      0.0f,
      slider->width,
      slider->height
    };

  cogl_framebuffer_draw_rectangles (fb,
                                    slider->border_pipeline,
                                    rectangles,
                                    4 /* n_rects */);
}

static void
rut_vec3_slider_set_size (RutObject *object,
                          float width,
                          float height)
{
  RutVec3Slider *slider = RUT_VEC3_SLIDER (object);
  float control_width, control_height, y_pos;
  int i;

  rut_shell_queue_redraw (slider->context->shell);
  slider->width = width;
  slider->height = height;

  control_width = slider->width - (RUT_VEC3_SLIDER_BORDER_THICKNESS +
                                   RUT_VEC3_SLIDER_BORDER_GAP) * 2;
  rut_sizable_get_preferred_height (slider->label,
                                    control_width,
                                    NULL, /* min_width */
                                    &control_height /* natural_height */);
  rut_sizable_set_size (slider->label, control_width, control_height);

  rut_transform_init_identity (slider->label_transform);
  rut_transform_translate (slider->label_transform,
                           RUT_VEC3_SLIDER_BORDER_THICKNESS +
                           RUT_VEC3_SLIDER_BORDER_GAP,
                           RUT_VEC3_SLIDER_BORDER_THICKNESS +
                           RUT_VEC3_SLIDER_BORDER_GAP,
                           0.0f);

  y_pos = (RUT_VEC3_SLIDER_BORDER_THICKNESS +
           RUT_VEC3_SLIDER_BORDER_GAP +
           RUT_VEC3_SLIDER_CONTROL_GAP +
           control_height);

  for (i = 0; i < 3; i++)
    {
      RutVec3SliderControl *control = slider->controls + i;

      rut_transform_init_identity (control->transform);
      rut_transform_translate (control->transform,
                               RUT_VEC3_SLIDER_BORDER_THICKNESS +
                               RUT_VEC3_SLIDER_BORDER_GAP,
                               y_pos,
                               0.0f);

      rut_sizable_get_preferred_height (control->slider,
                                        control_width,
                                        NULL, /* min_width */
                                        &control_height);
      rut_sizable_set_size (control->slider,
                            control_width,
                            control_height);

      y_pos += control_height + RUT_VEC3_SLIDER_CONTROL_GAP;
    }
}

static void
rut_vec3_slider_get_size (RutObject *object,
                          float *width,
                          float *height)
{
  RutVec3Slider *slider = RUT_VEC3_SLIDER (object);

  *width = slider->width;
  *height = slider->height;
}

static void
rut_vec3_slider_get_preferred_width (RutObject *object,
                                     float for_height,
                                     float *min_width_p,
                                     float *natural_width_p)
{
  RutVec3Slider *slider = RUT_VEC3_SLIDER (object);
  float width, max_width = -G_MAXFLOAT;
  int i;

  rut_sizable_get_preferred_width (slider->label, -1, NULL, &width);
  if (max_width < width)
    max_width = width;

  for (i = 0; i < 3; i++)
    {
      RutVec3SliderControl *control = slider->controls + i;

      rut_sizable_get_preferred_width (control->slider, -1, NULL, &width);
      if (max_width < width)
        max_width = width;
    }

  width = max_width + (RUT_VEC3_SLIDER_BORDER_THICKNESS +
                       RUT_VEC3_SLIDER_BORDER_GAP) * 2;

  if (min_width_p)
    *min_width_p = width;
  if (natural_width_p)
    *natural_width_p = width;
}

static void
rut_vec3_slider_get_preferred_height (RutObject *object,
                                      float for_width,
                                      float *min_height_p,
                                      float *natural_height_p)
{
  RutVec3Slider *slider = RUT_VEC3_SLIDER (object);
  float total_height = 0.0f, height;
  int i;

  rut_sizable_get_preferred_height (slider->label, -1, NULL, &height);
  total_height += height;

  for (i = 0; i < 3; i++)
    {
      RutVec3SliderControl *control = slider->controls + i;

      rut_sizable_get_preferred_height (control->slider,
                                        -1, /* for_width */
                                        NULL, /* min_height */
                                        &height);
      total_height += height;
    }

  height = (total_height +
            (RUT_VEC3_SLIDER_BORDER_THICKNESS +
             RUT_VEC3_SLIDER_BORDER_GAP) * 2 +
            RUT_VEC3_SLIDER_CONTROL_GAP * 3);

  if (min_height_p)
    *min_height_p = height;
  if (natural_height_p)
    *natural_height_p = height;
}

static RutGraphableVTable _rut_vec3_slider_graphable_vtable = {
  NULL, /* child removed */
  NULL, /* child addded */
  NULL /* parent changed */
};

static RutPaintableVTable _rut_vec3_slider_paintable_vtable = {
  _rut_vec3_slider_paint
};

static RutIntrospectableVTable _rut_vec3_slider_introspectable_vtable = {
  rut_simple_introspectable_lookup_property,
  rut_simple_introspectable_foreach_property
};

static RutSizableVTable _rut_vec3_slider_sizable_vtable = {
  rut_vec3_slider_set_size,
  rut_vec3_slider_get_size,
  rut_vec3_slider_get_preferred_width,
  rut_vec3_slider_get_preferred_height,
  NULL /* add_preferred_size_callback */
};

static void
_rut_vec3_slider_init_type (void)
{
  rut_type_init (&rut_vec3_slider_type, "RigVec3_slider");
  rut_type_add_interface (&rut_vec3_slider_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RutVec3Slider, ref_count),
                          &_rut_vec3_slider_ref_countable_vtable);
  rut_type_add_interface (&rut_vec3_slider_type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (RutVec3Slider, graphable),
                          &_rut_vec3_slider_graphable_vtable);
  rut_type_add_interface (&rut_vec3_slider_type,
                          RUT_INTERFACE_ID_PAINTABLE,
                          offsetof (RutVec3Slider, paintable),
                          &_rut_vec3_slider_paintable_vtable);
  rut_type_add_interface (&rut_vec3_slider_type,
                          RUT_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_rut_vec3_slider_introspectable_vtable);
  rut_type_add_interface (&rut_vec3_slider_type,
                          RUT_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RutVec3Slider, introspectable),
                          NULL); /* no implied vtable */
  rut_type_add_interface (&rut_vec3_slider_type,
                          RUT_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &_rut_vec3_slider_sizable_vtable);
}

static void
rut_vec3_slider_property_changed_cb (RutProperty *target_property,
                                     void *user_data)
{
  RutVec3Slider *slider = user_data;
  float value[3];
  int i;

  if (slider->in_set_value)
    return;

  for (i = 0; i < 3; i++)
    {
      RutVec3SliderControl *control = slider->controls + i;
      value[i] = rut_number_slider_get_value (control->slider);
    }

  rut_vec3_slider_set_value (slider, value);
}

RutVec3Slider *
rut_vec3_slider_new (RutContext *context)
{
  RutVec3Slider *slider = g_slice_new0 (RutVec3Slider);
  static CoglBool initialized = FALSE;
  int i;

  if (initialized == FALSE)
    {
      _rut_init ();
      _rut_vec3_slider_init_type ();

      initialized = TRUE;
    }

  slider->ref_count = 1;
  slider->context = rut_refable_ref (context);

  rut_object_init (&slider->_parent, &rut_vec3_slider_type);

  rut_paintable_init (RUT_OBJECT (slider));
  rut_graphable_init (RUT_OBJECT (slider));

  rut_simple_introspectable_init (slider,
                                  _rut_vec3_slider_prop_specs,
                                  slider->properties);

  slider->border_pipeline = cogl_pipeline_new (context->cogl_context);
  cogl_pipeline_set_color4ub (slider->border_pipeline, 0, 0, 0, 255);

  slider->label_transform = rut_transform_new (context);
  rut_graphable_add_child (slider, slider->label_transform);
  slider->label = rut_text_new (context);
  rut_text_set_font_name (slider->label, "Sans 15px");
  rut_graphable_add_child (slider->label_transform, slider->label);

  for (i = 0; i < 3; i++)
    {
      RutVec3SliderControl *control = slider->controls + i;
      char label[] = "X";

      control->transform = rut_transform_new (context);
      rut_graphable_add_child (slider, control->transform);
      control->slider = rut_number_slider_new (context);
      rut_graphable_add_child (control->transform, control->slider);

      label[0] += i;
      rut_number_slider_set_name (control->slider, label);

      control->property =
        rut_introspectable_lookup_property (control->slider, "value");
    }

  rut_property_set_binding (&slider->properties[RUT_VEC3_SLIDER_PROP_VALUE],
                            rut_vec3_slider_property_changed_cb,
                            slider,
                            slider->controls[0].property,
                            slider->controls[1].property,
                            slider->controls[2].property,
                            NULL);

  rut_vec3_slider_set_size (slider, 60, 30);

  return slider;
}

void
rut_vec3_slider_set_name (RutVec3Slider *slider,
                          const char *name)
{
  rut_text_set_text (slider->label, name);
}

void
rut_vec3_slider_set_min_value (RutVec3Slider *slider,
                               float min_value)
{
  int i;

  for (i = 0; i < 3; i++)
    {
      RutVec3SliderControl *control = slider->controls + i;

      rut_number_slider_set_min_value (control->slider, min_value);
    }
}

void
rut_vec3_slider_set_max_value (RutVec3Slider *slider,
                               float max_value)
{
  int i;

  for (i = 0; i < 3; i++)
    {
      RutVec3SliderControl *control = slider->controls + i;

      rut_number_slider_set_max_value (control->slider, max_value);
    }
}

void
rut_vec3_slider_set_value (RutObject *obj,
                           const float *value)
{
  RutVec3Slider *slider = RUT_VEC3_SLIDER (obj);
  int i;

  memcpy (slider->value, value, sizeof (float) * 3);

  /* Normally we update slider->value[] based on notifications from
   * the per-component slider controls, but since we are manually
   * updating the controls here we need to temporarily ignore
   * the notifications so we avoid any recursion
   *
   * Note: If we change property notifications be deferred to the
   * mainloop then this mechanism will become redundant
   */
  slider->in_set_value = TRUE;
  for (i = 0; i < 3; i++)
    {
      RutVec3SliderControl *control = slider->controls + i;
      rut_number_slider_set_value (control->slider, value[i]);
    }
  slider->in_set_value = FALSE;

  rut_property_dirty (&slider->context->property_ctx,
                      &slider->properties[RUT_VEC3_SLIDER_PROP_VALUE]);
}

void
rut_vec3_slider_set_step (RutVec3Slider *slider,
                          float step)
{
  int i;

  for (i = 0; i < 3; i++)
    {
      RutVec3SliderControl *control = slider->controls + i;

      rut_number_slider_set_step (control->slider, step);
    }
}

int
rut_vec3_slider_get_decimal_places (RutVec3Slider *slider)
{
  return rut_number_slider_get_decimal_places (slider->controls[0].slider);
}

void
rut_vec3_slider_set_decimal_places (RutVec3Slider *slider,
                                    int decimal_places)
{
  int i;

  for (i = 0; i < 3; i++)
    {
      RutVec3SliderControl *control = slider->controls + i;

      rut_number_slider_set_decimal_places (control->slider, decimal_places);
    }
}
