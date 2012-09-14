/*
 * Rig
 *
 * A tiny toolkit
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

#include <cogl/cogl.h>

#include "rig.h"
#include "rig-number-slider.h"
#include "rig-vec3-slider.h"
#include "rig-text.h"

/* The padding between the border and the controls */
#define RIG_VEC3_SLIDER_BORDER_GAP 2
/* The padding between controls */
#define RIG_VEC3_SLIDER_CONTROL_GAP 5
/* Thickness of the border path */
#define RIG_VEC3_SLIDER_BORDER_THICKNESS 1

enum {
  RIG_VEC3_SLIDER_PROP_VALUE,
  RIG_VEC3_SLIDER_N_PROPS
};

typedef struct
{
  RigNumberSlider *slider;
  RigTransform *transform;
  RigProperty *property;
} RigVec3SliderControl;

struct _RigVec3Slider
{
  RigObjectProps _parent;

  RigContext *context;

  RigGraphableProps graphable;
  RigPaintableProps paintable;

  CoglPipeline *border_pipeline;

  int width, height;

  int ref_count;

  RigText *label;
  RigTransform *label_transform;
  RigVec3SliderControl controls[3];

  float value[3];

  RigSimpleIntrospectableProps introspectable;
  RigProperty properties[RIG_VEC3_SLIDER_N_PROPS];
};

RigType rig_vec3_slider_type;

static RigPropertySpec
_rig_vec3_slider_prop_specs[] =
  {
    {
      .name = "value",
      .type = RIG_PROPERTY_TYPE_VEC3,
      .data_offset = offsetof (RigVec3Slider, value),
      .setter = rig_vec3_slider_set_value,
      .getter = NULL
    },
    { 0 } /* XXX: Needed for runtime counting of the number of properties */
  };

static void
_rig_vec3_slider_free (void *object)
{
  RigVec3Slider *slider = object;
  int i;

  rig_graphable_remove_child (slider->label);
  rig_ref_countable_unref (slider->label);
  rig_graphable_remove_child (slider->label_transform);
  rig_ref_countable_unref (slider->label_transform);

  for (i = 0; i < 3; i++)
    {
      RigVec3SliderControl *control = slider->controls + i;

      rig_graphable_remove_child (control->slider);
      rig_ref_countable_unref (control->slider);

      rig_graphable_remove_child (control->transform);
      rig_ref_countable_unref (control->transform);
    }

  cogl_object_unref (slider->border_pipeline);

  rig_ref_countable_unref (slider->context);

  rig_simple_introspectable_destroy (slider);

  g_slice_free (RigVec3Slider, slider);
}

RigRefCountableVTable _rig_vec3_slider_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_vec3_slider_free
};

static void
_rig_vec3_slider_paint (RigObject *object,
                        RigPaintContext *paint_ctx)
{
  RigVec3Slider *slider = (RigVec3Slider *) object;
  RigCamera *camera = paint_ctx->camera;
  CoglFramebuffer *fb = rig_camera_get_framebuffer (camera);
  float rectangles[16] =
    {
      /* Top rectangle */
      RIG_VEC3_SLIDER_BORDER_THICKNESS,
      0.0f,
      slider->width - RIG_VEC3_SLIDER_BORDER_THICKNESS,
      RIG_VEC3_SLIDER_BORDER_THICKNESS,

      /* Bottom rectangle */
      RIG_VEC3_SLIDER_BORDER_THICKNESS,
      slider->height - RIG_VEC3_SLIDER_BORDER_THICKNESS,
      slider->width - RIG_VEC3_SLIDER_BORDER_THICKNESS,
      slider->height,

      /* Left rectangle */
      0.0f,
      0.0f,
      RIG_VEC3_SLIDER_BORDER_THICKNESS,
      slider->height,

      /* Right rectangle */
      slider->width - RIG_VEC3_SLIDER_BORDER_THICKNESS,
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
rig_vec3_slider_set_size (RigObject *object,
                          float width,
                          float height)
{
  RigVec3Slider *slider = RIG_VEC3_SLIDER (object);
  float control_width, control_height, y_pos;
  int i;

  rig_shell_queue_redraw (slider->context->shell);
  slider->width = width;
  slider->height = height;

  control_width = slider->width - (RIG_VEC3_SLIDER_BORDER_THICKNESS +
                                   RIG_VEC3_SLIDER_BORDER_GAP) * 2;
  rig_sizable_get_preferred_height (slider->label,
                                    control_width,
                                    NULL, /* min_width */
                                    &control_height /* natural_height */);
  rig_sizable_set_size (slider->label, control_width, control_height);

  rig_transform_init_identity (slider->label_transform);
  rig_transform_translate (slider->label_transform,
                           RIG_VEC3_SLIDER_BORDER_THICKNESS +
                           RIG_VEC3_SLIDER_BORDER_GAP,
                           RIG_VEC3_SLIDER_BORDER_THICKNESS +
                           RIG_VEC3_SLIDER_BORDER_GAP,
                           0.0f);

  y_pos = (RIG_VEC3_SLIDER_BORDER_THICKNESS +
           RIG_VEC3_SLIDER_BORDER_GAP +
           RIG_VEC3_SLIDER_CONTROL_GAP +
           control_height);

  for (i = 0; i < 3; i++)
    {
      RigVec3SliderControl *control = slider->controls + i;

      rig_transform_init_identity (control->transform);
      rig_transform_translate (control->transform,
                               RIG_VEC3_SLIDER_BORDER_THICKNESS +
                               RIG_VEC3_SLIDER_BORDER_GAP,
                               y_pos,
                               0.0f);

      rig_sizable_get_preferred_height (control->slider,
                                        control_width,
                                        NULL, /* min_width */
                                        &control_height);
      rig_sizable_set_size (control->slider,
                            control_width,
                            control_height);

      y_pos += control_height + RIG_VEC3_SLIDER_CONTROL_GAP;
    }
}

static void
rig_vec3_slider_get_size (RigObject *object,
                          float *width,
                          float *height)
{
  RigVec3Slider *slider = RIG_VEC3_SLIDER (object);

  *width = slider->width;
  *height = slider->height;
}

static void
rig_vec3_slider_get_preferred_width (RigObject *object,
                                     float for_height,
                                     float *min_width_p,
                                     float *natural_width_p)
{
  RigVec3Slider *slider = RIG_VEC3_SLIDER (object);
  float width, max_width = -G_MAXFLOAT;
  int i;

  rig_sizable_get_preferred_width (slider->label, -1, NULL, &width);
  if (max_width < width)
    max_width = width;

  for (i = 0; i < 3; i++)
    {
      RigVec3SliderControl *control = slider->controls + i;

      rig_sizable_get_preferred_width (control->slider, -1, NULL, &width);
      if (max_width < width)
        max_width = width;
    }

  width = max_width + (RIG_VEC3_SLIDER_BORDER_THICKNESS +
                       RIG_VEC3_SLIDER_BORDER_GAP) * 2;

  if (min_width_p)
    *min_width_p = width;
  if (natural_width_p)
    *natural_width_p = width;
}

static void
rig_vec3_slider_get_preferred_height (RigObject *object,
                                      float for_width,
                                      float *min_height_p,
                                      float *natural_height_p)
{
  RigVec3Slider *slider = RIG_VEC3_SLIDER (object);
  float total_height = 0.0f, height;
  int i;

  rig_sizable_get_preferred_height (slider->label, -1, NULL, &height);
  total_height += height;

  for (i = 0; i < 3; i++)
    {
      RigVec3SliderControl *control = slider->controls + i;

      rig_sizable_get_preferred_height (control->slider,
                                        -1, /* for_width */
                                        NULL, /* min_height */
                                        &height);
      total_height += height;
    }

  height = (total_height +
            (RIG_VEC3_SLIDER_BORDER_THICKNESS +
             RIG_VEC3_SLIDER_BORDER_GAP) * 2 +
            RIG_VEC3_SLIDER_CONTROL_GAP * 3);

  if (min_height_p)
    *min_height_p = height;
  if (natural_height_p)
    *natural_height_p = height;
}

static RigGraphableVTable _rig_vec3_slider_graphable_vtable = {
  NULL, /* child removed */
  NULL, /* child addded */
  NULL /* parent changed */
};

static RigPaintableVTable _rig_vec3_slider_paintable_vtable = {
  _rig_vec3_slider_paint
};

static RigIntrospectableVTable _rig_vec3_slider_introspectable_vtable = {
  rig_simple_introspectable_lookup_property,
  rig_simple_introspectable_foreach_property
};

static RigSizableVTable _rig_vec3_slider_sizable_vtable = {
  rig_vec3_slider_set_size,
  rig_vec3_slider_get_size,
  rig_vec3_slider_get_preferred_width,
  rig_vec3_slider_get_preferred_height
};

static void
_rig_vec3_slider_init_type (void)
{
  rig_type_init (&rig_vec3_slider_type);
  rig_type_add_interface (&rig_vec3_slider_type,
                          RIG_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigVec3Slider, ref_count),
                          &_rig_vec3_slider_ref_countable_vtable);
  rig_type_add_interface (&rig_vec3_slider_type,
                          RIG_INTERFACE_ID_GRAPHABLE,
                          offsetof (RigVec3Slider, graphable),
                          &_rig_vec3_slider_graphable_vtable);
  rig_type_add_interface (&rig_vec3_slider_type,
                          RIG_INTERFACE_ID_PAINTABLE,
                          offsetof (RigVec3Slider, paintable),
                          &_rig_vec3_slider_paintable_vtable);
  rig_type_add_interface (&rig_vec3_slider_type,
                          RIG_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_rig_vec3_slider_introspectable_vtable);
  rig_type_add_interface (&rig_vec3_slider_type,
                          RIG_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RigVec3Slider, introspectable),
                          NULL); /* no implied vtable */
  rig_type_add_interface (&rig_vec3_slider_type,
                          RIG_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &_rig_vec3_slider_sizable_vtable);
}

static void
rig_vec3_slider_property_changed_cb (RigProperty *target_property,
                                     RigProperty *source_property,
                                     void *user_data)
{
  RigVec3Slider *slider = user_data;
  int i;

  for (i = 0; i < 3; i++)
    {
      RigVec3SliderControl *control = slider->controls + i;

      /* When rig_vec3_slider_set_value is called this callback will
       * be invoked three times, one for each of the child number
       * sliders that get modified. Therefore we only want to update
       * the value for the property that is actually being notified
       * because otherwise we will update the vec3 with values from
       * the number sliders that haven't been updated yet and we will
       * report an inconsistent value. */
      if (source_property == control->property)
        slider->value[i] = rig_number_slider_get_value (control->slider);
    }

  rig_property_dirty (&slider->context->property_ctx,
                      &slider->properties[RIG_VEC3_SLIDER_PROP_VALUE]);
}

RigVec3Slider *
rig_vec3_slider_new (RigContext *context)
{
  RigVec3Slider *slider = g_slice_new0 (RigVec3Slider);
  static CoglBool initialized = FALSE;
  int i;

  if (initialized == FALSE)
    {
      _rig_init ();
      _rig_vec3_slider_init_type ();

      initialized = TRUE;
    }

  slider->ref_count = 1;
  slider->context = rig_ref_countable_ref (context);

  rig_object_init (&slider->_parent, &rig_vec3_slider_type);

  rig_paintable_init (RIG_OBJECT (slider));
  rig_graphable_init (RIG_OBJECT (slider));

  rig_simple_introspectable_init (slider,
                                  _rig_vec3_slider_prop_specs,
                                  slider->properties);

  slider->border_pipeline = cogl_pipeline_new (context->cogl_context);
  cogl_pipeline_set_color4ub (slider->border_pipeline, 0, 0, 0, 255);

  slider->label_transform = rig_transform_new (context, NULL);
  rig_graphable_add_child (slider, slider->label_transform);
  slider->label = rig_text_new (context);
  rig_text_set_font_name (slider->label, "Sans 15px");
  rig_graphable_add_child (slider->label_transform, slider->label);

  for (i = 0; i < 3; i++)
    {
      RigVec3SliderControl *control = slider->controls + i;
      char label[] = "X";

      control->transform = rig_transform_new (context, NULL);
      rig_graphable_add_child (slider, control->transform);
      control->slider = rig_number_slider_new (context);
      rig_graphable_add_child (control->transform, control->slider);

      label[0] += i;
      rig_number_slider_set_name (control->slider, label);

      control->property =
        rig_introspectable_lookup_property (control->slider, "value");
    }

  rig_property_set_binding (&slider->properties[RIG_VEC3_SLIDER_PROP_VALUE],
                            rig_vec3_slider_property_changed_cb,
                            slider,
                            slider->controls[0].property,
                            slider->controls[1].property,
                            slider->controls[2].property,
                            NULL);

  rig_vec3_slider_set_size (slider, 60, 30);

  return slider;
}

void
rig_vec3_slider_set_name (RigVec3Slider *slider,
                          const char *name)
{
  rig_text_set_text (slider->label, name);
}

void
rig_vec3_slider_set_min_value (RigVec3Slider *slider,
                               float min_value)
{
  int i;

  for (i = 0; i < 3; i++)
    {
      RigVec3SliderControl *control = slider->controls + i;

      rig_number_slider_set_min_value (control->slider, min_value);
    }
}

void
rig_vec3_slider_set_max_value (RigVec3Slider *slider,
                               float max_value)
{
  int i;

  for (i = 0; i < 3; i++)
    {
      RigVec3SliderControl *control = slider->controls + i;

      rig_number_slider_set_max_value (control->slider, max_value);
    }
}

void
rig_vec3_slider_set_value (RigVec3Slider *slider,
                           const float *value)
{
  int i;

  /* This value will get updated anyway as the notifications for the
   * slider properties are emitted. However we want to copy in the
   * whole value immediately so that it won't notify on an
   * inconsistent state in response to the slider values changing */
  memcpy (slider->value, value, sizeof (float) * 3);

  for (i = 0; i < 3; i++)
    {
      RigVec3SliderControl *control = slider->controls + i;

      rig_number_slider_set_value (control->slider, value[i]);
    }
}

void
rig_vec3_slider_set_step (RigVec3Slider *slider,
                          float step)
{
  int i;

  for (i = 0; i < 3; i++)
    {
      RigVec3SliderControl *control = slider->controls + i;

      rig_number_slider_set_step (control->slider, step);
    }
}

int
rig_vec3_slider_get_decimal_places (RigVec3Slider *slider)
{
  return rig_number_slider_get_decimal_places (slider->controls[0].slider);
}

void
rig_vec3_slider_set_decimal_places (RigVec3Slider *slider,
                                    int decimal_places)
{
  int i;

  for (i = 0; i < 3; i++)
    {
      RigVec3SliderControl *control = slider->controls + i;

      rig_number_slider_set_decimal_places (control->slider, decimal_places);
    }
}
